// Copyright 2015 Bulat Ziganshin
// Released to public domain

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
    static const unsigned WARP     = 128;
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

for (int i=0; i<10; i++)
    concurrency::parallel_for_each ((av0.extent/(ITER/COMMON)).tile<WARP>(), [=](concurrency::tiled_index<WARP> idx) restrict(amp)
    {
        tile_static unsigned freq[BIN*WARP/COMMON];

        for (int j=idx.local[0],k=0;  k<BIN/COMMON;  k++,j+=WARP)
            freq[j] = 0;
        idx.barrier.wait_with_tile_static_memory_fence();

        unsigned INTERLEAVE = WARP/COMMON;
        unsigned offset = idx.local[0] % INTERLEAVE;
        unsigned i0 = idx.tile_origin[0]*(ITER/COMMON) + offset*ITER + (idx.local[0] / INTERLEAVE);
        for (int i=i0,k=0;  k<ITER/COMMON;  k++,i+=COMMON)
        {
            unsigned x = av0[i];

#define inc(x) concurrency::atomic_fetch_inc(x)
//#define inc(x) (*(x))++
            inc( & freq[( x      % BIN)*INTERLEAVE+offset]);
            inc( & freq[((x>> 8) % BIN)*INTERLEAVE+offset]);
            inc( & freq[((x>>16) % BIN)*INTERLEAVE+offset]);
            inc( & freq[((x>>24) % BIN)*INTERLEAVE+offset]);
        }
        idx.barrier.wait_with_tile_static_memory_fence();

        for (int i=idx.tile_origin[0]*(BIN/COMMON)+idx.local[0], j=idx.local[0], k=0;  k<BIN/COMMON;  k++,i+=WARP,j+=WARP)
            av1[i] = freq[j];
    });

//    av1.get_source_accelerator_view().create_marker().wait();
//    av1.get_source_accelerator_view().wait();
    av1.synchronize();
    t.Stop();
    std::cout << t.Elapsed() << " milliseconds\n";
//    for (int i=0; i<outbuf.size(); i++)
//      std::cout << (i%256==0? "\n== " : " ") << outbuf[i];

    return 0;
}
