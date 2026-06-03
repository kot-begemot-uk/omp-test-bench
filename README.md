# omp-test-bench
Benchmark software to test different OMP configurations and libraries

Compile with:

```
gcc -fopenmp omp-efficiency.c -lgomp -lm -g -o omp-efficiency
```

Run with:

```
YOUR OMP VARIABLES GO HERE omp-efficiency name-of-thescratcfile
```

On the first run some random data for the test vectors will be written to the scratchfile so that it does not need to be created every time.

Why no Makefile? Well, we want to play with different options, libraries, optimizations, right? It is much easier to tweak a 1 line command line than to deal with 20 different environment variables for a Makefile.
