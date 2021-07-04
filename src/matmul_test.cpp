#include <iostream>
#ifndef __SYNTHESIS__
#define PROFILE
#include <thread>
#endif	/* __SYNTHESIS__ */
#include "matrix.h"
#include "cache.h"
#include "cache_multiport.h"

#define N 8
#define M 8
#define P 8

static const int RD_PORTS = 2;

typedef int data_type;
typedef cache_multiport<data_type, RD_PORTS, N * M, 1, 1, M> cache_a;
typedef cache_multiport<data_type, RD_PORTS, M * P, 1, M, 4> cache_b;
typedef cache<data_type, false, true, N * P, 2, 1, M, false> cache_c;

void multiply_syn(cache_a &a_cache, cache_b &b_cache, cache_c &c_cache) {
#pragma HLS inline off
	matrix::multiply<cache_a &, cache_b &, cache_c &, N, M, P, RD_PORTS>
			(a_cache, b_cache, c_cache);
	a_cache.stop();
	b_cache.stop();
	c_cache.stop();
}

extern "C" void matmul_top(data_type a_arr[N * M], data_type b_arr[M * P], data_type c_arr[N * P]) {
#pragma HLS INTERFACE m_axi port=a_arr offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=b_arr offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=c_arr offset=slave bundle=gmem2
#pragma HLS INTERFACE ap_ctrl_hs port=return

#pragma HLS dataflow disable_start_propagation
	cache_a a_cache;
	cache_b b_cache;
	cache_c c_cache;

#ifdef __SYNTHESIS__
	a_cache.run(a_arr);
	b_cache.run(b_arr);
	c_cache.run(c_arr);
	multiply_syn(a_cache, b_cache, c_cache);
#else
	std::thread a_thd([&]{a_cache.run(a_arr);});
	std::thread b_thd([&]{b_cache.run(b_arr);});
	std::thread c_thd([&]{c_cache.run(c_arr);});
	std::thread matmul_thread([&]{matrix::multiply<cache_a &, cache_b &,
			cache_c &, N, M, P, RD_PORTS>(a_cache, b_cache, c_cache);});

	matmul_thread.join();

	a_cache.stop();
	b_cache.stop();
	c_cache.stop();
	a_thd.join();
	b_thd.join();
	c_thd.join();
#endif	/* __SYNTHESIS__ */
}

int main() {
	data_type a_arr[N * M];
	data_type b_arr[M * P];
	data_type c_arr[N * P];
	data_type c_arr_ref[N * P];

	matrix::generate_random<data_type>(a_arr, N, M);
	matrix::generate_random<data_type>(b_arr, M, P);

	// matrix multiplication with caches
	matmul_top(a_arr, b_arr, c_arr);
	// standard matrix multiplication
	matrix::multiply_ref<N, M, P>(a_arr, b_arr, c_arr_ref);

	std::cout << "A = " << std::endl;
	matrix::print(a_arr, N, M);
	std::cout << std::endl << "B = " << std::endl;
	matrix::print(b_arr, M, P);
	std::cout << std::endl << "C (reference) = " << std::endl;
	matrix::print(c_arr_ref, N, P);
	std::cout << std::endl << "C (cache) = " << std::endl;
	matrix::print(c_arr, N, P);

	if (!matrix::compare<data_type *>(c_arr, c_arr_ref, N, P)) {
		std::cerr << "Mismatch detected" << std::endl;
		return -1;
	}

	return 0;
}

