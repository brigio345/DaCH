#include <iostream>
#ifndef __SYNTHESIS__
#include <thread>
#endif	/* __SYNTHESIS__ */
#include "cache.h"

#define USE_CACHE
#define N 256

typedef cache<int, 1, true, N, 2, 1, 16, false, false> cache_a;
typedef cache<int, 1, true, N, 2, 1, 16, false, false> cache_b;

template <typename A_TYPE, typename B_TYPE>
	void array_war(A_TYPE a, B_TYPE b) {
#pragma HLS inline
WAR_LOOP:	for (auto i = 0; i < N; i++) {
			b[i] = a[i] + 100;
			a[i] = b[i];
		}
	}

void array_war_syn(cache_a &a, cache_b &b) {
#pragma HLS inline off
	a.init();
	b.init();

	array_war<cache_a &, cache_b &>(a, b);

	a.stop();
	b.stop();
}

extern "C" void array_war_top(int a[N], int b[N]) {
#pragma HLS INTERFACE m_axi port=a bundle=gmem0 latency=1
#pragma HLS INTERFACE m_axi port=b bundle=gmem1 latency=1

#ifdef USE_CACHE
#pragma HLS dataflow disable_start_propagation
	cache_a a_cache;
	cache_b b_cache;

#ifdef __SYNTHESIS__
	a_cache.run(a);
	b_cache.run(b);
	array_war_syn(a_cache, b_cache);
#else
	a_cache.init();
	b_cache.init();

	std::thread a_cache_thread([&]{a_cache.run(a);});
	std::thread b_cache_thread([&]{b_cache.run(b);});
	std::thread array_war_thread([&]{
			array_war<cache_a &, cache_b &>(a_cache, b_cache);
			});

	array_war_thread.join();

	a_cache.stop();
	b_cache.stop();
	a_cache_thread.join();
	b_cache_thread.join();
#endif	/* __SYNTHESIS__ */
#else
	array_war<int *, int *>(a, b);
#endif
}

int main() {
	int a[N];
	int b[N];
	int a_ref[N];
	int b_ref[N];

	for (auto i = 0; i < N; i++) {
		a[i] = i;
		b[i] = i;
		a_ref[i] = i;
		b_ref[i] = i;
	}
	array_war_top(a, b);
	array_war<int *, int *>(a_ref, b_ref);

	for (auto i = 0; i < N; i++) {
		if ((a[i] != a_ref[i]) || (b[i] != b_ref[i]))
			return 1;
	}

	return 0;
}

