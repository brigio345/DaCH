#ifndef UTILS_H
#define UTILS_H

namespace utils {
	constexpr unsigned log2_floor(unsigned x) {
		    return ((x > 1) ? (1 + log2_floor(x >> 1)) : 0);
	}

	constexpr unsigned log2_ceil(unsigned x) {
		    return ((x > 1) ? (log2_floor(x - 1) + 1) : 0);
	}
}

#endif /* UTILS_H */

