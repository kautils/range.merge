#ifdef TMAIN_KAUTIL_RANGE_MERGE_INTERFACE


#include <stdint.h>
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
    offset_type size(){ return lseek(fd,0,SEEK_END)- lseek(fd,0,SEEK_SET); }
    
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
            printf("[%lld] %lf %lf\n",i,block[0],block[1]);fflush(outto);
        }
    }
}


#include "merge.hpp"
#include <vector>
int test() { // testing for practice
    using value_type = uint64_t;
    using offset_type = long;
    auto f = fopen("merge.cache","w+b");
    auto fd = fileno(f);
    auto pref = file_syscall_premitive<value_type>{.fd=fileno(f)};
    auto m = kautil::range::merge{&pref};
    m.set_diff(1);
    m.exec(10,20);  
    m.exec(15,25);  
    m.exec(5,25);  
    m.exec(2,31);  
    m.exec(0,100);  
    m.exec(105,110);  
    m.exec(115,120);  
////    *m.exec(0,125);  
    m.exec(126,127);  
    m.exec(129,131);  
    m.exec(132,133);  
    m.exec(135,138);  
    m.exec(140,142);  
    m.exec(134,142);  
    m.exec(61,135);  
    m.exec(145,150);  
    m.exec(155,160);  
    m.exec(5,11);  
    
    m.exec(1,3); // todo : wrong

    debug_out_file<value_type,offset_type>(stdout,fd,0,1000);
    

    fclose(f);

    return 0;

}


int test1() { // testing each 

    using value_type = uint64_t;
    using offset_type = long;


    auto v = std::vector<value_type>{};
    for(auto i = 0 ; i < 100; i+=2){
        v.push_back(i*10+10);
        v.push_back(v.back()+10);
    }
    auto diff = value_type(0);
    auto from = value_type(0);
    auto to = value_type(0);


    { // case single block
        v.resize(2);
            diff = value_type(0);
            from = value_type(0);to = value_type(10);
            from = value_type(15);to = value_type(30);
            from = value_type(0);to = value_type(30);
            from = value_type(10);to = value_type(20);
            from = value_type(10);to = value_type(15);
            from = value_type(15);to = value_type(20);
            from = value_type(15);to = value_type(17); // todo : *
//            from = value_type(5);to = value_type(9);
//            from = value_type(31);to = value_type(35);
    }

    {
//        v.resize(2);
//        diff = 1;
//    // ovf-contained expect ([0,8] - (1,20)) 
//        from = value_type(1);to = value_type(11); 
//        from = value_type(1);to = value_type(9); 
//    // contained-ovf 
//        from = value_type(9);to = value_type(21); //expect [0,8] (10,20)  
//        from = value_type(8);to = value_type(22); //expect [0,8] (8,22)
//    // contained contained (inside-exact) 
//        from = value_type(15);to = value_type(21); // expect [0,8] : (10,20)    
//        from = value_type(15);to = value_type(22);  // expect [0,8] : (10,22)
//
//    // ovf(lower) ovf(lower) expect [0,8] : (5,8)
//        from = value_type(5);to = value_type(9); //expect [0,8] : (5,20)
//        from = value_type(5);to = value_type(8); //expect [0,8] : (5,8)
//    // ovf(lower) ovf(lower) 
//        from = value_type(21);to = value_type(35); // expect [0,8] : (10,35)     
//        from = value_type(22);to = value_type(35); // expect [16,24] : (22,35)
    }


//    { // case two block 
//        v.resize(4);diff = 0;
//        from = value_type(0);to = value_type(5); // ovf-ovf(lower) ([0,8] : (0,5))
//        from = value_type(0);to = value_type(10); // ovf-contained ([0,8] : (0,20))
//        from = value_type(0);to = value_type(30); // ovf-contained ([0,8] : (0,40))
//        from = value_type(0);to = value_type(50); // ovf-ovf ([0,8] : (0,50))
//        from = value_type(45);to = value_type(50); // ovf-ovf ([32,40] : (45,50))
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
//    }
//    
//    { // case three block 
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
//    }

    auto f = fopen("merge.cache","w+b");
    auto ws = fwrite(v.data(),sizeof(value_type),v.size(),f);fflush(f);
    auto fd = fileno(f);
    auto pref = file_syscall_premitive<value_type>{.fd=fileno(f)};
    auto m =kautil::range::merge{&pref};
    m.set_diff(diff);
    m.exec(from,to); // 0,20 => cont-overflow expect [0,8] (0,30) 
    fclose(f);

    return 0;
}



int main() {
    return test1();
    //return test();
}

#endif