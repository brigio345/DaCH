#include <iostream>
#include "cache.h"
#define DEBUG

static const size_t N = 128;

static const size_t RD_PORTS = 16;

typedef cache<int, true, false, RD_PORTS, N, 1, 1, 8, false, 1, 1, false, 7> cache_a;

template <typename T>
	void vecsum(T &a, int &sum) {
#pragma HLS inline
		int tmp = 0;
		int data;

VECSUM_LOOP:	for (auto i = 0; i < N; i++) {
#pragma HLS pipeline II=1
#pragma HLS unroll factor=RD_PORTS
			data = a[i];
			tmp += data;
#ifdef DEBUG
			std::cout << i << " " << data << std::endl;
#endif /* DEBUG */
		}

		sum = tmp;
	}

void vecsum_wrapper(cache_a &a, int &sum) {
#pragma HLS inline off
	a.init();

	vecsum(a, sum);

	a.stop();
}

extern "C" void vecsum_top(int a[N], int &sum) {
#pragma HLS INTERFACE m_axi port=a bundle=gmem0 depth=N
#pragma HLS INTERFACE ap_ctrl_hs port=return

#pragma HLS dataflow disable_start_propagation
	cache_a a_cache;

	a_cache.run(a);
	vecsum_wrapper(a_cache, sum);
}

int main() {
	int a[N];
	int sum;
	int sum_ref;

	for (auto i = 0; i < N; i++)
		a[i] = i;
	vecsum_top(a, sum);
	vecsum(a, sum_ref);
	std::cout << "sum=" << sum << std::endl;
	std::cout << "sum_ref=" << sum_ref << std::endl;

	return (sum != sum_ref);
}

