#include <iostream>
#ifndef __SYNTHESIS__
#include <thread>
#endif	/* __SYNTHESIS__ */
#include "cache.h"

//#define USE_CACHE
#define N 1024
#define BUFFER_SIZE 128

typedef cache<int, 1, false, N, 1, 1, 16, false, false> cache_a;
typedef cache<int, 0, true, N, 2, 1, 16, false, false> cache_b;

template <typename A_TYPE, typename B_TYPE>
	void unaligned_start_index_0(A_TYPE a, B_TYPE b) {
#pragma HLS inline
outCopyA:	for (auto i = 0; i < N; i += BUFFER_SIZE) {
copyA:			for (auto j = 0; j < (BUFFER_SIZE / 2); ++j) {
				b[j * 2] = a[i + j * 2]; // even index for b
				b[(j * 2) | 1] = a[i + ((j * 2) | 1)]; // odd index for b
			}
		}
	}

void unaligned_start_index_0_syn(cache_a &a, cache_b &b) {
#pragma HLS inline off
	a.init();
	b.init();

	unaligned_start_index_0<cache_a &, cache_b &>(a, b);

	a.stop();
	b.stop();
}

extern "C" void unaligned_start_index_0_top(int a[N], int b[BUFFER_SIZE]) {
#pragma HLS INTERFACE m_axi port=a bundle=gmem0 latency=1
#pragma HLS INTERFACE m_axi port=b bundle=gmem1 latency=1

#ifdef USE_CACHE
#pragma HLS dataflow disable_start_propagation
	cache_a a_cache;
	cache_b b_cache;

#ifdef __SYNTHESIS__
	a_cache.run(a);
	b_cache.run(b);
	unaligned_start_index_0_syn(a_cache, b_cache);
#else
	a_cache.init();
	b_cache.init();

	std::thread a_cache_thread([&]{a_cache.run(a);});
	std::thread b_cache_thread([&]{b_cache.run(b);});
	std::thread unaligned_start_index_0_thread([&]{
			unaligned_start_index_0<cache_a &, cache_b &>(a_cache, b_cache);
			});

	unaligned_start_index_0_thread.join();

	a_cache.stop();
	b_cache.stop();
	a_cache_thread.join();
	b_cache_thread.join();
#endif	/* __SYNTHESIS__ */
#else
	unaligned_start_index_0<int *, int *>(a, b);
#endif
}

int main() {
	int a[N];
	int b[BUFFER_SIZE];
	int b_ref[BUFFER_SIZE];

	for (auto i = 0; i < N; i++)
		a[i] = i;

	unaligned_start_index_0_top(a, b);
	unaligned_start_index_0<int *, int *>(a, b_ref);

	for (auto i = 0; i < BUFFER_SIZE; i++) {
		if (b[i] != b_ref[i])
			return 1;
	}

	return 0;
}
