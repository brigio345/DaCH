#ifndef MATRIX_H
#define MATRIX_H

#include "cache.h"
#include <cstdlib>
#include <iostream>

namespace matrix {
	template<typename T, typename U, typename V, size_t N, size_t M, size_t P, size_t RD_PORTS>
		void multiply(T A, U B, V C) {
#pragma HLS inline
MULT_I_LOOP:		for (int i = 0; i < N; i++) {
MULT_J_LOOP:			for (int j = 0; j < P; j++) {
					int acc = 0;
MULT_K_LOOP:				for (int k = 0; k < M; k++) {
#pragma HLS pipeline
#pragma HLS unroll factor=RD_PORTS
						acc += A[i * M + k] * B[k * P + j];
						if (k == (M - 1))
							C[i * P + j] = acc;
					}
				}
			}
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
				A[i] = static_cast<T>(std::rand() % 1000);
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

