![dach_logo](https://user-images.githubusercontent.com/5991825/196959914-0750dd8c-b089-4b53-b077-b3f5b2a78039.svg)

# DaCH
[![vitis_hls](https://img.shields.io/badge/vitis--hls-2020.1%20--%202021.2-blue)](https://docs.xilinx.com/r/2021.2-English/ug1399-vitis-hls/Getting-Started-with-Vitis-HLS)
[![license](https://img.shields.io/badge/license-BSD--3--Clause%20-blue)](https://github.com/brigio345/hls_cache/blob/master/LICENSE)

_DaCH_ (dataflow cache for high-level synthesis) is a library compatible with
*Xilinx Vitis HLS* for automating the memory management of field-programmable
gate arrays (FPGAs) in HLS kernels through caches.

## Overview
_DaCH_ allows allocating a dedicated cache to each DRAM-mapped array (thus
guaranteeing higher hit-ratios by avoiding interferences of accesses to
different arrays).
Each cache consists in a level 2 (L2) cache that exposes one or more (in case
of read-only arrays) ports.
Every port can be associated with a private level 1 (L1) cache.

The L2 cache is implemented as a dataflow task, thus the resulting system
resembles the load, compute, store design paradigm (as recommended by _Xilinx_),
without any source code modification.

Each cache is an object of type `cache`, and is configured by template parameters:
* `typename T`: the data type of the word
* `bool RD_ENABLED`: whether the original array is accessed in read mode
* `bool WR_ENABLED`: whether the original array is accessed in write mode
* `size_t PORTS`: the number of ports (`1` if `WR_ENABLED` is true)
* `size_t MAIN_SIZE`: the size of the original array
* `size_t N_SETS`: the number of L2 sets (`1` for fully-associative cache)
* `size_t N_WAYS`: the number of L2 ways (`1` for direct-mapped cache)
* `size_t N_WORDS_PER_LINE`: the size of the cache line, in words
* `bool LRU`: the replacement policy *least-recently used* if `true`, *last-in
  first-out* otherwise
* `size_t N_L1_SETS`: the number of L1 sets
* `size_t N_L1_WAYS`: the number of L1 ways
* `bool SWAP_TAG_SET`: the address bits mapping (x for more details)
* `size_t LATENCY`: the request-response distance of the L2 cache (y for more details)

## Usage
Given `top`, as an example kernel to be accelerated:
```cpp
template <typename T>
  void compute(T &a) {
    for (auto i = 0; i < (N - 1); i++) {
#pragma HLS pipeline
      a[i] = a[i + 1];
    }
  }

extern "C" void top(DATA_TYPE *a) {
#pragma HLS interface m_axi port=a bundle=gmem0
  compute(a);
}
```

Users can associate a cache to the `a` array by:
1. setting the cache parameters
2. wrapping the core function accessing the array (in this case `compute`) in a function that:
    1. initializes the cache (through the `init` member function)
    2. calls the core function
    3. stops the cache (through the `stop` member function)
3. calling the wrapper function, instead of the original core function (in this case `compute_wrapper`)

In practice, the required changes are:
```diff
+#include "cache.h"
+
+typedef cache<DATA_TYPE, RD_ENABLED, WR_ENABLED,
+   MAIN_SIZE, N_SETS, N_WAYS, N_WORDS_PER_LINE, LRU,
+   SWAP_TAG_SET, LATENCY> cache_type;

template <typename T>
  void compute(T &a) {
    for (auto i = 0; i < (N - 1); i++) {
#pragma HLS pipeline
      a[i] = a[i + 1];
    }
  }

+void compute_wrapper(cache_type &a_cache) {
+ a_cache.init();
+ compute(a_cache);
+ a_cache.stop();
+}
+
extern "C" void top(DATA_TYPE *a) {
#pragma HLS interface m_axi port=a bundle=gmem0
- compute(a);
+#pragma HLS dataflow
+ cache_type a_cache;
+ a_cache.run(a);
+ compute_wrapper(a_cache);
}
```

## Examples

## Documentation
