#include <iostream>
#ifndef __SYNTHESIS__
#include <thread>
#endif	/* __SYNTHESIS__ */
#include "cache.h"

#define USE_CACHE
#define N 256

typedef cache<int, 1, false, N, 1, 1, 16, false, false> cache_a;
typedef cache<int, 0, true, N, 2, 1, 16, false, false> cache_b;

template <typename A_TYPE, typename B_TYPE>
	void division_in_index(A_TYPE a, B_TYPE b) {
#pragma HLS inline
DII_LOOP:	for (auto i = 0; i < N * 16; i++) {
#pragma HLS pipeline
			b[i / 16] = a[i % 16];
		}
	}

void division_in_index_syn(cache_a &a, cache_b &b) {
#pragma HLS inline off
	a.init();
	b.init();

	division_in_index<cache_a &, cache_b &>(a, b);

	a.stop();
	b.stop();
}

extern "C" void division_in_index_top(int a[N], int b[N]) {
#pragma HLS INTERFACE m_axi port=a bundle=gmem0 latency=1
#pragma HLS INTERFACE m_axi port=b bundle=gmem1 latency=1
#pragma HLS INTERFACE ap_ctrl_hs port=return

#ifdef USE_CACHE
#pragma HLS dataflow disable_start_propagation
	cache_a a_cache;
	cache_b b_cache;

#ifdef __SYNTHESIS__
	a_cache.run(a);
	b_cache.run(b);
	division_in_index_syn(a_cache, b_cache);
#else
	a_cache.init();
	b_cache.init();

	std::thread a_cache_thread([&]{a_cache.run(a);});
	std::thread b_cache_thread([&]{b_cache.run(b);});
	std::thread division_in_index_thread([&]{
			division_in_index<cache_a &, cache_b &>(a_cache, b_cache);
			});

	division_in_index_thread.join();

	a_cache.stop();
	b_cache.stop();
	a_cache_thread.join();
	b_cache_thread.join();
#endif	/* __SYNTHESIS__ */
#else
	division_in_index<int *, int *>(a, b);
#endif /* USE_CACHE */
}

int main() {
	int a[N];
	int b[N];
	int b_ref[N];

	for (auto i = 0; i < N; i++) {
		a[i] = i;
		b[i] = 0;
		b_ref[i] = 0;
	}
	division_in_index_top(a, b);
	division_in_index<int *, int *>(a, b_ref);

	std::cout << "b= ";
	for (auto i = 0; i < N; i++)
		std::cout << b[i] << " ";
	std::cout << "\nb_ref= ";
	for (auto i = 0; i < N; i++)
		std::cout << b_ref[i] << " ";
	std::cout << std::endl;

	for (auto i = 0; i < N; i++) {
		if (b[i] != b_ref[i])
			return 1;
	}

	return 0;
}

