#ifdef TMAIN_KAUTIL_CACHE_MERGE_STATIC

#include <vector>
#include <stdint.h>
#include <numeric>
#include "kautil/algorithm/btree_search/btree_search.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

template<typename premitive>
struct file_syscall_premitive{
    using value_type = premitive;
    using offset_type = long;

    int fd=-1;
    char * buffer = 0;
    offset_type buffer_size = 0;
    struct stat st;
    
    ~file_syscall_premitive(){ free(buffer); }
    offset_type block_size(){ return sizeof(value_type); }
    offset_type size(){
        return lseek(fd,0,SEEK_END)- lseek(fd,0,SEEK_SET);
//        fstat(fd,&st);
//        return static_cast<offset_type>(st.st_size); 
    }
    
    
    void read_value(offset_type const& offset, value_type ** value){
        lseek(fd,offset,SEEK_SET);
        ::read(fd,*value,sizeof(value_type));
    }
    
    bool write(offset_type const& offset, void ** data, offset_type size){
        lseek(fd,offset,SEEK_SET);
        return size==::write(fd,*data,size);
    }
    
    // api may make a confusion but correct. this is because to avoid copy of value(object such as bigint) for future.
    bool read(offset_type const& from, void ** data, offset_type size){
        lseek(fd,from,SEEK_SET);
        return size==::read(fd,*data,size);
    }
    
    bool extend(offset_type extend_size){ fstat(fd,&st); return !ftruncate(fd,st.st_size+extend_size);   }
    int shift(offset_type dst,offset_type src,offset_type size){
        if(buffer_size < size){
            if(buffer)free(buffer);
            buffer = (char*) malloc(buffer_size = size);
        }
        lseek(fd,src,SEEK_SET);
        auto read_size = ::read(fd,buffer,size);
        lseek(fd,dst,SEEK_SET);
        ::write(fd,buffer,read_size);
        return 0;
    }
    
    int flush_buffer(){ return 0; }
};

using file_syscall_16b_pref= file_syscall_premitive<uint64_t>;
using file_syscall_16b_f_pref= file_syscall_premitive<double>;



int main() {
    
    using value_type = uint64_t;
    using offset_type = long;
    
    
    auto is_contained = [](offset_type pos , int direction){
        return bool(
            !direction
            +(2 == (  pos % 16  + (direction>0)))
            +(2 == (!(pos % 16) + (direction<0)))
        );
    };
    

    auto is_same_section = [](auto btres0,auto btres1){
        
        // if  nearest_pos is same, then
            // return -1 (case in the same vacant)
                // 1) !nearest_pos % 16 &&  both direction < 0
                // 2) nearest_pos % 16 &&  both direction > 0
            // return 1 (case in the same block)
                // 1) !nearest_pos % 16 &&  both direction > 0
                // 2) nearest_pos % 16 &&  both direction < 0
        // otherwise return 0 (two exist in different block) 
        
        auto is_same_pos = btres0.nearest_pos == btres1.nearest_pos; 
        auto is_odd = !(btres0.nearest_pos%sizeof(value_type)); 
        auto directions = btres0.direction+btres1.direction;

        return is_same_pos *(
              bool((is_odd * directions==-2) +(!is_odd * directions==2))*-1  
            + bool( is_odd * directions==2 +!is_odd * directions==-2)*1  
        );
    };
    
    
                    
    auto adjust_np_ovf = [](auto & i0,auto pos_min,auto pos_max){
            // if upper overflow then nearest_pos = max_size   
            // if lower overflow then nearest_pos = mix_size   
            return 
                  i0.overflow*((i0.direction > 0)*pos_max + (i0.direction < 0)*pos_min)
                +!i0.overflow*i0.nearest_pos;
    };
    
    auto adjust_nv_ovf = [](auto & i0,auto from,auto to){
            // if upper overflow then nearest_value = to
            // if lower overflow then nearest_value = from
            return 
                  i0.overflow*((i0.direction > 0)*to + (i0.direction < 0)*from)
                +!i0.overflow*i0.nearest_value;
    };
    
    
    
    std::vector<value_type> v;
    for(auto i = 0 ; i < 100; i+=2){
        v.push_back(i*10);
        v.push_back(v.back()+10);
    }
    v.resize(2);
    
    auto f = fopen("merge.cache","w+b");
    auto ws = fwrite(v.data(),sizeof(value_type),v.size(),f);fflush(f);
    auto fd = fileno(f);
    auto pref = file_syscall_premitive<value_type>{.fd=fileno(f)};
    
    {
        auto from = value_type(60);
        auto to = value_type(120);
        
        auto bt = kautil::algorithm::btree_search{&pref};
        auto i0 = bt.search(from,false);
        auto i1 = bt.search(to,false);
        
        
        if(auto cond_section = is_same_section(i0,i1)){ 
            // newly add element to memory or file
            auto is_claim_region = -1==cond_section;  
            auto fsize=  pref.size();
            if(!fsize){
                value_type new_block[2]= {from,to};
                auto new_block_ptr = &new_block;
                pref.write(0,(void**)&new_block_ptr,sizeof(new_block));
            }else if(fsize == (sizeof(value_type)*2) ){
    
                auto min_size = 0;
                auto max_size = pref.size();
    
                // adjust nearest_pos and nearest_value
                
                { // adjust overflow
                    // if upper overflow then 
                        // nearest_pos = max_size   
                        // nearest_value = to
                    
                    // if lower overflow then 
                        // nearest_pos = mix_size   
                        // nearest_value = from
                    
                    i0.nearest_pos = adjust_np_ovf(i0,min_size,max_size);
                    i1.nearest_pos = adjust_np_ovf(i1,min_size,max_size);
                    
                    i0.nearest_value = adjust_nv_ovf(i0,from,to);
                    i1.nearest_value = adjust_nv_ovf(i1,from,to);
                }
                
                
                {
                    // if c0 is contained and 
                        // d < 0 then -=8
                        // d > 0 then nothing
                        // d = 0 and !(%(sizeof(value_type)*2)) then -=8
                        // d = 0 and (%(sizeof(value_type)*2)) then nothing
                    auto c0_is_contained = !i0.overflow * is_contained(i0.nearest_pos,i0.direction);
                    auto c0_cond_d = i1.direction<0;
                    auto c0_cond_zero = (!i1.direction)*!(i1.nearest_pos%(sizeof(value_type)*2));
                    auto c0_adj_pos = -sizeof(value_type); 
                    i1.nearest_pos+= static_cast<offset_type>(c0_is_contained*(c0_cond_d+c0_cond_zero)*c0_adj_pos);
                    
                    // if c1 is contained and 
                        // d < 0 then +=8
                        // d > 0 then nothing
                        // d = 0 and !(%(sizeof(value_type)*2)) then +=8
                        // d = 0 and (%(sizeof(value_type)*2)) then nothing
                    auto c1_is_contained = !i1.overflow * is_contained(i1.nearest_pos,i1.direction);
                    auto c1_cond_d = i1.direction>0;
                    auto c1_cond_zero = (!i1.direction)*!(i1.nearest_pos%(sizeof(value_type)*2));
                    auto c1_adj_pos = sizeof(value_type); 
                    i1.nearest_pos+= static_cast<offset_type>( c1_is_contained*(c1_cond_d+c1_cond_zero)*c1_adj_pos);
                    
                }
            }

            
        }else{
            // wrong file
        }
        
    }

    fclose(f);
    
    
    
    
    
    return 0;
}

#endif