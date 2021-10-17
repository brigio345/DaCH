#include <iostream>
#include <cstdlib>
#include <cstring>
#ifndef __SYNTHESIS__
#define PROFILE
#include <thread>
#endif	/* __SYNTHESIS__ */
#include "cache.h"

#define CACHE
//#define GLOBAL
//#define LOCAL

#define N_BITS 7
#define N (1 << N_BITS)

typedef int data_type;
typedef cache<data_type, true, true, 1, N, 1, 2, 16, false, 1, 4> cache_a;

void compare_and_swap(data_type &pos0, data_type &pos1, const bool dir) {
#pragma HLS inline
	if ((pos0 > pos1) != dir) {
		const data_type tmp = pos0;
		pos0 = pos1;
		pos1 = tmp;
	}
}

template <typename T>
void bitonic_sort(T a, const bool dir = true) {
#pragma HLS inline
BITS_LOOP:for (auto bits = 1; bits <= N_BITS; bits++) {
STRIDE_LOOP:	for (auto stride = (bits - 1); stride >= 0; stride--) {
I_LOOP:			for (auto i = 0; i < (N / 2); i++) {
#pragma HLS pipeline II=1
				bool l_dir = ((i >> (bits - 1)) & 0x01);
				l_dir = (l_dir ^ dir);
				const int step = (1 << stride);
				const int POS = (i * 2 - (i & (step - 1)));
				data_type p0 = a[POS];
				data_type p1 = a[POS + step];
				compare_and_swap(p0, p1, l_dir);
				a[POS] = p0;
				a[POS + step] = p1;
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
#pragma HLS INTERFACE m_axi port=a_arr offset=slave bundle=gmem0
#pragma HLS INTERFACE ap_ctrl_hs port=return

#if defined(CACHE)
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
	printf("A hit ratio = %.3f ((%d + %d) / %d)\n",
			a_cache.get_hit_ratio(), a_cache.get_n_l1_hits(),
			a_cache.get_n_hits(), a_cache.get_n_reqs());
#endif /* PROFILE */
#endif	/* __SYNTHESIS__ */
#elif defined(GLOBAL)
	bitonic_sort<data_type *>(a_arr);
#elif defined(LOCAL)
	data_type local_arr[N];

	memcpy(local_arr, a_arr, (N * sizeof(data_type)));
	bitonic_sort(local_arr);
	memcpy(a_arr, local_arr, (N * sizeof(data_type)));
#else
	std::cout << "CACHE or GLOBAL or LOCAL must be defined" << std::endl;
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
		if (a_arr[i] != a_arr_ref[i])
			return 1;
	}

	for (auto i = 1; i < N; i++) {
		if (a_arr[i] > a_arr[i - 1])
			return 2;
	}

	return 0;
}

