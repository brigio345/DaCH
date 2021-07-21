#include <iostream>
#ifndef __SYNTHESIS__
#include <thread>
#endif	/* __SYNTHESIS__ */
#include "cache.h"

#define USE_CACHE
#define N 1024

typedef cache<int, 1, false, N, 1, 1, 16, false, false> cache_a;
typedef cache<int, 0, true, N, 2, 1, 16, false, false> cache_b;

template <typename A_TYPE, typename B_TYPE>
	void non_continuous_access(A_TYPE a, B_TYPE b) {
#pragma HLS inline
NCA_LOOP:	for (auto i = 0; i < N / 4; i++) {
#pragma HLS pipeline
			b[i * 4] = a[i * 4];
		}
	}

void non_continuous_access_syn(cache_a &a, cache_b &b) {
#pragma HLS inline off
	a.init();
	b.init();

	non_continuous_access<cache_a &, cache_b &>(a, b);

	a.stop();
	b.stop();
}

extern "C" void non_continuous_access_top(int a[N], int b[N]) {
#pragma HLS INTERFACE m_axi port=a bundle=gmem0 latency=1
#pragma HLS INTERFACE m_axi port=b bundle=gmem1 latency=1

#ifdef USE_CACHE
#pragma HLS dataflow disable_start_propagation
	cache_a a_cache;
	cache_b b_cache;

#ifdef __SYNTHESIS__
	a_cache.run(a);
	b_cache.run(b);
	non_continuous_access_syn(a_cache, b_cache);
#else
	a_cache.init();
	b_cache.init();

	std::thread a_cache_thread([&]{a_cache.run(a);});
	std::thread b_cache_thread([&]{b_cache.run(b);});
	std::thread non_continuous_access_thread([&]{
			non_continuous_access<cache_a &, cache_b &>(a_cache, b_cache);
			});

	non_continuous_access_thread.join();

	a_cache.stop();
	b_cache.stop();
	a_cache_thread.join();
	b_cache_thread.join();
#endif	/* __SYNTHESIS__ */
#else
	non_continuous_access<int *, int *>(a, b);
#endif
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
	non_continuous_access_top(a, b);
	non_continuous_access<int *, int *>(a, b_ref);

	for (auto i = 0; i < N; i++) {
		if (b[i] != b_ref[i])
			return 1;
	}

	return 0;
}

