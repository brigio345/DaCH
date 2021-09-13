#include <iostream>
#include <cstdlib>
#ifndef __SYNTHESIS__
#define PROFILE
#include <thread>
#endif	/* __SYNTHESIS__ */
#include "cache.h"

#define USE_CACHE
#define N 128

static const int RD_PORTS = 1;

typedef int data_type;
typedef cache<data_type, true, true, RD_PORTS, N, 1, 1, 16, false, 4> cache_a;

template <typename T>
void bitonic_sort(T a) {
#pragma HLS inline
K_LOOP:	for (auto k = 2; k <= N; k *= 2) {
J_LOOP:		for (auto j = (k / 2); j > 0; j /= 2) {
I_LOOP:			for (auto i = 0; i < N; i++) {
#pragma HLS pipeline II=1
				const auto l = (i ^ j);
				if (l > i) {
					if ((((i & k) == 0) && (a[i] > a[l])) ||
							(((i & k) != 0) && (a[i] < a[l]))) {
						data_type tmp = a[i];
						a[i] = a[l];
						a[l] = tmp;
					}
				}
			}
		}
	}
}

void bitonic_sort_syn(cache_a &a_cache) {
#pragma HLS inline off
	a_cache.init();

	bitonic_sort<cache_a &>(a_cache);

	a_cache.stop();
}

extern "C" void bitonic_sort_top(data_type a_arr[N]) {
#pragma HLS INTERFACE m_axi port=a_arr offset=slave bundle=gmem0 latency=1
#pragma HLS INTERFACE ap_ctrl_hs port=return

#ifdef USE_CACHE
#pragma HLS dataflow disable_start_propagation
	cache_a a_cache;

#ifdef __SYNTHESIS__
	a_cache.run(a_arr);
	bitonic_sort_syn(a_cache);
#else
	a_cache.init();

	std::thread a_thd([&]{a_cache.run(a_arr);});
	std::thread bitonic_sort_thread([&]{bitonic_sort<cache_a &>(a_cache);});

	bitonic_sort_thread.join();

	a_cache.stop();
	a_thd.join();

#ifdef PROFILE
	printf("A hit ratio = %.3f (%d / %d)\n",
			a_cache.get_hit_ratio(), a_cache.get_n_hits(),
			a_cache.get_n_reqs());
#endif /* PROFILE */
#endif	/* __SYNTHESIS__ */
#else
	bitonic_sort<data_type *>(a_arr);
#endif
}

int main() {
	data_type a_arr[N];
	data_type a_arr_ref[N];

	for (auto i = 0; i < N; i++) {
		a_arr[i] = (std::rand() % 1000);
		a_arr_ref[i] = a_arr[i];
	}

	// bitonic sorting with caches
	bitonic_sort_top(a_arr);
	// standard bitonic sorting
	bitonic_sort<data_type *>(a_arr_ref);

	for (auto i = 0; i < N; i++) {
		std::cout << a_arr[i] << std::endl;
		if (a_arr[i] != a_arr_ref[i])
			return 1;
	}

	return 0;
}

