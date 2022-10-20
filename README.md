![dach_logo](https://user-images.githubusercontent.com/5991825/196959914-0750dd8c-b089-4b53-b077-b3f5b2a78039.svg)

# DaCH
[![GitHub release (latest by date)](https://img.shields.io/github/v/release/brigio345/hls_cache)](https://github.com/brigio345/hls_cache/releases/latest)
[![vitis_hls](https://img.shields.io/badge/Vitis%20HLS-2021.2-green)](https://www.xilinx.com/support/documentation-navigation/design-hubs/2021-2/dh0090-vitis-hls-hub.html)
[![license](https://img.shields.io/badge/license-BSD--3--Clause%20-blue)](https://github.com/brigio345/hls_cache/blob/master/LICENSE)

_DaCH_ (dataflow cache for high-level synthesis) is a C++ **cache library** for accelerating memory-bound *Xilinx Vitis HLS* kernels.

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

## Benchmarking

## Documentation
