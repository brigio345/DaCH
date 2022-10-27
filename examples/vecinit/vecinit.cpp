#include <iostream>
#include "cache.h"

static const size_t N = 128;

typedef cache<int, false, true, 1, N, 1, 1, 8, false, 0, 0, false, 7> cache_t;

template <typename T>
	void vecinit(T &a) {
#pragma HLS inline
		for (int i = 0; i < N; i++) {
#pragma HLS pipeline
			a[i] = i;
		}
	}

extern "C" void vecinit_top(int a[N]) {
#pragma HLS INTERFACE m_axi port=a bundle=gmem0 depth=N
#pragma HLS INTERFACE ap_ctrl_hs port=return

#pragma HLS dataflow disable_start_propagation
	cache_t a_cache(a);

	cache_wrapper(vecinit<cache_t>, a_cache);
}

int main() {
	int a[N];
	int ret = 0;

	vecinit_top(a);

	for (int i = 0; i < N; i++) {
		std::cout << a[i] << " ";
		if (a[i] != i)
			ret = 1;
	}

	return ret;
}

