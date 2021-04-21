#include <iostream>
#ifndef __SYNTHESIS__
#include <thread>
#endif	/* __SYNTHESIS__ */
#include "matrix.h"
#include "cache.h"

#define N 64

void vecswap(int a[N], int b[N]) {
	int tmp;

	for (int i = 0; i < N; i++) {
		tmp = a[i];
		a[i] = b[i];
		b[i] = tmp;
	}
}

void vecswap_cache(cache<int, N, 1, 1> &a, cache<int, N, 1, 1> &b) {
	int tmp;

	for (int i = 0; i < N; i++) {
#if 0
		tmp = a[i];
		a[i] = b[i];
		b[i] = tmp;
#endif
		tmp = a.get(i);
		a.set(i, b.get(i));
		b.set(i, tmp);
	}
}

void vecswap_syn(cache<int, N, 1, 1> &a, cache<int, N, 1, 1> &b) {
	vecswap_cache(a, b);

	a.stop_operation();
	b.stop_operation();
}

extern "C" void vecswap_top(int a[N], int b[N]) {
#pragma HLS INTERFACE m_axi port=a bundle=gmem0 max_widen_bitwidth=1024
#pragma HLS INTERFACE m_axi port=b bundle=gmem1 max_widen_bitwidth=1024
#pragma HLS INTERFACE ap_ctrl_hs port=return

#pragma HLS dataflow disable_start_propagation
	cache<int, N, 1, 1> a_cache;
	cache<int, N, 1, 1> b_cache;

#ifdef __SYNTHESIS__
	vecswap_syn(a_cache, b_cache);
	a_cache.operate(a);
	b_cache.operate(b);
#else
	std::thread a_thread(&cache<int, N, 1, 1>::operate, std::ref(a_cache), std::ref(a));
	std::thread b_thread(&cache<int, N, 1, 1>::operate, std::ref(b_cache), std::ref(b));
	std::thread vecswap_thread(vecswap_cache, std::ref(a_cache), std::ref(b_cache));

	vecswap_thread.join();
	a_cache.stop_operation();
	b_cache.stop_operation();
	a_thread.join();
	b_thread.join();
#endif	/* __SYNTHESIS__ */
}

int main() {
	int a[N];
	int b[N];
	int a_ref[N];
	int b_ref[N];

	matrix::generate_random<int>(a, 1, N);
	std::cout << "IN: a=";
	matrix::print(a, 1, N);
	matrix::generate_random<int>(b, 1, N);
	std::cout << "IN: b=";
	matrix::print(b, 1, N);
	for (int i = 0; i < N; i++) {
		a_ref[i] = a[i];
		b_ref[i] = b[i];
	}
	vecswap_top(a, b);
	vecswap(a_ref, b_ref);
	std::cout << "OUT: a=";
	matrix::print(a, 1, N);
	std::cout << "OUT: b=";
	matrix::print(b, 1, N);
	std::cout << "OUT: a_ref=";
	matrix::print(a_ref, 1, N);
	std::cout << "OUT: b_ref=";
	matrix::print(b_ref, 1, N);

	for (int i = 0; i < N; i++) {
		if (a[i] != a_ref[i] || b[i] != b_ref[i])
			return 1;
	}

	return 0;
}

