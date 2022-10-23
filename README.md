![dach_logo](https://user-images.githubusercontent.com/5991825/196959914-0750dd8c-b089-4b53-b077-b3f5b2a78039.svg)

# DaCH
[![vitis_hls](https://img.shields.io/badge/vitis--hls-2020.1--2021.2%20%7C%202022.2-blue)](https://docs.xilinx.com/r/en-US/ug1399-vitis-hls)
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

### Cache parameters
Each cache is an object of type `cache`, and is configured by template parameters:
* `typename T`: the data type of the word.
* `bool RD_ENABLED`: whether the original array is accessed in read mode.
* `bool WR_ENABLED`: whether the original array is accessed in write mode.
* `size_t PORTS`: the number of ports (`1` if `WR_ENABLED` is true).
* `size_t MAIN_SIZE`: the size of the original array.
* `size_t N_SETS`: the number of L2 sets (`1` for fully-associative cache).
* `size_t N_WAYS`: the number of L2 ways (`1` for direct-mapped cache).
* `size_t N_WORDS_PER_LINE`: the size of the cache line, in words.
* `bool LRU`: the replacement policy *least-recently used* if `true`, *last-in
  first-out* otherwise.
* `size_t N_L1_SETS`: the number of L1 sets.
* `size_t N_L1_WAYS`: the number of L1 ways.
* `bool SWAP_TAG_SET`: the address bits mapping (x for more details).
* `size_t LATENCY`: the request-response distance of the L2 cache (y for more details).

## Usage
Users can associate a `cache` to an array by:
1. fixing the [`cache` parameters](#cache-parameters)
2. creating a wrapper function that:
    1. initializes the `cache` (through the `init` member function)
    2. calls the function that accesses the array
    3. stops the `cache` (through the `stop` member function)
3. calling the wrapper function, instead of the original function, in a
   `dataflow` region

As an example, the changes required for accelerating the `vecinit` kernel
consist in:
```diff
+#include "cache.h"
+
+typedef cache<int, RD_ENABLED, WR_ENABLED,
+   MAIN_SIZE, N_SETS, N_WAYS, N_WORDS_PER_LINE, LRU,
+   SWAP_TAG_SET, LATENCY> cache_type;

template <typename T>
  void vecinit(T &a) {
    for (int i = 0; i < N; i++) {
#pragma HLS pipeline
      a[i] = i;
    }
  }

+void vecinit_wrapper(cache_type &a_cache) {
+ a_cache.init();
+ vecinit(a_cache);
+ a_cache.stop();
+}
+
extern "C" void top(int *a) {
#pragma HLS interface m_axi port=a bundle=gmem0
- vecinit(a);
+#pragma HLS dataflow
+ cache_type a_cache;
+ a_cache.run(a);
+ vecinit_wrapper(a_cache);
}
```

Note that the algorithm original code (i.e., the `vecinit` function) is
unchanged: it is enough to change the input data type from `int *` to `cache &`.

### Cache parameters tuning
The `cache` exposes a set of profiling functions:
* `int get_n_reqs(const unsigned int port)`: returns the number of requests
  (reads and writes) to the L2 cache, on the port `port`.
* `int get_n_hits(const unsigned int port)`: returns the number of hits to the
  L1 cache, on the port `port`.
* `int get_n_l1_reqs(const unsigned int port)`: returns the number of requests
  (reads and writes) to the L2 cache, on the port `port`.
* `int get_n_l1_hits(const unsigned int port)`: returns the number of hits to the
  L1 cache, on the port `port`.
* `double get_hit_ratio(const unsigned int port)`: returns the hit ratio to the
  L2 and L1 caches, on the port `port`.

Therefore, users may tune the `cache` parameters based on the data returned by
these profiling functions.

## Examples

## Documentation

