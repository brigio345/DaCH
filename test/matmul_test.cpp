#include <iostream>
#include <cstring>
#define PROFILE
#include "cache.h"

#define CACHE
//#define GLOBAL
//#define MANUAL

//#define STANDARD
//#define HORIZONTAL

#define N 32
#define M 32
#define P 32

static const int RD_PORTS = 2;

#define BLK 16

#ifndef BLOCKS
#define BLOCKS 16
#endif /* BLOCKS */

#ifndef A_WORDS
#define A_WORDS BLK
#endif /* A_WORDS */
#ifndef A_L2_SETS
#define A_L2_SETS 1
#endif /* A_L2_SETS */
#ifndef A_L2_WAYS
#define A_L2_WAYS 1
#endif /* A_L2_WAYS */
#ifndef A_L1_SETS
#define A_L1_SETS 1
#endif /* A_L1_SETS */
#ifndef A_L1_WAYS
#define A_L1_WAYS 1
#endif /* A_L1_WAYS */
#ifndef A_L2_LATENCY
#define A_L2_LATENCY 3
#endif /* A_L2_LATENCY */

#ifndef B_WORDS
#define B_WORDS BLK
#endif /* B_WORDS */
#ifndef B_L2_SETS
#define B_L2_SETS 1
#endif /* B_L2_SETS */
#ifndef B_L2_WAYS
#define B_L2_WAYS 1
#endif /* B_L2_WAYS */
#ifndef B_L1_SETS
#define B_L1_SETS 1
#endif /* B_L1_SETS */
#ifndef B_L1_WAYS
#define B_L1_WAYS BLK
#endif /* B_L1_WAYS */
#ifndef B_L2_LATENCY
#define B_L2_LATENCY 3
#endif /* B_L2_LATENCY */

#ifndef C_WORDS
#define C_WORDS BLK
#endif /* C_WORDS */
#ifndef C_L2_SETS
#define C_L2_SETS 1
#endif /* C_L2_SETS */
#ifndef C_L2_WAYS
#define C_L2_WAYS BLK
#endif /* C_L2_WAYS */
#ifndef C_L1_SETS
#define C_L1_SETS 0
#endif /* C_L1_SETS */
#ifndef C_L1_WAYS
#define C_L1_WAYS 0
#endif /* C_L1_WAYS */
#ifndef C_L2_LATENCY
#define C_L2_LATENCY 3
#endif /* C_L2_LATENCY */

#ifdef HORIZONTAL
#define C_RD_ENABLED false
#else
#define C_RD_ENABLED true
#endif /* HORIZONTAL */

typedef int data_type;
typedef cache<data_type, true, false, RD_PORTS, N * M, A_L2_SETS, A_L2_WAYS,
	A_WORDS, false, A_L1_SETS, A_L1_WAYS, false, A_L2_LATENCY> cache_a;
typedef cache<data_type, true, false, 1, M * P, B_L2_SETS, B_L2_WAYS, B_WORDS,
	false, B_L1_SETS, B_L1_WAYS, true, B_L2_LATENCY> cache_b;
typedef cache<data_type, C_RD_ENABLED, true, 1, N * P, C_L2_SETS, C_L2_WAYS,
	C_WORDS, false, C_L1_SETS, C_L1_WAYS, false, C_L2_LATENCY> cache_c;

template<typename T, typename U, typename V>
void multiply(T &A, U &B, V &C) {
#pragma HLS inline
MULT_I_LOOP:for (int i = 0; i < N; i++) {
MULT_J_LOOP:	for (int j = 0; j < P; j++) {
			int acc = 0;
MULT_K_LOOP:		for (int k = 0; k < M; k++) {
#pragma HLS pipeline II=1
#pragma HLS unroll factor=RD_PORTS
				acc += A[i * M + k] * B[k * P + j];
				if (k == (M - 1))
					C[i * P + j] = acc;
			}
		}
	}
}

#ifdef STANDARD
#ifdef HORIZONTAL
// Standard Horizontal
template<>
void multiply(cache_a &A, cache_b &B, cache_c &C) {
#pragma HLS inline
MULT_I_LOOP:for (int i = 0; i < N; i++) {
MULT_J_LOOP:	for (int j = 0; j < P; j++) {
			data_type acc = 0;
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
// Standard Tiled
template<>
void multiply(cache_a &A, cache_b &B, cache_c &C) {
#pragma HLS inline
MULT_I_LOOP:for (int i = 0; i < N; i += RD_PORTS) {
MULT_J_LOOP:	for (int j = 0; j < P; j++) {
			data_type acc[RD_PORTS] = {0};
MULT_K_LOOP:		for (int k = 0; k < M; k++) {
#pragma HLS pipeline II=1
				const data_type b = B[k * P + j];
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
#endif /* HORIZONTAL */
#else
#ifdef HORIZONTAL
// Block Horizontal
template<>
void multiply(cache_a &A, cache_b &B, cache_c &C) {
#pragma HLS inline
II_LOOP:for (int ii = 0; ii < P; ii += BLOCKS) {
KK_LOOP:	for (int kk = 0; kk < M; kk += BLOCKS) {
J_LOOP:			for (int j = 0; j < N; j++) {
I_LOOP:				for (int i = ii; i < (ii + BLOCKS); i++) {
					data_type acc = 0;
K_LOOP:					for (int k = kk; k < (kk + BLOCKS); k += RD_PORTS) {
#pragma HLS pipeline II=1
						for (int port = 0; port < RD_PORTS; port++) {
#pragma HLS unroll
							acc += A.get(i * M + k + port, port) *
								B.get((k + port) * P + j, port);
						}
					}
					data_type tmp = C[i * P + j];
					C[i * P + j] = tmp + acc;
				}
			}
		}
	}
}
#else
// Block Tiled
template<>
void multiply(cache_a &A, cache_b &B, cache_c &C) {
#pragma HLS inline
II_LOOP:for (int ii = 0; ii < N; ii += BLK) {
KK_LOOP:	for (int kk = 0; kk < M; kk += BLK) {
J_LOOP:			for (int j = 0; j < P; j++) {
I_LOOP:				for (int i = ii; i < (ii + BLK); i += RD_PORTS) {
					data_type acc[RD_PORTS] = {0};
					data_type tmp[RD_PORTS];
LOAD_C_LOOP:				for (auto port = 0; port < RD_PORTS; port++) {
#pragma HLS unroll
						tmp[port] = C[(i + port) * P + j];
					}

K_LOOP:					for (int k = kk; k < (kk + BLK); k++) {
#pragma HLS pipeline II=1
						const data_type b = B[k * P + j];
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
#endif /* HORIZONTAL */
#endif /* STANDARD */

#define MAX_AXI_BITWIDTH 128
#define DATA_BITWIDTH (8 * sizeof(data_type))
#define MAX_ELEMENTS_PER_AXI_REQ ((MAX_AXI_BITWIDTH) / (DATA_BITWIDTH))

struct buffer_a_c {
	data_type buff[MAX_ELEMENTS_PER_AXI_REQ];
};

struct buffer_b {
	data_type buff[M];
};

void load_a(data_type *A, hls::stream<buffer_a_c> &a_stream) {
	buffer_a_c a_row;
LD_A_J:	for (auto j = 0; j < P; j += MAX_ELEMENTS_PER_AXI_REQ) {
LD_A_I:		for (auto i = 0; i < N; i++) {
LD_A_K:			for (auto k = 0; k < M; k += MAX_ELEMENTS_PER_AXI_REQ) {
#pragma HLS pipeline II=1
LD_A_L:				for (auto l = 0; l < MAX_ELEMENTS_PER_AXI_REQ; l++) {
#pragma HLS unroll
					a_row.buff[l] = A[i * M + k + l];
				}
				a_stream.write(a_row);
			}
		}
	}
}

void load_b(data_type *B, hls::stream<buffer_b> b_stream[MAX_ELEMENTS_PER_AXI_REQ]) {
	buffer_b b_col[MAX_ELEMENTS_PER_AXI_REQ];
LD_B_J:	for (auto j = 0; j < P; j += MAX_ELEMENTS_PER_AXI_REQ) {
LD_B_K:		for (auto k = 0; k < M; k++) {
#pragma HLS pipeline II=1
LD_B_L0:		for (auto l = 0; l < MAX_ELEMENTS_PER_AXI_REQ; l++) {
				b_col[l].buff[k] = B[k * P + j + l];
			}

			if (k == (M - 1)) {
LD_B_L1:			for (auto l = 0; l < MAX_ELEMENTS_PER_AXI_REQ; l++) {
					b_stream[l].write(b_col[l]);
				}
			}
		}
	}
}

void store(data_type *C, hls::stream<buffer_a_c> &c_stream) {
STR_C_J:for (auto j = 0; j < P; j += MAX_ELEMENTS_PER_AXI_REQ) {
STR_C_I:	for (auto i = 0; i < N; i++) {
#pragma HLS pipeline II=1
			const buffer_a_c buff = c_stream.read();
STR_C_L:		for (auto l = 0; l < MAX_ELEMENTS_PER_AXI_REQ; l++) {
				C[i * P + j + l] = buff.buff[l];
			}
		}
	}
}

void compute(hls::stream<buffer_a_c> &a_stream, hls::stream<buffer_b> b_stream[MAX_ELEMENTS_PER_AXI_REQ], hls::stream<buffer_a_c> &c_stream) {
	buffer_b b_col[MAX_ELEMENTS_PER_AXI_REQ];
COMP_J:	for (auto j = 0; j < P; j += MAX_ELEMENTS_PER_AXI_REQ) {
COMP_I:		for (auto i = 0; i < N; i++) {
			buffer_a_c c_buff = {.buff = {0}};
COMP_K:			for (int k = 0; k < M; k += MAX_ELEMENTS_PER_AXI_REQ) {
#pragma HLS pipeline II=1
				if (k == 0 && i == 0) {
COMP_L0:				for (auto l = 0; l < MAX_ELEMENTS_PER_AXI_REQ; l++) {
						b_col[l] = b_stream[l].read();
					}
				}

				buffer_a_c a_row = a_stream.read();
COMP_L1:			for (auto l = 0; l < MAX_ELEMENTS_PER_AXI_REQ; l++) {
COMP_M:					for (auto m = 0; m < MAX_ELEMENTS_PER_AXI_REQ; m++) {
						c_buff.buff[l] += a_row.buff[m] * b_col[l].buff[k + m];
					}
				}

				if ((k + MAX_ELEMENTS_PER_AXI_REQ) >= M) {
					c_stream.write(c_buff);
				}
			}
		}
	}
}

void multiply_buffer(data_type a_arr[N * M], data_type b_arr[M * P], data_type c_arr[N * P]) {
#pragma HLS dataflow
	hls::stream<buffer_a_c, 128> a_stream;
	hls::stream<buffer_b, 6> b_stream[MAX_ELEMENTS_PER_AXI_REQ];
	hls::stream<buffer_a_c, 2> c_stream;

	load_a(a_arr, a_stream);
	load_b(b_arr, b_stream);
	compute(a_stream, b_stream, c_stream);
	store(c_arr, c_stream);
}

void multiply_syn(cache_a &a_cache, cache_b &b_cache, cache_c &c_cache) {
#pragma HLS inline off
	a_cache.init();
	b_cache.init();
	c_cache.init();

	multiply(a_cache, b_cache, c_cache);

	a_cache.stop();
	b_cache.stop();
	c_cache.stop();
}

extern "C" void matmul_top(data_type a_arr[N * M], data_type b_arr[M * P], data_type c_arr[N * P]) {
#pragma HLS INTERFACE m_axi port=a_arr offset=slave bundle=gmem0 latency=0 depth=1024
#pragma HLS INTERFACE m_axi port=b_arr offset=slave bundle=gmem1 latency=0 depth=1024
#pragma HLS INTERFACE m_axi port=c_arr offset=slave bundle=gmem2 latency=0 depth=1024
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

	a_cache.run(a_arr);
	b_cache.run(b_arr);
	c_cache.run(c_arr);

	multiply(a_cache, b_cache, c_cache);

	a_cache.stop();
	b_cache.stop();
	c_cache.stop();

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
	multiply(a_arr, b_arr, c_arr);
#elif defined(MANUAL)
	multiply_buffer(a_arr, b_arr, c_arr);
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
	multiply(a_arr, b_arr, c_arr_ref);

	int err = 0;
	for (auto i = 0; i < N * P; i++) {
		if (c_arr[i] != c_arr_ref[i]) {
			err++;
			printf("Mismatch: %d %d\n", c_arr[i], c_arr_ref[i]);
		}
	}

	return err;
}

