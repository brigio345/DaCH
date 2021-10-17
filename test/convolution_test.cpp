#include <iostream>
#include <cstring>
#ifndef __SYNTHESIS__
#include <thread>
#endif	/* __SYNTHESIS__ */
#define PROFILE
#include "cache.h"

#define CACHE
//#define GLOBAL
//#define LOCAL

#define N_ROWS 32
#define N_COLS 32
#define N_KER_ROWS 3
#define N_KER_COLS 3
#define N_KER_ROWS_ROUND 4
#define N_KER_COLS_ROUND 4

static const size_t RD_PORTS = 1;

typedef cache<int, true, false, RD_PORTS, (N_ROWS * N_COLS), 1, 4, 32, false, 0, 9> cache_in;
typedef cache<int, true, false, RD_PORTS, (N_KER_ROWS_ROUND * N_KER_COLS_ROUND), 1, 1, 16, false, 1, 9> cache_ker;
typedef cache<int, false, true, 1, (N_ROWS * N_COLS), 1, 1, 32, false, 0, 1> cache_out;

template <typename T, typename U, typename V>
	void convolution(T in, U kernel, V out) {
#pragma HLS inline
		// find center position of kernel (half of kernel size)
		const auto kCenterX = N_KER_COLS / 2;
		const auto kCenterY = N_KER_ROWS / 2;

I_LOOP:		for (auto i = 0; i < N_ROWS; ++i) {
J_LOOP:			for (auto j = 0; j < N_COLS; ++j) {
				auto tmp = 0;
M_LOOP:				for (auto m = 0; m < N_KER_ROWS; ++m) {
N_LOOP:					for (auto n = 0; n < N_KER_COLS; ++n) {
#pragma HLS pipeline II=1
						// index of input signal, used for checking boundary
						const auto ii = i + (kCenterY - m);
						const auto jj = j + (kCenterX - n);

						// ignore input samples which are out of bound
						if ((ii >= 0) && (ii < N_ROWS) && (jj >= 0) && (jj < N_COLS))
							tmp += (in[ii * N_COLS + jj] * kernel[m * N_KER_COLS + n]);

						if (m == (N_KER_ROWS - 1) && (n == (N_KER_COLS - 1)))
							out[i * N_COLS + j] = tmp;
					}
				}
			}
		}
	}

void convolution_syn(cache_in &in, cache_ker &kernel, cache_out &out) {
#pragma HLS inline off
	in.init();
	kernel.init();
	out.init();

	convolution<cache_in &, cache_ker &, cache_out &>(in, kernel, out);

	in.stop();
	kernel.stop();
	out.stop();
}

extern "C" void convolution_top(int in[N_ROWS * N_COLS],
		int kernel[N_KER_ROWS_ROUND * N_KER_COLS_ROUND], int out[N_ROWS * N_COLS]) {
#pragma HLS INTERFACE m_axi port=in bundle=gmem0
#pragma HLS INTERFACE m_axi port=out bundle=gmem1
#pragma HLS interface m_axi port=kernel bundle=gmem2

#if defined(CACHE)
#pragma HLS dataflow disable_start_propagation
	cache_in in_cache;
	cache_ker ker_cache;
	cache_out out_cache;
#pragma HLS disaggregate variable=in_cache
#pragma HLS disaggregate variable=ker_cache
#pragma HLS disaggregate variable=out_cache

#ifdef __SYNTHESIS__
	in_cache.run(in);
	ker_cache.run(kernel);
	out_cache.run(out);
	convolution_syn(in_cache, ker_cache, out_cache);
#else
	in_cache.init();
	ker_cache.init();
	out_cache.init();

	std::thread in_cache_thread([&]{in_cache.run(in);});
	std::thread ker_cache_thread([&]{ker_cache.run(kernel);});
	std::thread out_cache_thread([&]{out_cache.run(out);});
	std::thread convolution_thread([&]{
			convolution<cache_in &, cache_ker &, cache_out &>(in_cache,
					ker_cache, out_cache);
			});

	convolution_thread.join();

	in_cache.stop();
	ker_cache.stop();
	out_cache.stop();
	in_cache_thread.join();
	ker_cache_thread.join();
	out_cache_thread.join();

#ifdef PROFILE
	printf("in hit ratio = %.3f ((%d + %d) / %d)\n",
			in_cache.get_hit_ratio(), in_cache.get_n_l1_hits(),
			in_cache.get_n_hits(), in_cache.get_n_reqs());
	printf("kernel hit ratio = %.3f ((%d + %d) / %d)\n",
			ker_cache.get_hit_ratio(), ker_cache.get_n_l1_hits(),
			ker_cache.get_n_hits(), ker_cache.get_n_reqs());
	printf("out hit ratio = %.3f (%d / %d)\n",
			out_cache.get_hit_ratio(), out_cache.get_n_hits(),
			out_cache.get_n_reqs());
#endif /* PROFILE */
#endif	/* __SYNTHESIS__ */
#elif defined(GLOBAL)
	convolution(in, kernel, out);
#elif defined(LOCAL)
	int in_local[N_ROWS * N_COLS];
	int kernel_local[N_KER_ROWS * N_KER_COLS];
	int out_local [N_ROWS * N_COLS];
#pragma HLS array_reshape variable=in_local cyclic factor=32
#pragma HLS array_reshape variable=kernel_local complete
#pragma HLS array_reshape variable=out_local cyclic factor=32

	memcpy(in_local, in, (N_ROWS * N_COLS * sizeof(int)));
	memcpy(kernel_local, kernel, (N_KER_ROWS * N_KER_COLS * sizeof(int)));

	convolution(in_local, kernel_local, out_local);

	memcpy(out, out_local, (N_ROWS * N_COLS * sizeof(int)));
#endif /* CACHE */
}

int main() {
	int in[N_ROWS * N_COLS];
	int kernel[N_KER_ROWS_ROUND * N_KER_COLS_ROUND];
	int out[N_ROWS * N_COLS];
	int out_ref[N_ROWS * N_COLS];

	for (auto i = 0; i < (N_ROWS * N_COLS); i++)
		in[i] = i;

	for (auto i = 0; i < (N_KER_ROWS * N_KER_COLS); i++)
		kernel[i] = i;

	convolution_top(in, kernel, out);
	convolution(in, kernel, out_ref);

	for (auto i = 0; i < (N_ROWS * N_COLS); i++)
		if (out[i] != out_ref[i])
			return 1;

	return 0;
}

