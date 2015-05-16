#include <amp.h>
#include <iostream>
#include "timer.h"
//using namespace concurrency;

int main()
{
    static const unsigned DATASIZE = 1<<29;
    static const unsigned CHUNK    = 1<<14;
    static const unsigned BIN      = 1<<8;
    static const unsigned ITER     = CHUNK/sizeof(unsigned);
    static const unsigned WARP     = 64;
    static const unsigned COMMON   = 16;   // how many threads share single set of counters, i.e. handle the same chunk

    std::vector<unsigned> inbuf(DATASIZE/sizeof(unsigned));
    std::vector<unsigned> outbuf((inbuf.size()/CHUNK)*BIN);

    concurrency::array_view<unsigned, 1> av0( inbuf.size(),  inbuf);
    concurrency::array_view<unsigned, 1> av1(outbuf.size(), outbuf);
    av0.discard_data();
    av1.discard_data();

    concurrency::parallel_for_each (av0.extent, [=](concurrency::index<1> idx) restrict(amp)
    {
        av0[idx] = idx[0]*1234567890;
    });
    av0.synchronize();

    Timer t;
    t.Start();

    concurrency::parallel_for_each ((av0.extent/(ITER/COMMON)).tile<WARP>(), [=](concurrency::tiled_index<WARP> idx) restrict(amp)
    {
        tile_static unsigned freq[BIN*WARP/COMMON];
        int chunk_num = idx.global[0]/COMMON;
//        int sub_chunk = idx.global[0]%COMMON;
//        int base = BIN*(chunk_num%(WARP/COMMON));
//        int first_index = chunk_num*ITER + sub_chunk * (ITER/COMMON);

//        for (int k=0; k<BIN; k++)
//            freq[k*WARP+idx.local[0]] = 0;

        for (int j=idx.local[0]*BIN/COMMON,k=0;  k<BIN/COMMON;  k++,j++)
            freq[j] = 0;
        idx.barrier.wait_with_tile_static_memory_fence();

        int base = BIN*(idx.local[0]/COMMON);
        for (int i=idx.global[0]*(ITER/COMMON),k=0;  k<ITER/COMMON;  k++,i++)
        {
            unsigned x = av0[i];

#define inc(x) concurrency::atomic_fetch_inc(x)
//#define inc(x) (*(x))++
            inc( & freq[base +  x      % BIN]);
            inc( & freq[base + (x>> 8) % BIN]);
            inc( & freq[base + (x>>16) % BIN]);
            inc( & freq[base + (x>>24) % BIN]);
        }
        idx.barrier.wait_with_tile_static_memory_fence();

        for (int i=idx.global[0]*BIN/COMMON,j=idx.local[0]*BIN/COMMON,k=0;  k<BIN/COMMON;  k++,i++,j++)
            av1[i] = freq[j];
    });

//    av1.get_source_accelerator_view().wait();
    av1.synchronize();
    t.Stop();
    std::cout << t.Elapsed() << " milliseconds\n";
//    for (int i=0; i<outbuf.size(); i++)
//      std::cout << (i%256==0? "\n== " : " ") << outbuf[i];

    return 0;
}
