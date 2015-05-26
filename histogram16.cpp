// Copyright 2015 Bulat Ziganshin
// Released to public domain

#include <amp.h>
#include <stdio.h>
#include "timer.h"

int main()
{
    static const unsigned DATASIZE = 1<<25;
    static const unsigned BINS     = 1<<4;   // count low 4-bit freqs for every byte
    static const unsigned CHUNK_CNT= 1<<8;   // split data into 256 blocks and calculate histogram for each one separately
    static const unsigned CHUNK    = DATASIZE/CHUNK_CNT;         // 128k
    static const unsigned ITER     = CHUNK/sizeof(unsigned);     //  32k
    static const unsigned STRIDE   = 128;   // how many elements to process by the single thread

    std::vector<unsigned> inbuf(CHUNK_CNT*ITER);
    std::vector<unsigned> outbuf(CHUNK_CNT*BINS);

    concurrency::array_view<unsigned, 2> srcdata(CHUNK_CNT, ITER, inbuf);
    concurrency::array_view<unsigned, 2> histogram(CHUNK_CNT, BINS, outbuf);
    srcdata.discard_data();
    histogram.discard_data();

    // fill with pseudo-random data
    concurrency::parallel_for_each (srcdata.extent, [=](concurrency::index<2> idx) restrict(amp)
    {
        srcdata[idx] = (srcdata.extent[1]*idx[0]+idx[1]) * 1234567891;
    });
    srcdata.synchronize();

    const uint64_t DATASET = uint64_t(10)<<30;
    printf("Measuring on %d GiB random data", int(DATASET>>30));
    Timer t;  t.Start();

    for (int i=0; i<DATASET/DATASIZE; i++)
    {
        concurrency::extent<2>  data_extent (srcdata.extent[0], srcdata.extent[1]/STRIDE);
        static const unsigned TILE = 256, TILE1 = 64, TILE0 = TILE/TILE1;  // single tile processes TILE0 blocks and TILE1 independent parts in every block

        concurrency::parallel_for_each (data_extent.tile<TILE0,TILE1>(), [=](concurrency::tiled_index<TILE0,TILE1> idx) restrict(amp)
        {
            tile_static unsigned freq[BINS*TILE];
            unsigned loc_idx = idx.local[0]*TILE1+idx.local[1];
            for (int i=0; i<BINS; i++)
                freq[i*TILE+loc_idx] = 0;
            //idx.barrier.wait_with_tile_static_memory_fence();   // may improve performance

            for (int i=0; i<STRIDE; i++)
            {
                #define count(bin)  (freq[(bin)*TILE+loc_idx]++)
                unsigned x = srcdata (idx.global[0], i*(ITER/STRIDE)+idx.global[1]);
                count( x      % BINS);
                count((x>> 8) % BINS);
                count((x>>16) % BINS);
                count((x>>24) % BINS);
                #undef count
            }
            idx.barrier.wait_with_tile_static_memory_fence();

            // We update only sum per thread, that's correct only if TILE1 >= BINS
            unsigned sum = 0;
            for (int i=0; i<BINS; i++)
                sum += freq [loc_idx*BINS + i];
            concurrency::atomic_fetch_add (&histogram(idx.global[0], (loc_idx*BINS)/TILE),  sum);
        });
    }

    histogram.synchronize();
    t.Stop();  auto speed = DATASET/(t.Elapsed()/1000);
    printf(": %.3lf milliseconds = %.3lf GB/s = %.3lf GiB/s\n", t.Elapsed(), speed/1e9, speed/(1<<30));
    return 0;
}
