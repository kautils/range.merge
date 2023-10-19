#ifdef TMAIN_KAUTIL_CACHE_MERGE_STATIC


#include <vector>
#include <stdint.h>
#include "kautil/algorithm/btree_search/btree_search.hpp"
#include "kautil/region/region.hpp"

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
    
    bool extend(offset_type extend_size){ 
        fstat(fd,&st); 
        return !ftruncate(fd,st.st_size+extend_size);   
    }
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



template<typename value_type,typename offset_type>
void debug_out_file(FILE* outto,int fd,offset_type from,offset_type to){
    struct stat st;
    fstat(fd,&st);
    auto cnt = 0;
    lseek(fd,0,SEEK_SET);
    auto start = from;
    auto size = st.st_size;
    value_type block[2];
    for(auto i = 0; i< size; i+=(sizeof(value_type)*2)){
        if(from <= i && i<= to ){
            lseek(fd,i,SEEK_SET);
            ::read(fd,&block, sizeof(value_type) * 2);
            //            read_block(i,block);
            printf("[%lld] %lld %lld\n",i,block[0],block[1]);fflush(outto);
        }
    }
}

template<typename value_type,typename offset_type>
void debug_out_file_f(FILE* outto,int fd,offset_type from,offset_type to){
    struct stat st;
    fstat(fd,&st);
    auto cnt = 0;
    lseek(fd,0,SEEK_SET);
    auto start = from;
    auto size = st.st_size;
    value_type block[2];
    for(auto i = 0; i< size; i+=(sizeof(value_type)*2)){
        if(from <= i && i<= to ){
            lseek(fd,i,SEEK_SET);
            ::read(fd,&block, sizeof(value_type) * 2);
            //            read_block(i,block);
            printf("[%lld] %lf %lf\n",i,block[0],block[1]);fflush(outto);
        }
    }
}

#include "merge.hpp"


int main() {
    
    return temp();
    
    
    using value_type = uint64_t;
    using offset_type = long;

    
    //@note not used
    auto is_contained = [](offset_type pos , int direction){
        return bool(
            !direction
            +(2 == ( bool(pos % 16)  + (direction<0)))
            +(2 == (!bool(pos % 16) + (direction>0)))
        );
    };
    

    auto is_same_section = [](auto btres0,auto btres1){
        
        // if  nearest_pos is same, then
            // return -1 (case in the same vacant)
                // 1) !nearest_pos % 16 &&  both direction < 0 && (both is not exact)
                // 2) nearest_pos % 16 &&  both direction > 0 && (both is not exact)
            // return 1 (case in the same block)
                // 1) !nearest_pos % 16 &&  both direction > 0
                // 2) nearest_pos % 16 &&  both direction < 0
        // otherwise return 0 (two exist in different block) 
        
        auto is_same_pos = btres0.nearest_pos == btres1.nearest_pos; 
        auto is_even = !(btres0.nearest_pos%(sizeof(value_type)*2) ); 
        
        // -2 <= directions <= 2  direction==0 is counted as contained. 
        auto cond_exact = !btres0.direction+!btres1.direction;
        auto directions = btres0.direction+btres1.direction+cond_exact;
        
        return is_same_pos *(
              bool((is_even * directions==-2) +(!is_even * directions==2))*!bool(cond_exact)*-1   
            + bool( is_even * directions==2 +!is_even * directions==-2)*1  
        );
    };
    
    
                    
    auto adjust_np_ovf = [](auto & i0,auto pos_min,auto pos_max){
            // if upper overflow then nearest_pos = max_size   
            // if lower overflow then nearest_pos = mix_size   
            return 
                  i0.overflow*((i0.direction > 0)*pos_max + (i0.direction < 0)*pos_min)
                +!i0.overflow*i0.nearest_pos;
    };
    
    // adjust direction with specified diff
    auto is_adjust_diff = [](auto i0,auto diff,auto input){
        auto i0_diff=
              (i0.nearest_value > input)*(i0.nearest_value-input)
            +!(i0.nearest_value > input)*(-i0.nearest_value+input);
        return (i0_diff <= diff);
    };

    
        
    auto v = std::vector<value_type>{};
    for(auto i = 0 ; i < 100; i+=2){
        v.push_back(i*10+10);
        v.push_back(v.back()+10);
    }
    
    auto diff = value_type(0);
    auto from = value_type(0);
    auto to = value_type(0);
//    {
//        v.resize(0);
//        // case zero : correct
//        // from = value_type(10);
//        // to = value_type(100);
//        
//        // case zero : wrong
//        from = value_type(0);
//        to = value_type(0);
//    }
    
    { // case single block
//        v.resize(2);diff = 0;
//        from = value_type(0);to = value_type(10); // ovf-contained ([0,8] : (0,20))
//        from = value_type(15);to = value_type(30); // contained-ovf 
//        from = value_type(0);to = value_type(30); // ovf-ovf   
//        from = value_type(10);to = value_type(20); // contained contained (exact-exact)   
//        from = value_type(10);to = value_type(15); // contained contained (exact-inside)   
//        from = value_type(15);to = value_type(20); // contained contained (inside-exact)   
//        from = value_type(15);to = value_type(17); // contained contained (inside-inside)   
//        from = value_type(5);to = value_type(9); // ovf(lower) ovf(lower) expect [0,8] : (5,9)   
//        from = value_type(31);to = value_type(35); // ovf(lower) ovf(lower) expect [16,24] : (31,35)   
    }


    { // case single block : change the diff
        v.resize(2);diff = 1;
// ovf-contained expect ([0,8] - (1,20)) 
//    from = value_type(1);to = value_type(11);  // todo  miss : [0,8] - (1,20)
    from = value_type(1);to = value_type(9); 

// contained-ovf 
//    from = value_type(9);to = value_type(21); //expect [0,8] (10,20)  todo  miss : [0,8] - (9,21) 
//    from = value_type(8);to = value_type(22); //expect [0,8] (8,22)
        
// contained contained (inside-exact) 
//    from = value_type(15);to = value_type(21); // expect [0,8] : (10,20)    
//    from = value_type(15);to = value_type(22);  // expect [0,8] : (10,22)
        
// ovf(lower) ovf(lower) expect [0,8] : (5,8)
//    from = value_type(5);to = value_type(9); //expect [0,8] : (5,20)
//    from = value_type(5);to = value_type(8); //expect [0,8] : (5,8)

// ovf(lower) ovf(lower) 
//    from = value_type(21);to = value_type(35); // expect [0,8] : (10,35)     
//    from = value_type(22);to = value_type(35); // expect [16,24] : (22,35)

    }

    { // case two block 
//        v.resize(4);diff = 0;
//        from = value_type(0);to = value_type(5); // ovf-ovf(lower) ([0,8] : (0,5))
//        from = value_type(0);to = value_type(10); // ovf-contained ([0,8] : (0,20))
//        from = value_type(0);to = value_type(30); // ovf-contained ([0,8] : (0,40))
//        from = value_type(0);to = value_type(50); // ovf-ovf ([0,8] : (0,50))
//        from = value_type(45);to = value_type(50); // ovf-ovf ([32,40] : (45,50))
//        
//        from = value_type(10);to = value_type(15); // exact-cont ([10,20] : (0,8))
//        from = value_type(12);to = value_type(15); // cont-cont ([10,20] : (0,8))
//        from = value_type(10);to = value_type(20); // exact-exact ([10,20] : (0,8))
//        from = value_type(12);to = value_type(18); // cont-cont ([10,20] : (0,8))
//        from = value_type(10);to = value_type(25); // exact-vac ([10,25] : (0,8))
//        from = value_type(15);to = value_type(25); // cont-vac ([10,25] : (0,8))
//        from = value_type(15);to = value_type(25); // cont-vac ([10,25] : (0,8))
//        from = value_type(21);to = value_type(25); // vac-vac ([21,25] : (16,24))
//        from = value_type(25);to = value_type(35); // vac-cont ([25,40] : (16,24))
//        from = value_type(25);to = value_type(40); // vac-exact ([25,40] : (16,24))
//        from = value_type(25);to = value_type(45); // vac-vac ([25,45] : (16,24))
//        from = value_type(30);to = value_type(40); // exact-exact ([30,40] : (16,24))
//        from = value_type(30);to = value_type(45); // exact-vac ([30,45] : (16,24))
//        from = value_type(35);to = value_type(40); // cont-exact ([30,40] : (16,24))
//        from = value_type(35);to = value_type(45); // cont-vac ([30,45] : (16,24))
//        from = value_type(45);to = value_type(50); // vac-vac ([45,50] : (32,40))
    }
    
    
    { // case three block 
//        v.resize(6);diff = 0;
//        from = value_type(0);to = value_type(10); // ovf-exact ([0,8] : (0,20))
//        from = value_type(0);to = value_type(60); // ovf-exact ([0,8] : (0,60))
//        from = value_type(0);to = value_type(65); // ovf-ovf ([0,8] : (0,65))
//        from = value_type(0);to = value_type(55); // ovf-cont ([0,8] : (0,60))
//        from = value_type(10);to = value_type(60); // exact-exact ([0,8] : (10,60))
//        from = value_type(10);to = value_type(65); // exact-ovf ([0,8] : (10,65))
//        from = value_type(10);to = value_type(55); // ovf-cont ([0,8] : (0,60))
//        from = value_type(15);to = value_type(60); // cont-exact ([0,8] : (10,60))
//        from = value_type(15);to = value_type(65); // cont-ovf ([0,8] : (10,65))
//        from = value_type(15);to = value_type(55); // cont-cont ([0,8] : (10,60))
//        from = value_type(20);to = value_type(60); // exact-exact ([0,8] : (10,60))
//        from = value_type(25);to = value_type(60); // vac-exact ([16,24] : (25,60))
//        from = value_type(25);to = value_type(65); // vac-ovf ([16,24] : (25,60))
//        from = value_type(25);to = value_type(55); // vac-cont ([16,24] : (25,60))
//        from = value_type(30);to = value_type(60); // exact-exact ([16,24] : (30,60))
//        from = value_type(30);to = value_type(65); // exact-ovf ([16,24] : (30,65))
//        from = value_type(30);to = value_type(55); // exact-cont ([16,24] : (30,60))
//        from = value_type(35);to = value_type(60); // cont-exact ([16,24] : (30,60))
//        from = value_type(35);to = value_type(65); // cont-ovf ([16,24] : (30,65))
//        from = value_type(35);to = value_type(55); // cont-cont ([16,24] : (30,60))
//        from = value_type(45);to = value_type(60); // vac-exact ([32,40] : (45,60))
//        from = value_type(45);to = value_type(65); // vac-ovf ([32,40] : (45,60))
//        from = value_type(45);to = value_type(55); // vac-cont ([32,40] : (45,60))
//        from = value_type(50);to = value_type(60); // exact-exact ([32,40] : (50,60))
//        from = value_type(50);to = value_type(65); // exact-ovf ([32,40] : (50,65))
//        from = value_type(50);to = value_type(55); // exact-cont ([32,40] : (50,60))
//        from = value_type(55);to = value_type(60); // cont-exact ([32,40] : (50,60))
//        from = value_type(55);to = value_type(65); // cont-ovf ([32,40] : (50,65))
//        from = value_type(60);to = value_type(65); // exact-ovf ([32,40] : (50,65))
//        from = value_type(65);to = value_type(70); // ovf-ovf(upper) ([48,56] : (65,75))
    }
    
    
    auto f = fopen("merge.cache","w+b");
    auto ws = fwrite(v.data(),sizeof(value_type),v.size(),f);fflush(f);
    auto fd = fileno(f);
    auto pref = file_syscall_premitive<value_type>{.fd=fileno(f)};
    
    {
        
        auto is_ordered = from<to;
        auto bt = kautil::algorithm::btree_search{&pref};
        auto i0 = bt.search(from,false);
        auto i1 = bt.search(to,false);
        
        
        
        // adjust direction with specified diff
        auto i0_is_diff_adjust = is_adjust_diff(i0,diff,from);
        i0.direction*=!i0_is_diff_adjust;
        i0.overflow*=!i0_is_diff_adjust;
        
        auto i1_is_diff_adjust = is_adjust_diff(i1,diff,to);
        i1.direction*=!i1_is_diff_adjust;
        i1.overflow*=!i1_is_diff_adjust;

        constexpr auto kSameBlock = 1,kDifferent = 0,kSamevacant = -1;
        auto cond_section = is_same_section(i0,i1);
        if(0==(!is_ordered+!(kSameBlock !=cond_section))){ 
            // newly add element to memory or file
            auto is_claim_region = bool(kSamevacant==cond_section);  
            auto fsize=  pref.size();
            if(is_ordered*!fsize){
                printf("write block : from,to(%lld,%lld)",from,to);
                value_type new_block[2]= {from,to};
                auto new_block_ptr = &new_block;
                pref.write(0,(void**)&new_block_ptr,sizeof(*new_block));
//            }else if(0==(!is_ordered+!(fsize == (sizeof(value_type)*2))) ){
            }else if(2==(is_ordered+!(fsize % (sizeof(value_type)*2))) ){
    
                auto squash_src=offset_type(0),squash_length=offset_type(0);
                auto pos_min = 0;
                auto pos_max = pref.size()-sizeof(value_type);
                
                
                // adjust nearest_pos and nearest_value
                { // adjust overflow
                    // if upper overflow then 
                        // nearest_pos = max_size   
                        // nearest_value = to
                    
                    // if lower overflow then 
                        // nearest_pos = mix_size   
                        // nearest_value = from
                    
                        
                    i0.nearest_pos = adjust_np_ovf(i0,pos_min,pos_max);
                    i0.nearest_value = i0.overflow*from + !i0.overflow*i0.nearest_value; 
                    
                    i1.nearest_pos = adjust_np_ovf(i1,pos_min,pos_max);
                    i1.nearest_value = i1.overflow*to + !i1.overflow*i1.nearest_value;

                }
                
                
                {
                    // if c0 is contained and 
                        // d < 0 then -=8
                        // d > 0 then nothing
                        // d = 0 and !(%(sizeof(value_type)*2)) then -=8
                        // d = 0 and (%(sizeof(value_type)*2)) then nothing
                    //auto c0_is_contained = bool(!i0.overflow * is_contained(i0.nearest_pos,i0.direction));
                    auto c0_adj_pos = -sizeof(value_type); 
                    auto c0_cond_d =i0.direction<0; 
                    auto c0_cond_dzero = 2==(!i0.direction+bool(i0.nearest_pos%(sizeof(value_type)*2) ));
                    auto c0_is_contained = is_contained(i0.nearest_pos,i0.direction);
                    auto c0_is_contained_adjust = 3==(c0_is_contained+!i0.overflow+bool(c0_cond_d+c0_cond_dzero)); 
                    if(c0_is_contained_adjust){
                        i0.nearest_pos+=static_cast<offset_type>(c0_adj_pos);
                        auto ptr = &i0.nearest_value;
                        pref.read_value(i0.nearest_pos,&ptr);
                    }
                    i0.nearest_value=
                            !c0_is_contained*from
                            +c0_is_contained*i0.nearest_value;
                    
                    // if c1 is contained and 
                        // d < 0 then nothing
                        // d > 0 then +=8
                        // d = 0 and !(%(sizeof(value_type)*2)) then nothing
                        // d = 0 and (%(sizeof(value_type)*2)) then +=8
                    auto c1_adj_pos = sizeof(value_type); 
                    auto c1_cond_d =i1.direction>0; 
                    auto c1_cond_dzero = 2==(!i1.direction+!bool(i1.nearest_pos%(sizeof(value_type)*2) ));
                    auto c1_is_contained = is_contained(i1.nearest_pos,i1.direction);
                    auto c1_is_contained_adjust = 3==(c1_is_contained+!i1.overflow+bool(c1_cond_d+c1_cond_dzero)); 
                    if(c1_is_contained_adjust){
                        i1.nearest_pos+=static_cast<offset_type>(c1_adj_pos);
                        auto ptr = &i1.nearest_value;
                        pref.read_value(i1.nearest_pos,&ptr);
                    }
                    
                    i1.nearest_value=
                            !c1_is_contained*to
                            +c1_is_contained*i1.nearest_value;
                    
                    
                    auto is_same_vacant = bool(cond_section==kSamevacant);
                    i0.nearest_pos= 
                             !i0.overflow*(is_same_vacant*(i0.nearest_pos+sizeof(value_type))+!is_same_vacant*i0.nearest_pos)
                             +i0.overflow*i0.nearest_pos;
                    i1.nearest_pos= 
                             !i1.overflow*(is_same_vacant*(i0.nearest_pos+sizeof(value_type))+!is_same_vacant*i1.nearest_pos)
                             +i1.overflow*i1.nearest_pos;
                }

                // adjust pos of i0 when it　belongs to vacant 
                auto i0_adj_when_vacant =
                    (2== (!bool(i0.nearest_pos%(sizeof(value_type)*2))+(i0.direction<0)))*(-sizeof(value_type))
                    +(2==( bool(i0.nearest_pos%(sizeof(value_type)*2))+(i0.direction>0)))*(sizeof(value_type));
                i0.nearest_pos+=!i0.overflow*i0_adj_when_vacant;
                
                // express squash with i1 
                squash_src = i1.nearest_pos; 
                squash_length = static_cast<offset_type>(((i1.nearest_pos-i0.nearest_pos)/(sizeof(value_type)*2))*sizeof(value_type)*2);
                i1.nearest_pos-=squash_length;
                //i1.nearest_pos-=static_cast<offset_type>(((i1.nearest_pos-i0.nearest_pos)/(sizeof(value_type)*2))*sizeof(value_type)*2);
                
                
                auto is_ovf_ovf_upper = (2==(i0.overflow+(i0.direction>0))); 
                auto is_ovf_ovf_lower = (2==(i1.overflow+(i1.direction<0))); 
                {// adjust when ovf_ovf
                    auto is_ovf_ovf = bool(is_ovf_ovf_upper+is_ovf_ovf_lower);
                    i0.nearest_pos=is_ovf_ovf_upper*fsize +!is_ovf_ovf_upper*i0.nearest_pos;  
                    i0.nearest_pos=/*is_ovf_ovf_lower*0+*/ !is_ovf_ovf_lower*i0.nearest_pos;  
                    i1.nearest_pos=static_cast<offset_type>(
                              is_ovf_ovf*(i0.nearest_pos+sizeof(value_type))
                            +!is_ovf_ovf*i1.nearest_pos
                            );
                    is_claim_region|=is_ovf_ovf; 
                }
                
                printf("[%ld] %lld\n",i0.nearest_pos,i0.nearest_value);fflush(stdout);
                printf("[%ld] %lld\n",i1.nearest_pos,i1.nearest_value);fflush(stdout);
                
                
                constexpr auto kBuffer = offset_type(512);
                if(is_claim_region){
                    auto claim_size = is_ovf_ovf_upper*(fsize-sizeof(value_type));
                    auto claim_res = kautil::region{&pref}.claim(claim_size,sizeof(value_type)*2,kBuffer);
                    //printf("claim region (src,length)(%ld,%ld) : return(%d)\n",claim_res,claim_size,sizeof(value_type)*2);
                }else{ // squash
                    auto shrink_res = kautil::region{&pref}.shrink(squash_src,squash_length,kBuffer);
                    //printf("squash (src,length)(%ld,%ld) return(%d)\n",squash_length,squash_src,shrink_res);
                }
                
                {
                    auto i0_ptr = &i0.nearest_value;
                    pref.write(i0.nearest_pos,(void**)&i0_ptr,sizeof(value_type));
                }

                {
                    auto i1_ptr = &i1.nearest_value;
                    pref.write(i1.nearest_pos,(void**)&i1_ptr,sizeof(value_type));
                }
                
                
            }else{
                printf("else\n",fsize);
                printf("  %d\n",fsize);
            }

            
            
        }else{
            printf("wrong corrupted cache data\n");
            printf("  is_ordered(%d)\n",is_ordered);
        }

        
        printf("output result\n");
        debug_out_file<value_type,offset_type>(stdout,fd,0,100);
        
        
        
    }
    
    
    
    
    
    
    

    fclose(f);
    
    
    
    
    
    return 0;
}

#endif