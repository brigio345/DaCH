#include <iostream>
#include <cstring>
#ifndef __SYNTHESIS__
#define PROFILE
#include <thread>
#endif	/* __SYNTHESIS__ */
#include "matrix.h"
#include "cache.h"

#define CACHE
//#define GLOBAL
//#define LOCAL

#define N 16
#define M 16
#define P 16

static const int RD_PORTS = 1;

typedef int data_type;
typedef cache<data_type, true, false, RD_PORTS, N * M, 1, 1, 16, false, 0, 1> cache_a;
typedef cache<data_type, true, false, RD_PORTS, M * P, 1, 1, 16, false, 0, 1> cache_b;
typedef cache<data_type, false, true, 1, N * P, 1, 1, 16, false, 0, 1> cache_c;

void multiply_syn(cache_a &a_cache, cache_b &b_cache, cache_c &c_cache) {
#pragma HLS inline off
	a_cache.init();
	b_cache.init();
	c_cache.init();

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

#if defined(CACHE)
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
	a_cache.init();
	b_cache.init();
	c_cache.init();

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

#ifdef PROFILE
	printf("A hit ratio = %.3f ((%d + %d) / %d)\n",
			a_cache.get_hit_ratio(), a_cache.get_n_l1_hits(),
			a_cache.get_n_hits(), a_cache.get_n_reqs());
	printf("B hit ratio = %.3f ((%d + %d) / %d)\n",
			b_cache.get_hit_ratio(), b_cache.get_n_l1_hits(),
			b_cache.get_n_hits(), b_cache.get_n_reqs());
	printf("C hit ratio = %.3f ((%d + %d) / %d)\n",
			c_cache.get_hit_ratio(), c_cache.get_n_l1_hits(),
			c_cache.get_n_hits(), c_cache.get_n_reqs());
#endif /* PROFILE */
#endif	/* __SYNTHESIS__ */
#elif defined(GLOBAL)
	matrix::multiply<data_type *, data_type *, data_type *, N, M, P, RD_PORTS>(a_arr, b_arr, c_arr);
#elif defined(LOCAL)
	data_type a_local[N * M];
	data_type b_local[M * P];
	data_type c_local[N * P];
#pragma HLS array_reshape variable=a_local cyclic factor=32
#pragma HLS array_reshape variable=b_local cyclic factor=32
#pragma HLS array_reshape variable=c_local cyclic factor=32

	memcpy(a_local, a_arr, (N * M * sizeof(data_type)));
	memcpy(b_local, b_arr, (M * P * sizeof(data_type)));

	matrix::multiply<data_type *, data_type *, data_type *, N, M, P, RD_PORTS>(a_local, b_local, c_local);

	memcpy(c_arr, c_local, (N * P * sizeof(data_type)));
#endif
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
	matrix::multiply<data_type *, data_type *, data_type *,
		N, M, P, RD_PORTS>(a_arr, b_arr, c_arr_ref);

#ifdef DEBUG
	std::cout << "A = " << std::endl;
	matrix::print(a_arr, N, M);
	std::cout << std::endl << "B = " << std::endl;
	matrix::print(b_arr, M, P);
	std::cout << std::endl << "C (reference) = " << std::endl;
	matrix::print(c_arr_ref, N, P);
	std::cout << std::endl << "C (cache) = " << std::endl;
	matrix::print(c_arr, N, P);
#endif /* DEBUG */

	if (!matrix::compare<data_type *>(c_arr, c_arr_ref, N, P)) {
		std::cerr << "Mismatch detected" << std::endl;
		return -1;
	}

	return 0;
}

