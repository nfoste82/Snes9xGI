// Includes construction, pixel traversal and destruction, no retained resolver.
// Compile HEIGHT_SOURCE_BASELINE against the startup tree for the full table.
#include "../runtime/frame.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>
static size_t bytes=0;
void *operator new(size_t n){bytes+=n;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void *p)noexcept{std::free(p);}
void operator delete(void *p,size_t)noexcept{std::free(p);}
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    auto frame=std::make_unique<RemasterFrame>();
    if(!S9xReadRemasterFrame(argv[1],*frame))return 2;
    auto &f=*frame;volatile uint64_t checksum=0;
    std::cout<<"iteration,ms,allocated_bytes,instances\n";
    for(int n=0;n<120;++n) {
        size_t before=bytes;auto begin=std::chrono::steady_clock::now();
        {
#ifdef HEIGHT_SOURCE_BASELINE
            RemasterFrameChannelResolver resolver(f);
#else
            RemasterFrameHeightResolver resolver(f);
#endif
            for(size_t at=0;at<f.mainPixels.size();++at)checksum+=resolver.selectHeight(f,at,true).donors[0].instanceId;
        }
        auto end=std::chrono::steady_clock::now();
        std::cout<<n<<','<<std::chrono::duration<double,std::milli>(end-begin).count()<<','<<bytes-before<<','<<f.tileInstances.size()<<'\n';
    }
    std::cerr<<"checksum="<<checksum<<'\n';
}
