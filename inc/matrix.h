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

	template<typename T, typename U, typename V, size_t N, size_t M, size_t P>
		void multiply(T A, U B, V C) {
#pragma HLS inline
#if (defined(__PROFILE__) && (!defined(__SYNTHESIS__)))
			int n_l1_hit_a = 0;
			int n_hit_a = 0;
			int n_l1_hit_b = 0;
			int n_hit_b = 0;
			int n_hit_c = 0;
			int hit_status_a;
			int hit_status_b;
#endif /* (defined(__PROFILE__) && (!defined(__SYNTHESIS__))) */

MULT_I_LOOP:		for (int i = 0; i < N; i++) {
MULT_J_LOOP:			for (int j = 0; j < P; j++) {
					int acc = 0;
MULT_K_LOOP:				for (int k = 0; k < M; k++) {
#pragma HLS pipeline
#if (defined(__PROFILE__) && (!defined(__SYNTHESIS__)))
						acc += A.get(i * M + k, hit_status_a) * B.get(k * P + j, hit_status_b);

						if (hit_status_a == L1_HIT)
							n_l1_hit_a++;
						else if (hit_status_a == HIT)
							n_hit_a++;

						if (hit_status_b == L1_HIT)
							n_l1_hit_b++;
						else if (hit_status_b == HIT)
							n_hit_b++;
#else
						acc += A.get(i * M + k) * B.get(k * P + j);
#endif /* (defined(__PROFILE__) && (!defined(__SYNTHESIS__))) */
					}
#if (defined(__PROFILE__) && (!defined(__SYNTHESIS__)))
					C.set(i * P + j, acc, hit_status_a);

					if (hit_status_a == HIT)
						n_hit_c++;
#else
					C.set(i * P + j, acc);
#endif /* (defined(__PROFILE__) && (!defined(__SYNTHESIS__))) */
				}
			}

#if (defined(__PROFILE__) && (!defined(__SYNTHESIS__)))
			int n_reqs_ab = (N * M * P);
			int n_reqs_c = (N * P);
			printf("A hit ratio = %.3f ((%d + %d) / %d)\n",
					((n_hit_a + n_l1_hit_a) / ((float) n_reqs_ab)),
					n_l1_hit_a, n_hit_a, n_reqs_ab);
			printf("B hit ratio = %.3f ((%d + %d) / %d)\n",
					((n_hit_b + n_l1_hit_b) / ((float) n_reqs_ab)),
					n_l1_hit_b, n_hit_b, n_reqs_ab);
			printf("C hit ratio = %.3f (%d / %d)\n",
					(n_hit_c / ((float) n_reqs_c)),
					n_hit_c, n_reqs_c);
#endif /* (defined(__PROFILE__) && (!defined(__SYNTHESIS__))) */
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

