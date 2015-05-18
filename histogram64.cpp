// Copyright 2015 Bulat Ziganshin
// Released to public domain

#include <amp.h>
#include <iostream>
#include "timer.h"

int main()
{
    static const unsigned DATASIZE = 1<<25;
    static const unsigned BIN      = 1<<6;   // count low 6-bit freqs
    static const unsigned CHUNK_CNT= 1<<8;   // split data into 256 blocks and count each one separately
    static const unsigned CHUNK    = DATASIZE/CHUNK_CNT;         // 128k
    static const unsigned ITER     = CHUNK/sizeof(unsigned);     //  32k
    static const unsigned STRIDE   = 512;   // how many elements process by the single thread

    std::vector<unsigned> inbuf(CHUNK_CNT*ITER);
    std::vector<unsigned> outbuf(CHUNK_CNT*BIN);

    concurrency::array_view<unsigned, 2> av0(CHUNK_CNT, ITER, inbuf);
    concurrency::array_view<unsigned, 2> av1(CHUNK_CNT, BIN, outbuf);
    av0.discard_data();
    av1.discard_data();

    concurrency::parallel_for_each (av0.extent, [=](concurrency::index<2> idx) restrict(amp)
    {
        av0[idx] = (av0.extent[1]*idx[0]+idx[1]) * 1234567890;
    });
    av0.synchronize();

//    for (int i=0; i<5; i++)
//      std::cout << (i%256==0? "\n== " : " ") << inbuf[i];

    Timer t;
    t.Start();

    concurrency::extent<2>  data_extent (av0.extent[0], av0.extent[1]/STRIDE);
    static const unsigned WARP = 64, WARP1 = 32, WARP0 = WARP/WARP1;

    const uint64_t DATASET = uint64_t(10)<<30;
    std::cout << "Measuring on " << (DATASET>>30) << " GB dataset";
for (int i=0; i<DATASET/DATASIZE; i++)
    concurrency::parallel_for_each (data_extent.tile<WARP0,WARP1>(), [=](concurrency::tiled_index<WARP0,WARP1> idx) restrict(amp)
    {
        tile_static unsigned freq[BIN*WARP];
        unsigned loc_idx = idx.local[0]*WARP0+idx.local[1];
        for (int i=0; i<BIN; i++)
            freq[i*WARP+loc_idx] = 0;
        idx.barrier.wait_with_tile_static_memory_fence();

        for (int i=0; i<STRIDE; i++)
        {
            unsigned x = av0 (idx.global[0], i*(ITER/STRIDE)+idx.global[1]);
//          unsigned x = av0 (idx.global[0], idx.global[1]*STRIDE+i);             // without memory read coalescing - 11x slower!
            freq[( x      % BIN)*WARP+loc_idx]++;
            freq[((x>> 8) % BIN)*WARP+loc_idx]++;
            freq[((x>>16) % BIN)*WARP+loc_idx]++;
            freq[((x>>24) % BIN)*WARP+loc_idx]++;
        }
        idx.barrier.wait_with_tile_static_memory_fence();

        unsigned sum = 0;
        for (int i=0; i<BIN; i++)
            sum += freq [i*WARP + loc_idx];
        concurrency::atomic_fetch_add (&av1(idx.global[0], idx.local[1] % BIN),  sum);
    });

    av1.synchronize();
    t.Stop();
    std::cout << ": " << t.Elapsed() << " milliseconds\n";

    return 0;
}
