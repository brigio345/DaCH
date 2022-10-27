#include <iostream>
#include "cache.h"

static const int N = 64;

typedef cache<int, true, true, 1, N, 2, 1, 8, true, 0, 0, false, 2> cache_t;

template <typename T>
	void vecswap(T &a, T &b) {
#pragma HLS inline off
		int tmp;

VECSWAP_LOOP:	for (auto i = 0; i < N; i++) {
#pragma HLS pipeline
			tmp = a[i];
			a[i] = b[i];
			b[i] = tmp;
		}
	}

extern "C" void vecswap_top(int a[N], int b[N]) {
#pragma HLS INTERFACE m_axi port=a bundle=gmem0 depth=N
#pragma HLS INTERFACE m_axi port=b bundle=gmem1 depth=N
#pragma HLS INTERFACE ap_ctrl_hs port=return

#pragma HLS dataflow disable_start_propagation
	cache_t a_cache(a);
	cache_t b_cache(b);

	cache_wrapper(vecswap<cache_t>, a_cache, b_cache);
}

int main() {
	int a[N];
	int b[N];
	int a_ref[N];
	int b_ref[N];

	for (auto i = 0; i < N; i++) {
		a[i] = i;
		b[i] = -i;
		a_ref[i] = a[i];
		b_ref[i] = b[i];
	}

	vecswap_top(a, b);
	vecswap(a_ref, b_ref);

	for (int i = 0; i < N; i++) {
		if (a[i] != a_ref[i] || b[i] != b_ref[i])
			return 1;
	}

	return 0;
}

