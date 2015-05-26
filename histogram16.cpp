// Copyright 2015 Bulat Ziganshin
// Released to public domain

#include <amp.h>
#include <iostream>
#include "timer.h"

int main()
{
    static const unsigned DATASIZE = 1<<25;
    static const unsigned BIN      = 1<<4;   // count low 4-bit freqs for every byte
    static const unsigned CHUNK_CNT= 1<<8;   // split data into 256 blocks and calculate histogram for each one separately
    static const unsigned CHUNK    = DATASIZE/CHUNK_CNT;         // 128k
    static const unsigned ITER     = CHUNK/sizeof(unsigned);     //  32k
    static const unsigned STRIDE   = 128;   // how many elements to process by the single thread

    std::vector<unsigned> inbuf(CHUNK_CNT*ITER);
    std::vector<unsigned> outbuf(CHUNK_CNT*BIN);

    concurrency::array_view<unsigned, 2> srcdata(CHUNK_CNT, ITER, inbuf);
    concurrency::array_view<unsigned, 2> histogram(CHUNK_CNT, BIN, outbuf);
    srcdata.discard_data();
    histogram.discard_data();

    concurrency::parallel_for_each (srcdata.extent, [=](concurrency::index<2> idx) restrict(amp)
    {
        srcdata[idx] = (srcdata.extent[1]*idx[0]+idx[1]) * 1234567890;
    });
    srcdata.synchronize();

    Timer t;
    t.Start();

    concurrency::extent<2>  data_extent (srcdata.extent[0], srcdata.extent[1]/STRIDE);
    static const unsigned TILE = 256, TILE1 = 64, TILE0 = TILE/TILE1;  // single tile processes TILE0 blocks and TILE1 independent parts in every block

    const uint64_t DATASET = uint64_t(10)<<30;
    std::cout << "Measuring on " << (DATASET>>30) << " GB dataset";

    for (int i=0; i<DATASET/DATASIZE; i++)
        concurrency::parallel_for_each (data_extent.tile<TILE0,TILE1>(), [=](concurrency::tiled_index<TILE0,TILE1> idx) restrict(amp)
        {
            tile_static unsigned freq[BIN*TILE];
            unsigned loc_idx = idx.local[0]*TILE0+idx.local[1];
            for (int i=0; i<BIN; i++)
                freq[i*TILE+loc_idx] = 0;
            idx.barrier.wait_with_tile_static_memory_fence();

            for (int i=0; i<STRIDE; i++)
            {
                unsigned x = srcdata (idx.global[0], i*(ITER/STRIDE)+idx.global[1]);
                freq[( x      % BIN)*TILE+loc_idx]++;
                freq[((x>> 8) % BIN)*TILE+loc_idx]++;
                freq[((x>>16) % BIN)*TILE+loc_idx]++;
                freq[((x>>24) % BIN)*TILE+loc_idx]++;
            }
            idx.barrier.wait_with_tile_static_memory_fence();

            unsigned sum = 0;
            for (int i=0; i<BIN; i++)
                sum += freq [loc_idx*BIN + i];   // assume TILE1 >= BIN
            concurrency::atomic_fetch_add (&histogram(idx.global[0], (loc_idx*BIN)/TILE),  sum);
        });

    histogram.synchronize();
    t.Stop();
    std::cout << ": " << t.Elapsed() << " milliseconds\n";

    return 0;
}
