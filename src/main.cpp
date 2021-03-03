#include <thread>
#include "matrix.h"
#include "streamedArray.h"
#include "cache.h"

void top(int arrA[], int arrB[], int arrC[], const unsigned short N,
		const unsigned short M, const unsigned short P) {
	streamedArray<int> A(arrA), B(arrB), C(arrC);
	cache<int> cacheA(A);
	cache<int> cacheB(B);
	cache<int> cacheC(C);

	//#define __SYNTHESIS__
#ifdef __SYNTHESIS__
#pragma HLS dataflow
#pragma HLS interface ap_ctrl_none port=return
	matrix::multiply<streamedArray<int> &>(A, B, C, N, M, P);
	cacheA.read();
	cacheB.read();
	cacheC.write();
#else
	std::thread AThread(&cache<int>::read, cacheA);
	std::thread BThread(&cache<int>::read, cacheB);
	std::thread CThread(&cache<int>::write, cacheC);
	std::thread matmulThread(matrix::multiply<streamedArray<int> &>,
			std::ref(A), std::ref(B), std::ref(C), N, M, P);

	AThread.detach();
	BThread.detach();
	CThread.detach();
	matmulThread.join(); // when matmul returns, kill everything
#endif	/* __SYNTHESIS__ */
}

int main() {
	const unsigned short N = 5;
	const unsigned short M = 4;
	const unsigned short P = 3;
	int arrA[N * M];
	int arrB[M * P];
	int arrC[N * P];
	int arrCRef[N * P];

	matrix::generateRandom(arrA, N, M);
	matrix::generateRandom(arrB, M, P);

	// standard matrix multiplication
	matrix::multiply<int[]>(arrA, arrB, arrCRef, N, M, P);
	// matrix multiplication with caches
	top(arrA, arrB, arrC, N, M, P);

	if (!matrix::compare<int[]>(arrC, arrCRef, N, P)) {
		std::cerr << "Mismatch detected" << std::endl;
		return -1;
	}

	return 0;
}

