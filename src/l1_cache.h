#ifndef L1_CACHE_H
#define L1_CACHE_H

#include "address.h"
#include "utils.h"
#include "ap_int.h"
#ifdef __SYNTHESIS__
#include "hls_vector.h"
#else
#include <array>
#endif /* __SYNTHESIS__ */

template <typename T, size_t MAIN_SIZE, size_t N_SETS, size_t N_WORDS_PER_LINE>
class l1_cache {
	private:
		static const size_t ADDR_SIZE = utils::log2_ceil(MAIN_SIZE);
		static const size_t SET_SIZE = utils::log2_ceil(N_SETS);
		static const size_t OFF_SIZE = utils::log2_ceil(N_WORDS_PER_LINE);
		static const size_t TAG_SIZE = (ADDR_SIZE - (SET_SIZE + OFF_SIZE));

		static_assert(((MAIN_SIZE > 0) && ((1 << ADDR_SIZE) == MAIN_SIZE)),
				"MAIN_SIZE must be a power of 2 greater than 0");
		static_assert(((N_SETS == 0) || (1 << SET_SIZE) == N_SETS),
				"N_SETS must be a power of 2");
		static_assert(((N_WORDS_PER_LINE > 0) &&
					((1 << OFF_SIZE) == N_WORDS_PER_LINE)),
				"N_WORDS_PER_LINE must be a power of 2 greater than 0");
		static_assert((MAIN_SIZE >= (N_SETS * N_WORDS_PER_LINE)),
				"N_SETS and/or N_WORDS_PER_LINE are too big for the specified MAIN_SIZE");

#ifdef __SYNTHESIS__
		template <typename TYPE, size_t SIZE>
			using array_type = hls::vector<TYPE, SIZE>;
#else
		template <typename TYPE, size_t SIZE>
			using array_type = std::array<TYPE, SIZE>;
#endif /* __SYNTHESIS__ */

		typedef array_type<T, N_WORDS_PER_LINE> line_type;
		typedef address<ADDR_SIZE, TAG_SIZE, SET_SIZE, 0> addr_type;

		ap_uint<(TAG_SIZE > 0) ? TAG_SIZE : 1> m_tag[(N_SETS > 0) ? N_SETS : 1];	// 1
		bool m_valid[(N_SETS > 0) ? N_SETS : 1];					// 2
		T m_cache_mem[((N_SETS > 0) ? N_SETS : 1) * N_WORDS_PER_LINE];			// 3

	public:
		l1_cache() {
#pragma HLS array_partition variable=m_tag complete
#pragma HLS array_partition variable=m_valid complete
#pragma HLS array_partition variable=m_cache_mem cyclic factor=N_WORDS_PER_LINE
		}

		void init() {
#pragma HLS inline
			for (auto line = 0; line < N_SETS; line++)
				m_valid[line] = false;
		}

		void get_line(const ap_uint<ADDR_SIZE> addr_main, line_type &line) const {
#pragma HLS inline
			const addr_type addr(addr_main);

			for (auto off = 0; off < N_WORDS_PER_LINE; off++) {
#pragma HLS unroll
				line[off] = m_cache_mem[addr.m_set * N_WORDS_PER_LINE + off];
			}
		}

		void set_line(const ap_uint<ADDR_SIZE> addr_main, const line_type &line) {
#pragma HLS inline
			const addr_type addr(addr_main);

			for (auto off = 0; off < N_WORDS_PER_LINE; off++) {
#pragma HLS unroll
				m_cache_mem[addr.m_set * N_WORDS_PER_LINE + off] = line[off];
			}
			m_valid[addr.m_set] = true;
			m_tag[addr.m_set] = addr.m_tag;
		}

		void notify_write(const ap_uint<ADDR_SIZE> addr_main) {
#pragma HLS inline
			const addr_type addr(addr_main);

			if (hit(addr_main))
				m_valid[addr.m_set] = false;
		}

		inline bool hit(const ap_uint<ADDR_SIZE> &addr_main) const {
#pragma HLS inline
			const addr_type addr(addr_main);

			return (m_valid[addr.m_set] &&
					(addr.m_tag == m_tag[addr.m_set]));
		}
};

#endif /* L1_CACHE_H */

