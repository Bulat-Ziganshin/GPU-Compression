#include <amp.h>
#include <iostream>
using namespace concurrency;

int main()
{
    // simple vector addition example
    std::vector<unsigned> inbuf(1<<27);
    std::vector<unsigned> outbuf((1<<27)/64);

    concurrency::array_view<const unsigned, 1> av0( inbuf.size(),  inbuf);
    concurrency::array_view<unsigned, 1> av1(outbuf.size(), outbuf);
//    av0.discard_data();
    av1.discard_data();

/*
    concurrency::parallel_for_each(av0.extent, [=](concurrency::index<1> idx) restrict(amp)
    {
      av0[idx] = idx[0];
    });
*/

    concurrency::parallel_for_each(av0.extent/16384, [=](concurrency::index<1> idx) restrict(amp)
    {
        unsigned freq[256];
        for (int k=0; k<256; k++)
            freq[k] = 0;
        for (int i=idx[0]*16384,k=0; k<16384; k++,i++)
        {
            unsigned x = av0[i];
            freq[ x      & 255]++;
            freq[(x>> 8) & 255]++;
            freq[(x>>16) & 255]++;
            freq[(x>>24) & 255]++;
        }
        for (int i=idx[0]*256,k=0; k<256; k++,i++)
            av1[i] = freq[k];
    });

    av1.synchronize();
//    for (int i=0; i<outbuf.size(); i++)
//      std::cout << (i%256==0? "\n== " : " ") << outbuf[i];

    return 0;
}
