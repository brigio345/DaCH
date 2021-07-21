#include <iostream>
#ifndef __SYNTHESIS__
#include <thread>
#endif	/* __SYNTHESIS__ */
#include "cache.h"

#define USE_CACHE
#define N 1024

typedef ap_int<21> data_type;
typedef cache<data_type, 1, false, N, 1, 1, 16, false, false> cache_a;
typedef cache<data_type, 0, true, N, 2, 1, 16, false, false> cache_b;

template <typename A_TYPE, typename B_TYPE>
	void non_byte_access(A_TYPE a, B_TYPE b) {
#pragma HLS inline
NBA_LOOP:	for (auto i = 0; i < N; i++) {
			b[i] = a[i];
		}
	}

void non_byte_access_syn(cache_a &a, cache_b &b) {
#pragma HLS inline off
	a.init();
	b.init();

	non_byte_access<cache_a &, cache_b &>(a, b);

	a.stop();
	b.stop();
}

extern "C" void non_byte_access_top(data_type a[N], data_type b[N]) {
#pragma HLS INTERFACE m_axi port=a bundle=gmem0 latency=1
#pragma HLS INTERFACE m_axi port=b bundle=gmem1 latency=1

#ifdef USE_CACHE
#pragma HLS dataflow disable_start_propagation
	cache_a a_cache;
	cache_b b_cache;

#ifdef __SYNTHESIS__
	a_cache.run(a);
	b_cache.run(b);
	non_byte_access_syn(a_cache, b_cache);
#else
	a_cache.init();
	b_cache.init();

	std::thread a_cache_thread([&]{a_cache.run(a);});
	std::thread b_cache_thread([&]{b_cache.run(b);});
	std::thread non_byte_access_thread([&]{
			non_byte_access<cache_a &, cache_b &>(a_cache, b_cache);
			});

	non_byte_access_thread.join();

	a_cache.stop();
	b_cache.stop();
	a_cache_thread.join();
	b_cache_thread.join();
#endif	/* __SYNTHESIS__ */
#else
	non_byte_access<data_type *, data_type *>(a, b);
#endif
}

int main() {
	data_type a[N];
	data_type b[N];
	data_type b_ref[N];

	for (auto i = 0; i < N; i++) {
		a[i] = i;
		b[i] = 0;
		b_ref[i] = 0;
	}
	non_byte_access_top(a, b);
	non_byte_access<data_type *, data_type *>(a, b_ref);

	for (auto i = 0; i < N; i++) {
		if (b[i] != b_ref[i])
			return 1;
	}

	return 0;
}

