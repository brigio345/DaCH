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

#define N_ROWS 16
#define N_COLS 16
#define N_KER_ROWS 2
#define N_KER_COLS 2
#define N_KER_ROWS_ROUND 2
#define N_KER_COLS_ROUND 2

static const size_t RD_PORTS = 1;

typedef cache<int, true, false, RD_PORTS, (N_ROWS * N_COLS), 1, 1, 16, false, 1, 16 / RD_PORTS, false, 6> cache_in;
typedef cache<int, true, false, RD_PORTS, (N_KER_ROWS_ROUND * N_KER_COLS_ROUND), 1, 1, N_KER_COLS, false, N_KER_ROWS, 1, false, 1> cache_ker;
typedef cache<int, false, true, 1, (N_ROWS * N_COLS), 1, 1, 32, false, 0, 0, false, 4> cache_out;

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
#pragma HLS unroll factor=RD_PORTS
						// index of input signal, used for checking boundary
						const auto ii = i + (kCenterY - m);
						const auto jj = j + (kCenterX - n);

						// ignore input samples which are out of bound
						if ((ii >= 0) && (ii < N_ROWS) && (jj >= 0) && (jj < N_COLS))
							tmp += (in[ii * N_COLS + jj] * kernel[m * N_KER_COLS + n]);
					}
				}
				out[i * N_COLS + j] = tmp;
			}
		}
	}

#ifdef STANDARD
template <>
	void convolution(cache_in &in, cache_ker &kernel, cache_out &out) {
#pragma HLS inline
		// find center position of kernel (half of kernel size)
		const auto kCenterX = N_KER_COLS / 2;
		const auto kCenterY = N_KER_ROWS / 2;

I_LOOP:		for (auto i = 0; i < N_ROWS; ++i) {
J_LOOP:			for (auto j = 0; j < N_COLS; ++j) {
				auto tmp = 0;
M_LOOP:				for (auto m = 0; m < N_KER_ROWS; ++m) {
N_LOOP:					for (auto n = 0; n < N_KER_COLS; n += RD_PORTS) {
#pragma HLS pipeline II=1
PORT_LOOP:					for (auto port = 0; port < RD_PORTS; port++) {
							// index of input signal, used for checking boundary
							const auto ii = i + (kCenterY - m);
							const auto jj = j + (kCenterX - (n + port));

							// ignore input samples which are out of bound
							if (((n + port) < N_KER_COLS) &&
									((ii >= 0) && (ii < N_ROWS) && (jj >= 0) && (jj < N_COLS))) {
								tmp += (in.get(ii * N_COLS + jj, port) *
										kernel.get(m * N_KER_COLS + (n + port), port));
							}
						}
						if ((m == (int)(N_KER_ROWS - 1)) && (n >= (int)(N_KER_COLS - RD_PORTS - 1)))
							out[i * N_COLS + j] = tmp;
					}
				}
			}
		}
	}
#else
template <>
	void convolution(cache_in &in, cache_ker &kernel, cache_out &out) {
#pragma HLS inline
		// find center position of kernel (half of kernel size)
		const auto kCenterX = N_KER_COLS / 2;
		const auto kCenterY = N_KER_ROWS / 2;

I_LOOP:		for (auto i = 0; i < N_ROWS; ++i) {
J_LOOP:			for (auto j = 0; j < N_COLS; ++j) {
				auto tmp = 0;
M_LOOP:				for (auto m = 0; m < N_KER_ROWS; m += RD_PORTS) {
N_LOOP:					for (auto n = 0; n < N_KER_COLS; ++n) {
#pragma HLS pipeline II=1
PORT_LOOP:					for (auto port = 0; port < RD_PORTS; port++) {
							// index of input signal, used for checking boundary
							const auto ii = i + (kCenterY - (m + port));
							const auto jj = j + (kCenterX - n);

							// ignore input samples which are out of bound
							if ((m + port < N_KER_ROWS) &&
									((ii >= 0) && (ii < N_ROWS) && (jj >= 0) && (jj < N_COLS))) {
								tmp += (in.get(ii * N_COLS + jj, port) *
										kernel.get((m + port) * N_KER_COLS + n, port));
							}
						}
						if ((m >= (int)(N_KER_ROWS - RD_PORTS - 1)) && (n == (N_KER_COLS - 1)))
							out[i * N_COLS + j] = tmp;
					}
				}
			}
		}
	}
#endif

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

void convolution_syn(cache_in &in, int *kernel, cache_out &out) {
#pragma HLS inline off
	in.init();
	out.init();

	convolution<cache_in &, int *, cache_out &>(in, kernel, out);

	in.stop();
	out.stop();
}

extern "C" void convolution_top(int in[N_ROWS * N_COLS],
		int kernel[N_KER_ROWS * N_KER_COLS], int out[N_ROWS * N_COLS]) {
#pragma HLS INTERFACE m_axi port=in offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=kernel offset=slave bundle=gmem1
#pragma HLS interface m_axi port=out offset=slave bundle=gmem2
#pragma HLS INTERFACE ap_ctrl_hs port=return

#if defined(CACHE)
#pragma HLS dataflow disable_start_propagation
	cache_in in_cache;
#ifndef KER_BUFF
	cache_ker ker_cache;
#endif
	cache_out out_cache;

#ifdef __SYNTHESIS__
	in_cache.run(in);
	out_cache.run(out);
#ifdef KER_BUFF
	convolution_syn(in_cache, kernel, out_cache);
#else
	ker_cache.run(kernel);
	convolution_syn(in_cache, ker_cache, out_cache);
#endif
#else
	in_cache.init();
	out_cache.init();

	std::thread in_cache_thread([&]{in_cache.run(in);});
	std::thread out_cache_thread([&]{out_cache.run(out);});
#ifdef KER_BUFF
	std::thread convolution_thread([&]{
			convolution<cache_in &, int *, cache_out &>(in_cache,
					kernel, out_cache);
			});
#else
	ker_cache.init();
	std::thread ker_cache_thread([&]{ker_cache.run(kernel);});

	std::thread convolution_thread([&]{
			convolution<cache_in &, cache_ker &, cache_out &>(in_cache,
					ker_cache, out_cache);
			});
#endif

	convolution_thread.join();

	in_cache.stop();
	out_cache.stop();
#ifndef KER_BUFF
	ker_cache.stop();
	ker_cache_thread.join();
#endif
	in_cache_thread.join();
	out_cache_thread.join();

#ifdef PROFILE
	printf("IN hit ratio = \n");
	for (auto port = 0; port < RD_PORTS; port++) {
		printf("\tP=%d: L1=%d/%d; L2=%d/%d\n", port,
				in_cache.get_n_l1_hits(port), in_cache.get_n_l1_reqs(port),
				in_cache.get_n_hits(port), in_cache.get_n_reqs(port));
	}
#ifndef KER_BUFF
	printf("Kernel hit ratio = \n");
	for (auto port = 0; port < RD_PORTS; port++) {
		printf("\tP=%d: L1=%d/%d; L2=%d/%d\n", port,
				ker_cache.get_n_l1_hits(port), ker_cache.get_n_l1_reqs(port),
				ker_cache.get_n_hits(port), ker_cache.get_n_reqs(port));
	}
#endif
	printf("Out hit ratio = L1=%d/%d; L2=%d/%d\n",
			out_cache.get_n_l1_hits(0), out_cache.get_n_l1_reqs(0),
			out_cache.get_n_hits(0), out_cache.get_n_reqs(0));
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
	int *in;
	int *kernel;
	int *out;
	int *out_ref;

	in = (int *)malloc(N_ROWS * N_COLS * sizeof(int));
	kernel = (int *)malloc(N_KER_ROWS * N_KER_COLS * sizeof(int));
	out = (int *)malloc(N_ROWS * N_COLS * sizeof(int));
	out_ref = (int *)malloc(N_ROWS * N_COLS * sizeof(int));

	for (auto i = 0; i < (N_ROWS * N_COLS); i++)
		in[i] = i;

	for (auto i = 0; i < (N_KER_ROWS * N_KER_COLS); i++)
		kernel[i] = i;

	convolution_top(in, kernel, out);
	convolution(in, kernel, out_ref);

	int ret = 0;
	for (auto i = 0; i < (N_ROWS * N_COLS); i++)
		if (out[i] != out_ref[i])
			ret++;

	free(in);
	free(kernel);
	free(out);
	free(out_ref);

	return ret;
}

