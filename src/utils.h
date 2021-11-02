#ifndef UTILS_H
#define UTILS_H

namespace utils {
	constexpr unsigned log2_floor(const unsigned x) {
		    return ((x > 1) ? (1 + log2_floor(x >> 1)) : 0);
	}

	constexpr unsigned log2_ceil(const unsigned x) {
		    return ((x > 1) ? (log2_floor(x - 1) + 1) : 0);
	}

	template <typename T, size_t AMOUNT>
		__attribute__((hls_preserve))
		T delay(T data) {
#pragma HLS pipeline II=1
#pragma HLS latency min=AMOUNT max=AMOUNT
#pragma HLS inline off
			return data;
		}
}

#endif /* UTILS_H */

