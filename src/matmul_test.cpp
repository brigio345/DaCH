#include <iostream>
#ifndef __SYNTHESIS__
#include <thread>
#endif	/* __SYNTHESIS__ */
#include "matrix.h"
#include "cache.h"

#define N 4
#define M 4
#define P 4

typedef int data_type;
typedef cache<data_type, 1, 0, N * M> cache_a;
typedef cache<data_type, 1, 0, M * P> cache_b;
typedef cache<data_type, 0, 1, N * P> cache_c;

void multiply_syn(cache_a &a_cache, cache_b &b_cache, cache_c &c_cache) {
#pragma HLS inline off
	a_cache.init();
	b_cache.init();
	c_cache.init();

	matrix::multiply<cache_a &, cache_b &, cache_c &, N, M, P>
			(a_cache, b_cache, c_cache);
	a_cache.stop();
	b_cache.stop();
	c_cache.stop();
}

extern "C" void matmul_top(data_type a_arr[N * M], data_type b_arr[M * P], data_type c_arr[N * P]) {
#pragma HLS INTERFACE m_axi port=a_arr offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=b_arr offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=c_arr offset=slave bundle=gmem2
#pragma HLS stable variable=a_arr
#pragma HLS stable variable=b_arr
#pragma HLS stable variable=c_arr
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
	std::thread a_thd(&cache_a::run, std::ref(a_cache), std::ref(a_arr));
	std::thread b_thd(&cache_b::run, std::ref(b_cache), std::ref(b_arr));
	std::thread c_thd(&cache_c::run, std::ref(c_cache), std::ref(c_arr));
	std::thread matmul_thread(&matrix::multiply<cache_a &, cache_b &, cache_c &, N, M, P>,
			std::ref(a_cache), std::ref(b_cache), std::ref(c_cache));

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

