#ifndef MATRIX_H
#define MATRIX_H

#include "cache.h"
#include <cstdlib>
#include <iostream>

namespace matrix {
	template<size_t N, size_t M, size_t P>
		void multiply_ref(int *A, int *B, int *C) {
#pragma HLS inline
			for (int i = 0; i < N; i++) {
				for (int j = 0; j < P; j++) {
					int acc = 0;
					for (int k = 0; k < M; k++) {
#pragma HLS pipeline
						acc += A[i * M + k] * B[k * P + j];
					}
					C[i * P + j] = acc;
				}
			}
		}

	template<typename T, typename U, typename V, size_t N, size_t M, size_t P, size_t RD_PORTS>
		void multiply(T A, U B, V C) {
#pragma HLS inline
MULT_I_LOOP:		for (int i = 0; i < N; i++) {
MULT_J_LOOP:			for (int j = 0; j < P; j++) {
					int acc = 0;
MULT_K_LOOP:				for (int k = 0; k < M; k += RD_PORTS) {
#pragma HLS pipeline
						int prod[RD_PORTS];
						for (auto l = 0; l < RD_PORTS; l++) {
#pragma HLS unroll
							prod[l] = A.get(i * M + (k + l)) * B.get((k + l) * P + j);
						}

						for (auto l = 0; l < RD_PORTS; l++) {
#pragma HLS unroll
							acc += prod[l];
						}
#ifdef DEBUG
						std::cout << "a[" << (i * M + k) << "]=" << a << std::endl;
						std::cout << "b[" << (k * P + j) << "]=" << b << std::endl;
#endif /* DEBUG */
					}
					C.set(i * P + j, acc);
				}
			}

#if (defined(PROFILE) && (!defined(__SYNTHESIS__)))
			printf("A hit ratio = %.3f (%d / %d)\n",
					A.get_hit_ratio(), A.get_n_hits(),
					A.get_n_reqs());
			printf("B hit ratio = %.3f (%d / %d)\n",
					B.get_hit_ratio(), B.get_n_hits(),
					B.get_n_reqs());
			printf("C hit ratio = %.3f ((%d + %d) / %d)\n",
					C.get_hit_ratio(), C.get_n_l1_hits(),
					C.get_n_hits(), C.get_n_reqs());
#endif /* (defined(PROFILE) && (!defined(__SYNTHESIS__))) */
		}

	template<typename T>
		bool compare(const T A, const T B,
				unsigned short N, unsigned short M) {
			for (int i = 0; i < N * M; i++)
				if (A[i] != B[i])
					return false;
			return true;
		}

	template<typename T>
		void generate_random(T A[], unsigned short N, unsigned short M) {
			for (int i = 0; i < N * M; i++)
				A[i] = (T)(std::rand() % 1000);
		}

	template<typename T>
		void print(T A[], unsigned short N, unsigned short M) {
			for (int i = 0; i < N; i++) {
				for (int j = 0; j < M; j++)
					std::cout << A[i * M + j] << " ";
				std::cout << std::endl;
			}
		}
}

#endif /* MATRIX_H */

