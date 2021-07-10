#include <iostream>
#ifndef __SYNTHESIS__
#include <thread>
#endif	/* __SYNTHESIS__ */
#include "matrix.h"
#include "cache.h"

#define N 64

typedef cache<int, true, true, N, 1, 1, 8, true, false> cache_t;

template <typename T>
	void vecswap(T a, T b) {
#pragma HLS inline off
		int tmp;

		for (auto i = 0; i < N; i++) {
			tmp = a[i];
			a[i] = (int) b[i];
			b[i] = tmp;
		}
	}

void vecswap_syn(cache_t &a, cache_t &b) {
	a.init();
	b.init();

	vecswap<cache_t &>(a, b);

	a.stop();
	b.stop();
}

extern "C" void vecswap_top(int a[N], int b[N]) {
#pragma HLS INTERFACE m_axi port=a bundle=gmem0
#pragma HLS INTERFACE m_axi port=b bundle=gmem1
#pragma HLS INTERFACE ap_ctrl_hs port=return

#pragma HLS dataflow disable_start_propagation
	cache_t a_cache;
	cache_t b_cache;

#ifdef __SYNTHESIS__
	a_cache.run(a);
	b_cache.run(b);
	vecswap_syn(a_cache, b_cache);
#else
	a_cache.init();
	b_cache.init();

	std::thread a_cache_thd([&]{a_cache.run(a);});
	std::thread b_cache_thd([&]{b_cache.run(b);});
	std::thread vecswap_thread([&]{vecswap<cache_t &>(a_cache, b_cache);});

	vecswap_thread.join();

	a_cache.stop();
	b_cache.stop();
	a_cache_thd.join();
	b_cache_thd.join();
#endif	/* __SYNTHESIS__ */
}

int main() {
	int a[N];
	int b[N];
	int a_ref[N];
	int b_ref[N];

	for (auto i = 0; i < N; i++) {
		a[i] = i;
		b[i] = -i;
		a_ref[i] = a[i];
		b_ref[i] = b[i];
	}

	vecswap_top(a, b);
	vecswap<int *>(a_ref, b_ref);

	std::cout << "OUT: a=";
	matrix::print(a, 1, N);
	std::cout << "OUT: b=";
	matrix::print(b, 1, N);

	for (int i = 0; i < N; i++) {
		if (a[i] != a_ref[i] || b[i] != b_ref[i])
			return 1;
	}

	return 0;
}

