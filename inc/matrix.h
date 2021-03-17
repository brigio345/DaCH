#ifndef MATRIX_H
#define MATRIX_H

#include <cstdlib>
#include <iostream>

namespace matrix {
	template<typename T, size_t N, size_t M, size_t P, size_t N_PORTS>
		void multiply(T A, T B, T C) {
			for (int i = 0; i < N; i++) {
				for (int j = 0; j < P; j++) {
					int acc = 0;
					for (int k = 0; k < M; k++) {
#pragma HLS pipeline
#pragma HLS unroll factor=N_PORTS
						acc += A[i * M + k] * B[k * P + j];
					}
					C[i * P + j] = acc;
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

