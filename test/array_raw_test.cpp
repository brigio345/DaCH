#include <iostream>
#ifndef __SYNTHESIS__
#include <thread>
#endif	/* __SYNTHESIS__ */
#include "cache.h"

#define USE_CACHE
#define N 1024

typedef cache<int, 1, true, N, 2, 1, 16, false, true> cache_type;

template <typename T>
	void array_raw(T a) {
#pragma HLS inline
RAW_LOOP:	for (auto i = 2; i < N; i++) {
#pragma HLS pipeline
			a[i - 2] = a[i];
		}
	}

void array_raw_syn(cache_type &a) {
#pragma HLS inline off
	a.init();

	array_raw<cache_type &>(a);

	a.stop();
}

extern "C" void array_raw_top(int a[N]) {
#pragma HLS INTERFACE m_axi port=a bundle=gmem0 latency=1

#ifdef USE_CACHE
#pragma HLS dataflow disable_start_propagation
	cache_type a_cache;

#ifdef __SYNTHESIS__
	a_cache.run(a);
	array_raw_syn(a_cache);
#else
	a_cache.init();

	std::thread a_cache_thread([&]{a_cache.run(a);});
	std::thread array_raw_thread([&]{
			array_raw<cache_type &>(a_cache);
			});

	array_raw_thread.join();

	a_cache.stop();
	a_cache_thread.join();
#endif	/* __SYNTHESIS__ */
#else
	array_raw<int *>(a);
#endif
}

int main() {
	int a[N];
	int a_ref[N];

	for (auto i = 0; i < N; i++) {
		a[i] = i;
		a_ref[i] = i;
	}
	array_raw_top(a);
	array_raw<int *>(a_ref);

	std::cout << "a= ";
	for (auto i = 0; i < N; i++)
		std::cout << a[i] << " ";
	std::cout << "\na_ref= ";
	for (auto i = 0; i < N; i++)
		std::cout << a_ref[i] << " ";
	std::cout << std::endl;

	for (auto i = 0; i < N; i++) {
		if (a[i] != a_ref[i])
			return 1;
	}

	return 0;
}

