#include <iostream>
#include <cstring>
#ifndef __SYNTHESIS__
#define PROFILE
#include <thread>
#endif	/* __SYNTHESIS__ */
#include "cache.h"

#define CACHE
//#define GLOBAL
//#define LOCAL

//#define NAIVE
#define STANDARD

#define N 16
#define M 16
#define P 16

static const int RD_PORTS = 1; 
static const int BLOCKS = 2;
typedef int data_type;
typedef cache<data_type, true, false, RD_PORTS, N * M, 1, 1, BLOCKS, false, 0, 0, false, 7> cache_a;
typedef cache<data_type, true, false, 1, M * P, 1, BLOCKS, BLOCKS, false, 0, 0, false, 7> cache_b;
typedef cache<data_type, true, true, 1, N * P, 1, RD_PORTS, BLOCKS, false, 0, 0, false, 7> cache_c;

template<typename T, typename U, typename V>
void multiply(T A, U B, V C) {
#pragma HLS inline
MULT_I_LOOP:		for (int i = 0; i < N; i++) {
MULT_J_LOOP:			for (int j = 0; j < P; j++) {
					int acc = 0;
MULT_K_LOOP:				for (int k = 0; k < M; k++) {
#pragma HLS pipeline II=1
#pragma HLS unroll factor=RD_PORTS
						acc += A[i * M + k] * B[k * P + j];
						if (k == (M - 1))
							C[i * P + j] = acc;
					}
				}
			}
}

#ifdef NAIVE
#ifdef STANDARD
// Naive Standard
template<>
void multiply(cache_a &A, cache_b &B, cache_c &C) {
#pragma HLS inline
MULT_I_LOOP:for (int i = 0; i < N; i++) {
MULT_J_LOOP:	for (int j = 0; j < P; j++) {
			int acc = 0;
MULT_K_LOOP:		for (int k = 0; k < M; k += RD_PORTS) {
#pragma HLS pipeline II=1
PORT_LOOP:			for (auto port = 0; port < RD_PORTS; port++) {
#pragma HLS unroll
					acc += A.get(i * M + (k + port), port) * B.get((k + port) * P + j, port);
				}
			}
			C[i * P + j] = acc;
		}
	    }
}
#else
// Naive Optimized
template<>
void multiply(cache_a &A, cache_b &B, cache_c &C) {
#pragma HLS inline
MULT_I_LOOP:for (int i = 0; i < N; i += RD_PORTS) {
MULT_J_LOOP:	for (int j = 0; j < P; j++) {
			int acc[RD_PORTS] = {0};
MULT_K_LOOP:		for (int k = 0; k < M; k++) {
#pragma HLS pipeline II=1
				const int b = B[k * P + j];
MULTIPLY_LOOP:			for (auto port = 0; port < RD_PORTS; port++)
					acc[port] += A.get((i + port) * M + k, port) * b;
			}

STORE_LOOP:		for (auto port = 0; port < RD_PORTS; port++) {
#pragma HLS unroll
				C[(i + port) * P + j] = acc[port];
			}
		}
	}
}
#endif /* STANDARD */
#else
#ifdef STANDARD
// Block Standard
template<>
void multiply(cache_a &A, cache_b &B, cache_c &C) {
#pragma HLS inline
JJ_LOOP:for (int jj = 0; jj < P; jj += BLOCKS) {
KK_LOOP:	for (int kk = 0; kk < M; kk += BLOCKS) {
I_LOOP:			for (int i = 0; i < N; i++) {
J_LOOP:				for (int j = jj; j < (jj + BLOCKS); j++) {
					int acc = 0;
K_LOOP:					for (int k = kk; k < (kk + BLOCKS); k += RD_PORTS) {
#pragma HLS pipeline II=1
						for (int port = 0; port < RD_PORTS; port++) {
#pragma HLS unroll
							acc += A.get(i * M + k + port, port) *
								B.get((k + port) * P + j, port);
						}
					}
					int tmp = C[i * P + j];
					C[i * P + j] = tmp + acc;
				}
			}
		}
	}
}
#else
// Block Optimized
template<>
void multiply(cache_a &A, cache_b &B, cache_c &C) {
#pragma HLS inline
JJ_LOOP:for (int jj = 0; jj < P; jj += BLOCKS) {
KK_LOOP:	for (int kk = 0; kk < M; kk += BLOCKS) {
I_LOOP:			for (int i = 0; i < N; i += RD_PORTS) {
J_LOOP:				for (int j = jj; j < (jj + BLOCKS); j++) {
					int acc[RD_PORTS] = {0};
					int tmp[RD_PORTS];
LOAD_C_LOOP:				for (auto port = 0; port < RD_PORTS; port++) {
#pragma HLS unroll
						tmp[port] = C[(i + port) * P + j];
					}

K_LOOP:					for (int k = kk; k < (kk + BLOCKS); k++) {
#pragma HLS pipeline II=1
						const int b = B[k * P + j];
MULT_LOOP:					for (int port = 0; port < RD_PORTS; port++)
							acc[port] += A.get((i + port) * M + k, port) * b;
					}

STORE_C_LOOP:				for (auto port = 0; port < RD_PORTS; port++) {
#pragma HLS unroll
						C[(i + port) * P + j] = tmp[port] + acc[port];
					}
				}
			}
		}
	}
}
#endif /* STANDARD */
#endif /* NAIVE */

void multiply_syn(cache_a &a_cache, cache_b &b_cache, cache_c &c_cache) {
#pragma HLS inline off
	a_cache.init();
	b_cache.init();
	c_cache.init();

	multiply<cache_a &, cache_b &, cache_c &> (a_cache, b_cache, c_cache);

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
	std::thread matmul_thread([&]{multiply<cache_a &, cache_b &, cache_c &>
			(a_cache, b_cache, c_cache);});

	matmul_thread.join();

	a_cache.stop();
	b_cache.stop();
	c_cache.stop();
	a_thd.join();
	b_thd.join();
	c_thd.join();

#ifdef PROFILE
	printf("A hit ratio = \n");
	for (auto port = 0; port < RD_PORTS; port++) {
		printf("\tP=%d: L1=%d/%d; L2=%d/%d\n", port,
				a_cache.get_n_l1_hits(port), a_cache.get_n_l1_reqs(port),
				a_cache.get_n_hits(port), a_cache.get_n_reqs(port));
	}
	printf("B hit ratio = L1=%d/%d; L2=%d/%d\n",
			b_cache.get_n_l1_hits(0), b_cache.get_n_l1_reqs(0),
			b_cache.get_n_hits(0), b_cache.get_n_reqs(0));
	printf("C hit ratio = L1=%d/%d; L2=%d/%d\n",
			c_cache.get_n_l1_hits(0), c_cache.get_n_l1_reqs(0),
			c_cache.get_n_hits(0), c_cache.get_n_reqs(0));
#endif /* PROFILE */
#endif	/* __SYNTHESIS__ */
#elif defined(GLOBAL)
	multiply<data_type *, data_type *, data_type *>(a_arr, b_arr, c_arr);
#elif defined(LOCAL)
	data_type a_local[N * M];
	data_type b_local[M * P];
	data_type c_local[N * P];
#pragma HLS array_reshape variable=a_local cyclic factor=32
#pragma HLS array_reshape variable=b_local cyclic factor=32
#pragma HLS array_reshape variable=c_local cyclic factor=32

	memcpy(a_local, a_arr, (N * M * sizeof(data_type)));
	memcpy(b_local, b_arr, (M * P * sizeof(data_type)));

	multiply<data_type *, data_type *, data_type *>(a_local, b_local, c_local);

	memcpy(c_arr, c_local, (N * P * sizeof(data_type)));
#endif
}

int main() {
	data_type a_arr[N * M];
	data_type b_arr[M * P];
	data_type c_arr[N * P] = {0};
	data_type c_arr_ref[N * P] = {0};

	for (auto i = 0; i < N * M; i++)
		a_arr[i] = i;
	for (auto i = 0; i < M * P; i++)
		b_arr[i] = i;

	// matrix multiplication with caches
	matmul_top(a_arr, b_arr, c_arr);
	// standard matrix multiplication
	multiply<data_type *, data_type *, data_type *>(a_arr, b_arr, c_arr_ref);

	auto err = 0;
	for (auto i = 0; i < N * P; i++) {
		if (c_arr[i] != c_arr_ref[i])
			err++;
	}

	return err;
}

