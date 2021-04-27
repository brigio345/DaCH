#include <iostream>
#ifndef __SYNTHESIS__
#include <thread>
#endif	/* __SYNTHESIS__ */
#include "matrix.h"
#include "cache.h"

#define N 128

typedef cache<int, 1, 0, N> cache_a;

void vecsum(int a[N], int &sum) {
	int tmp = 0;

	for (int i = 0; i < N; i++) {
		tmp += a[i];
	}

	sum = tmp;
}

void vecsum_cache(cache_a &a, int &sum) {
	int tmp = 0;

	for (int i = 0; i < N; i++) {
		tmp += a.get(i);
	}

	sum = tmp;
}

void vecsum_syn(cache_a &a, int &sum) {
	vecsum_cache(a, sum);

	a.stop();
}

extern "C" void vecsum_top(int a[N], int &sum) {
#pragma HLS INTERFACE m_axi port=a bundle=gmem0 max_widen_bitwidth=1024
#pragma HLS INTERFACE ap_ctrl_hs port=return

#pragma HLS dataflow disable_start_propagation
	cache_a a_cache;

#ifdef __SYNTHESIS__
	vecsum_syn(a_cache, sum);
	a_cache.run(a);
#else
	std::thread a_thread(&cache_a::run, std::ref(a_cache), std::ref(a));
	std::thread vecsum_thread(vecsum_cache, std::ref(a_cache), std::ref(sum));

	vecsum_thread.join();
	a_cache.stop();
	a_thread.join();
#endif	/* __SYNTHESIS__ */
}

int main() {
	int a[N];
	int sum;
	int sum_ref;

	matrix::generate_random<int>(a, 1, N);
	std::cout << "a=";
	matrix::print(a, 1, N);
	vecsum_top(a, sum);
	vecsum(a, sum_ref);
	std::cout << "sum=" << sum << std::endl;
	std::cout << "sum_ref=" << sum_ref << std::endl;

	return (sum != sum_ref);
}

