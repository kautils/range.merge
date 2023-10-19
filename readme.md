### kautil_range_merge
* merge ranges.
* part of cache library.

### example
```c++
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
    m.exec(105,110);  
    m.exec(115,120);  
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
    m.exec(1,3);  
    
    fclose(f);

    return 0;
    
}

```

