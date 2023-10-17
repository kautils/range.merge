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
    
    
//    auto adjust_pole_position = [](offset_type input_pos,int direction){
//        
//        
//        
//    };
    
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
        auto i0 = bt.search(from);
        auto i1 = bt.search(to);
        
        auto fsize=  pref.size();
        if(!fsize){
            value_type new_block[2]= {from,to};
            auto new_block_ptr = &new_block;
            pref.write(0,(void**)&new_block_ptr,sizeof(new_block));
        }else if(fsize == (sizeof(value_type)*2) ){
            
            auto c0 = is_contained(i0.neighbor_pos,i0.direction);
            auto c1 = is_contained(i1.neighbor_pos,i1.direction);
            
            
            //adjust0;
            if(c0){
                // d < 0 then -=8
                // d > 0 then nothing
                // d = 0 and !(%(sizeof(value_type)*2)) then -=8
                // d = 0 and (%(sizeof(value_type)*2)) then nothing
                i0.nearest_pos-=
                         ( i0.direction<0)*(sizeof(value_type))
                        +(!i0.direction)*(i0.nearest_pos%(sizeof(value_type)*2))*(sizeof(value_type));
            }
            
            //adjust1;
            if(c1){
                // d < 0 then +=8
                // d > 0 then nothing
                // d = 0 and !(%(sizeof(value_type)*2)) then +=8
                // d = 0 and (%(sizeof(value_type)*2)) then nothing
                i1.nearest_pos+=
                         ( i1.direction>0)*(sizeof(value_type))
                        +(!i1.direction)*!(i1.nearest_pos%(sizeof(value_type)*2))*(sizeof(value_type));
            }
            
            
            
        }
        
    }

    fclose(f);
    
    
    
    
    
    return 0;
}

#endif