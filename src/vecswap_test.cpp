#include <iostream>
#ifndef __SYNTHESIS__
#include <thread>
#endif	/* __SYNTHESIS__ */
#include "matrix.h"
#include "cache.h"

#define N 512

typedef cache<int, 1, 1, N> cache_t;

void vecswap(int a[N], int b[N]) {
	int tmp;

	for (int i = 0; i < N; i++) {
		tmp = a[i];
		a[i] = b[i];
		b[i] = tmp;
	}
}

void vecswap_cache(cache_t &a, cache_t &b) {
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

void vecshift_cache(cache_t &a) {
	int tmp;

	for (int i = N - 1; i > 0; i--) {
		tmp = a.get(i - 1);
		a.set(i, tmp);
	}
}

void vecswap_syn(cache_t &a, cache_t &b) {
	vecswap_cache(a, b);

	a.stop();
	b.stop();
}

void vecshift_syn(cache_t &a) {
	vecshift_cache(a);

	a.stop();
}

extern "C" void vecswaap_top(int a[N], int b[N]) {
#pragma HLS INTERFACE m_axi port=a bundle=gmem0 max_widen_bitwidth=1024
#pragma HLS INTERFACE m_axi port=b bundle=gmem1 max_widen_bitwidth=1024
#pragma HLS INTERFACE ap_ctrl_hs port=return

#pragma HLS dataflow disable_start_propagation
	cache_t a_cache;
	cache_t b_cache;

	a_cache.run(a);
	b_cache.run(b);

#ifdef __SYNTHESIS__
	vecswap_syn(a_cache, b_cache);
#else
	std::thread vecswap_thread(vecswap_cache, std::ref(a_cache), std::ref(b_cache));

	vecswap_thread.join();

	a_cache.stop();
	b_cache.stop();
#endif	/* __SYNTHESIS__ */
}

extern "C" void vecswap_top(int a[N]) {
#pragma HLS INTERFACE m_axi port=a bundle=gmem0 max_widen_bitwidth=1024
#pragma HLS INTERFACE ap_ctrl_hs port=return

#pragma HLS dataflow disable_start_propagation
	cache_t a_cache;

#ifdef __SYNTHESIS__
	a_cache.run(a);
	vecshift_syn(a_cache);
#else
	std::thread cache_thread(&cache_t::run, std::ref(a_cache), std::ref(a));
	std::thread vecshift_thread(vecshift_cache, std::ref(a_cache));

	vecshift_thread.join();

	a_cache.stop();
	cache_thread.join();
#endif	/* __SYNTHESIS__ */
}

int main() {
	int a[N];
	int b[N];
	int a_ref[N];
	int b_ref[N];

#if 0
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
#endif
	for (int i = 0; i < N; i++)
		a[i] = i;

	vecswap_top(a);

	for (int i = 0; i < N; i++)
		std::cout << a[i] << " ";
	std::cout << std::endl;

	for (int i = 1; i < N; i++)
		if (a[i] != (i - 1))
			return 1;
	return 0;
}

