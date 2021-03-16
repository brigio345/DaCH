#include <iostream>
#ifndef __SYNTHESIS__
#include <thread>
#endif	/* __SYNTHESIS__ */
#include "matrix.h"
#include "cache.h"

typedef int data_type;

#define N 5
#define M 4
#define P 3

extern "C" void matmul_top(data_type *a_arr, data_type *b_arr, data_type *c_arr) {
#pragma HLS INTERFACE m_axi port=a_arr offset=slave bundle=gmem0 
#pragma HLS INTERFACE m_axi port=b_arr offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=c_arr offset=slave bundle=gmem2 
#pragma HLS INTERFACE s_axilite port=a_arr bundle=control
#pragma HLS INTERFACE s_axilite port=b_arr bundle=control
#pragma HLS INTERFACE s_axilite port=c_arr bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
	cache<data_type> a_cache(a_arr);
	cache<data_type> b_cache(b_arr);
	cache<data_type> c_cache(c_arr);
#ifdef __SYNTHESIS__
	matrix::multiply<cache<data_type> &, N, M, P>(a_cache, b_cache, c_cache);
	a_cache.operate();
	b_cache.operate();
	c_cache.operate();
#else
	std::thread a_thread(&cache<data_type>::operate, std::ref(a_cache));
	std::thread b_thread(&cache<data_type>::operate, std::ref(b_cache));
	std::thread c_thread(&cache<data_type>::operate, std::ref(c_cache));
	std::thread matmul_thread(&matrix::multiply<cache<data_type> &, N, M, P>,
			std::ref(a_cache), std::ref(b_cache), std::ref(c_cache));

	matmul_thread.join();

	a_cache.stop_operation();
	b_cache.stop_operation();
	c_cache.stop_operation();

	a_thread.join();
	b_thread.join();
	c_thread.join();
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
	matrix::multiply<data_type *, N, M, P>(a_arr, b_arr, c_arr_ref);

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

