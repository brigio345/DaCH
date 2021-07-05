#include <iostream>
#ifndef __SYNTHESIS__
#include <thread>
#endif	/* __SYNTHESIS__ */
#include "matrix.h"
#include "cache.h"

#define N 128

typedef cache<int, false, true, N, 2, 1, 8, true> cache_t;

template <typename T>
	void vecinit(T a) {
#pragma HLS inline
		for (int i = 0; i < N; i++) {
#pragma HLS pipeline
			a[i] = i;
		}
	}

void vecinit_syn(cache_t &a) {
#pragma HLS inline off
	a.init();

	vecinit<cache_t &>(a);

	a.stop();
}

extern "C" void vecinit_top(int a[N]) {
#pragma HLS INTERFACE m_axi port=a bundle=gmem0
#pragma HLS INTERFACE ap_ctrl_hs port=return

#pragma HLS dataflow disable_start_propagation
	cache_t a_cache;

#ifdef __SYNTHESIS__
	a_cache.run(a);
	vecinit_syn(a_cache);
#else
	a_cache.init();

	std::thread cache_thread([&]{a_cache.run(a);});
	std::thread vecinit_thread([&]{vecinit<cache_t &>(a_cache);});

	vecinit_thread.join();

	a_cache.stop();
	cache_thread.join();
#endif	/* __SYNTHESIS__ */
}

int main() {
	int a[N];
	int ret = 0;

	vecinit_top(a);

	for (int i = 0; i < N; i++) {
		std::cout << a[i] << " ";
		if (a[i] != i)
			ret = 1;
	}

	return ret;
}

