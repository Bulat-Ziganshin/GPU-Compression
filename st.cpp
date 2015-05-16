// Copyright 2015 Bulat Ziganshin
// Released to public domain

#include <amp.h>
#include <iostream>
#include "timer.h"

int main()
{
    static const unsigned DATASIZE = 1<<25;
    static const unsigned CHUNK    = 1<<17;
    static const unsigned BIN      = 1<<4;
    static const unsigned ITER     = CHUNK/sizeof(unsigned);
    static const unsigned WARP     = 128;
    static const unsigned COMMON   = 8;   // how many threads share single set of counters, i.e. handle the same chunk

    std::vector<unsigned> inbuf(DATASIZE/sizeof(unsigned));
    std::vector<unsigned> outbuf((DATASIZE/CHUNK)*BIN);

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


    concurrency::parallel_for_each (av0.extent/(ITER/COMMON), [=](concurrency::index<1> idx) restrict(amp)
    {
        unsigned freq[BIN];
        for (int i=0; i<BIN; i++)
            freq[i] = 0;

        for (int i=0; i<ITER/COMMON; i++)
        {
            unsigned x = av0[idx[0]*ITER/COMMON+i];
            freq[ x      % BIN]++;
            freq[(x>> 8) % BIN]++;
            freq[(x>>16) % BIN]++;
            freq[(x>>24) % BIN]++;
        }
        for (int i=0; i<BIN; i++)
            concurrency::atomic_fetch_add (&av1[idx[0]/COMMON*BIN+i], freq[i]);
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
