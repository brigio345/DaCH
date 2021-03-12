#include <thread>
#include "matrix.h"
#include "cache.h"

extern "C" void top(int * const arrA, int * const arrB, int * const arrC,
		const unsigned short N, const unsigned short M, const unsigned short P) {
	#pragma HLS DATAFLOW
	cache<int> cacheA(arrA);
	cache<int> cacheB(arrB);
	cache<int> cacheC(arrC);

#ifdef __SYNTHESIS__
	matrix::multiply<cache<int> &>(cacheA, cacheB, cacheC, N, M, P);
	cacheA.read();
	cacheB.read();
	cacheC.write();
#else
	std::thread AThread(&cache<int>::read, std::ref(cacheA));
	std::thread BThread(&cache<int>::read, std::ref(cacheB));
	std::thread CThread(&cache<int>::write, std::ref(cacheC));
	std::thread matmulThread(&matrix::multiply<cache<int> &>,
			std::ref(cacheA), std::ref(cacheB), std::ref(cacheC), N, M, P);

	matmulThread.join();

	cacheA.stopRead();
	cacheB.stopRead();
	cacheC.stopWrite();

	AThread.join();
	BThread.join();
	CThread.join();
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

	std::cout << "A = " << std::endl;
	matrix::print(arrA, N, M);
	std::cout << std::endl << "B = " << std::endl;
	matrix::print(arrB, M, P);
	std::cout << std::endl << "C (reference) = " << std::endl;
	matrix::print(arrCRef, N, P);
	std::cout << std::endl << "C (cache) = " << std::endl;
	matrix::print(arrC, N, P);

	if (!matrix::compare<int[]>(arrC, arrCRef, N, P)) {
		std::cerr << "Mismatch detected" << std::endl;
		return -1;
	}

	return 0;
}

