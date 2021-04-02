#include <iostream>
#ifndef __SYNTHESIS__
#include <thread>
#endif	/* __SYNTHESIS__ */
#include "matrix.h"
#include "cache_ro.h"
#include "cache_wo.h"

#define N 20
#define N_PORTS 1

template <typename T>
	void vecsum(T vec, int &sum) {
		int tmp = 0;

		for (int i = 0; i < N; i++) {
#pragma HLS pipeline
			tmp += vec[i];
		}

		sum = tmp;
	}

void vecsum_syn(cache_ro<int> &vec, int &sum) {
	vecsum<cache_ro<int> &>(vec, sum);
	vec.stop_operation();
}

void vecsum_top(int vec[N], int &sum) {
#pragma HLS INTERFACE m_axi port=vec offset=slave bundle=gmem0
#pragma HLS stable variable=vec
#pragma HLS INTERFACE ap_ctrl_hs port=return

#pragma HLS dataflow disable_start_propagation
	cache_ro<int> vec_cache(vec);

#ifdef __SYNTHESIS__
	vecsum_syn(vec_cache, sum);
	vec_cache.operate();
#else
	std::thread vec_thread(&cache_ro<int>::operate, std::ref(vec_cache));
	std::thread vecsum_thread(vecsum<cache_ro<int> &>, std::ref(vec_cache), std::ref(sum));

	vecsum_thread.join();
	vec_cache.stop_operation();
	vec_thread.join();
#endif	/* __SYNTHESIS__ */
}

int main() {
	int vec[N];
	int sum;
	int sum_ref;

	matrix::generate_random<int>(vec, 1, N);
	matrix::print(vec, 1, N);
	vecsum_top(vec, sum);
	vecsum<int *>(vec, sum_ref);
	std::cout << "sum=" << sum << std::endl;
	std::cout << "sum_ref=" << sum_ref << std::endl;

	return (sum != sum_ref);
}

