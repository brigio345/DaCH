![dach_logo](https://user-images.githubusercontent.com/5991825/196959914-0750dd8c-b089-4b53-b077-b3f5b2a78039.svg)

# DaCH
[![vitis_hls](https://img.shields.io/badge/vitis--hls-2020.1--2021.2%20%7C%202022.2-blue)](https://docs.xilinx.com/r/en-US/ug1399-vitis-hls)
[![license](https://img.shields.io/badge/license-BSD--3--Clause%20-blue)](https://github.com/brigio345/hls_cache/blob/master/LICENSE)

_DaCH_ (dataflow cache for high-level synthesis) is a library compatible with
*Xilinx Vitis HLS* for automating the memory management of field-programmable
gate arrays (FPGAs) in HLS kernels through caches.

_DaCH_ provides the `cache` class, that allows associating a dedicated cache to
each DRAM-mapped array, that automatically buffers the data in block-RAMs and registers.

A `cache` object consists in a level 2 (L2) cache that exposes one or more
(in case of read-only arrays) ports.
Each L2 port can be associated with a private level 1 (L1) cache.
The L2 cache is implemented as a dataflow task that consumes a stream of
requests (read or write) from the function accessing the array, and produces
a stream of responses (read) in the opposite direction.

_DaCH_ fits well within the *Xilinx* guidelines for
[creating efficient HLS designs](https://docs.xilinx.com/r/en-US/ug1399-vitis-hls/Creating-Efficient-HLS-Designs),
since it automates multiple suggested techniques:
* the kernel decomposition in producer-consumer tasks, since the L2 cache is a dataflow task
	that consumes a stream of requests (read or write) and produces a stream of responses (read)
> * Decompose the kernel algorithm by building a pipeline of producer-consumer tasks,
> 	modeled using a Load, Compute, Store (LCS) coding pattern
* the creation of internal caching structures
> * In many cases, the sequential order of data in and out of the Compute tasks is different
> 	from the arrangement order of data in global memory.
> 	* In this situation, optimizing the interface functions requires creating internal caching
> 		structures that gather enough data and organize it appropriately to minimize the overhead
>			of global memory accesses while being able to satisfy the sequential order expected by the
>			streaming interface 
* the access to large contiguous blocks of data, since `cache` accesses the DRAM in lines
	(i.e., blocks of words)
> * Global memories have long access times (DRAM, HBM) and their bandwidth is limited (DRAM).
> 	To reduce the overhead of accessing global memory, the interface function needs to
> 	* Access sufficiently large contiguous blocks of data (to benefit from bursting)

> * Maximize the port width of the interface, i.e., the bit-width of each AXI port
>		by setting it to 512 bits (64 bytes).
> 	* Accessing the global memory is expensive and so accessing larger word sizes is
>			more efficient.
> 	* Transfer large blocks of data from the global device memory.
>			One large transfer is more efficient than several smaller transfers. 
* the avoidance of redundant DRAM accesses, since `cache` accesses DRAM in case of miss only
> * Accessing data sequentially leads to larger bursts (and higher data throughput efficiency)
> 	as compared to accessing random and/or out-of-order data (where burst analysis will fail)
> 	* Avoid redundant accesses (to preserve bandwidth)

Further details can be found in the paper:
[Array-specific dataflow caches for high-level synthesis of memory-intensive algorithms on FPGAs](https://ieeexplore.ieee.org/document/9940270).

## Cache parameters
The `cache` specifications are configurable through template parameters:
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

## Profiling
`cache` exposes a set of profiling functions, useful for tuning the
[cache parameters](#cache-parameters):
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

## Usage
A `cache` object is associated to an array by:
1. adding the _DaCH_ `src` directory to the include path, and including
	the `cache.h` header file
2. setting the [`cache` parameters](#cache-parameters), possibly taking advantage
	of the [profiling functions](#profiling) for their fine-tuning
4. instantiating the `cache` and calling the function to be accelerated through
   the `cache_wrapper` function, in a `dataflow` region

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

extern "C" void top(int *a) {
#pragma HLS interface m_axi port=a bundle=gmem0
+#pragma HLS dataflow
+ cache_type a_cache(a);
- vecinit(a);
+ cache_wrapper(vecinit<cache_type>, a_cache);
}
```

Note that the algorithm original code (i.e., the `vecinit` function) is
unchanged: it is enough to change the input data type from `int *` to `cache &`.

## Examples

## Publication
BibTeX:
```bibtex
@article{DaCH,
	author={Brignone, Giovanni and Usman Jamal, M. and Lazarescu, Mihai T. and Lavagno, Luciano},
	journal={IEEE Access},
	title={Array-specific dataflow caches for high-level synthesis of memory-intensive algorithms on FPGAs},
	year={2022},
	doi={10.1109/ACCESS.2022.3219868}
}
```
Plain text:
```
G. Brignone, M. Usman Jamal, M. T. Lazarescu and L. Lavagno, "Array-specific dataflow caches for high-level synthesis of memory-intensive algorithms on FPGAs," in IEEE Access, 2022, doi: 10.1109/ACCESS.2022.3219868.
```
