#include <iostream>
#ifndef __SYNTHESIS__
#include <thread>
#endif	/* __SYNTHESIS__ */
#include "matrix.h"
#include "cache.h"

#define N 128

typedef cache<int, false, true, N, 2, 8> cache_t;

void vecinit(int a[N]) {
	for (int i = 0; i < N; i++) {
		a[i] = i;
	}
}

void vecinit_cache(cache_t &a) {
#pragma HLS inline off
	a.init();

	for (int i = 0; i < N; i++) {
		a.set(i, i);
	}
}

void vecinit_syn(cache_t &a) {
	vecinit_cache(a);

	a.stop();
}

extern "C" void vecinit_top(int a[N]) {
#pragma HLS INTERFACE m_axi port=a bundle=gmem0 max_widen_bitwidth=1024
#pragma HLS INTERFACE ap_ctrl_hs port=return

#pragma HLS dataflow disable_start_propagation
	cache_t a_cache;

#ifdef __SYNTHESIS__
	a_cache.run(a);
	vecinit_syn(a_cache);
#else
	a_cache.init();

	std::thread cache_thread(&cache_t::run, std::ref(a_cache), std::ref(a));
	std::thread vecinit_thread(vecinit_cache, std::ref(a_cache));

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

