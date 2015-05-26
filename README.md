# GPU-Compression
Examples of data compression algorithms running on GPU

**Accelerators**: enum all accelerators and print all their properties

**Histogram16**: build 16-bin histogram of lower 4 bits of each byte. That's the maximum amount that can be processed (on my GPU) without using atomics. The speed is 50 GB/s and it's probably limited by amount of arithmetic computations performed.

**Histogram**: build 256-bin histogram. The speed is  15 GB/s and i expect that on newer GPUs with faster atomics it will be much faster.

In order to run executables you may need to installl Microsoft redistributable package.

All tests are performed on GeForce 560Ti, (fabric) overclocked to 900 MHz (384 ALUs in 8 CUs running at 1800 MHz). Memory is 256-bit 2000 MHz = 128 GB/s total bandwidth.
