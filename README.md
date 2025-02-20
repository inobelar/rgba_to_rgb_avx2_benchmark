# Benchmark: RGBA to RGB copying using AVX2

Usage:

```shell
$ g++ -v -std=c++11 -O3 -march=native -mtune=native -mavx2 -DNDEBUG -I./third_party/nanobench/include -o bench main.cpp
$ ./bench
```

For benchmarking used: [nanobench](https://github.com/martinus/nanobench)
