#ifndef UTILS_H
#define UTILS_H

namespace utils {
	constexpr unsigned log2_floor(const unsigned x) {
		    return ((x > 1) ? (1 + log2_floor(x >> 1)) : 0);
	}

	constexpr unsigned log2_ceil(const unsigned x) {
		    return ((x > 1) ? (log2_floor(x - 1) + 1) : 0);
	}

	constexpr int ceil(const float x) {
		const int round = (int)x;
		const int ceil_pos = (round < x) ? (round + 1) : round;
		const int ceil_neg = (round > x) ? (round - 1) : round;

		return (x >= 0) ? ceil_pos : ceil_neg;
	}

	template <size_t AMOUNT, typename T>
		__attribute__((hls_preserve))
		T delay(T data) {
#pragma HLS pipeline II=1
#pragma HLS latency min=AMOUNT max=AMOUNT
#pragma HLS inline off
			return data;
		}
}

#endif /* UTILS_H */

