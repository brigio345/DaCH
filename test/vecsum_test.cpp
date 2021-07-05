#include <iostream>
#ifndef __SYNTHESIS__
#include <thread>
#endif	/* __SYNTHESIS__ */
#include "cache_multiport.h"
#define DEBUG

#define N 128

static const size_t RD_PORTS = 2;

typedef cache_multiport<int, RD_PORTS, N, 1, 1, 8> cache_a;

template <typename T>
	void vecsum(T a, int &sum) {
#pragma HLS inline
		int tmp = 0;
		int data;

VECSUM_LOOP:	for (auto i = 0; i < N; i++) {
#pragma HLS pipeline
#pragma HLS unroll factor=RD_PORTS
			data = a[i];
			tmp += data;
#ifdef DEBUG
			std::cout << i << " " << data << std::endl;
#endif /* DEBUG */
		}

		sum = tmp;
	}

void vecsum_syn(cache_a &a, int &sum) {
#pragma HLS inline off
	a.init();

	vecsum<cache_a &>(a, sum);

	a.stop();
}

extern "C" void vecsum_top(int a[N], int &sum) {
#pragma HLS INTERFACE m_axi port=a bundle=gmem0 max_widen_bitwidth=1024
#pragma HLS INTERFACE ap_ctrl_hs port=return

#pragma HLS dataflow disable_start_propagation
	cache_a a_cache;

#ifdef __SYNTHESIS__
	a_cache.run(a);
	vecsum_syn(a_cache, sum);
#else
	a_cache.init();

	std::thread cache_thread([&]{a_cache.run(a);});
	std::thread vecsum_thread([&]{vecsum<cache_a &>(a_cache, sum);});

	vecsum_thread.join();

	a_cache.stop();
	cache_thread.join();
#endif	/* __SYNTHESIS__ */
}

int main() {
	int a[N];
	int sum;
	int sum_ref;

	for (auto i = 0; i < N; i++)
		a[i] = i;
	vecsum_top(a, sum);
	vecsum<int *>(a, sum_ref);
	std::cout << "sum=" << sum << std::endl;
	std::cout << "sum_ref=" << sum_ref << std::endl;

	return (sum != sum_ref);
}

