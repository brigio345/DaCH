#ifndef MATRIX_H
#define MATRIX_H

#include <cstdlib>
#include <iostream>

namespace matrix {
	template<typename T>
		void multiply(T A, T B, T C, unsigned short N,
				unsigned short M, unsigned short P) {
			for (int i = 0; i < N; i++) {
				for (int j = 0; j < P; j++) {
					int acc = 0;
					for (int k = 0; k < M; k++)
						acc += A[i * M + k] * B[k * P + j];
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

	void generateRandom(int A[], unsigned short N, unsigned short M) {
		for (int i = 0; i < N * M; i++)
			A[i] = std::rand() % 1000;
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

