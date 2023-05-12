#ifndef RAW_CACHE_H
#define RAW_CACHE_H

#include "utils.h"
#include <ap_int.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wpedantic"
#pragma GCC diagnostic error "-Wall"
#pragma GCC diagnostic error "-Wextra"
#pragma GCC diagnostic ignored "-Wunused-label"

template <typename WORD_TYPE, size_t N_LINES, size_t N_WORDS_PER_LINE,
	 size_t DISTANCE>
class raw_cache {
	private:
		static const size_t MAIN_SIZE = (N_LINES * N_WORDS_PER_LINE);
		static const size_t ADDR_SIZE = utils::log2_ceil(MAIN_SIZE);

		typedef WORD_TYPE line_type[N_WORDS_PER_LINE];

		ap_uint<DISTANCE> m_valid;
		WORD_TYPE m_cache_mem[DISTANCE][N_WORDS_PER_LINE];
		ap_uint<(ADDR_SIZE > 0) ? ADDR_SIZE : 1> m_tag[DISTANCE];

	public:
		raw_cache() {
#pragma HLS array_partition variable=m_cache_mem type=complete dim=0
#pragma HLS array_partition variable=m_tag type=complete dim=0
		}

		void init() {
#pragma HLS inline
			m_valid = 0;
		}

		void get_line(const WORD_TYPE main_mem[N_LINES][N_WORDS_PER_LINE],
				const ap_uint<(ADDR_SIZE > 0) ? ADDR_SIZE : 1> addr_main,
				line_type line) const {
#pragma HLS inline
			const auto way = hit(addr_main);
			if (way != -1) {
				for (size_t off = 0; off < N_WORDS_PER_LINE; off++)
					line[off] = m_cache_mem[way][off];
			} else {
				for (size_t off = 0; off < N_WORDS_PER_LINE; off++)
					line[off] = main_mem[addr_main][off];
			}
		}

		void set_line(WORD_TYPE main_mem[N_LINES][N_WORDS_PER_LINE],
				const ap_uint<(ADDR_SIZE > 0) ? ADDR_SIZE : 1> addr_main,
				const line_type line) {
#pragma HLS inline
			for (size_t off = 0; off < N_WORDS_PER_LINE; off++)
				main_mem[addr_main][off] = line[off];

			for (int way = (DISTANCE - 1); way > 0; way--) {
#pragma HLS unroll
				for (size_t off = 0; off < N_WORDS_PER_LINE; off++)
					m_cache_mem[way][off] = m_cache_mem[way - 1][off];
				m_tag[way] = m_tag[way - 1];
				m_valid[way] = m_valid[way - 1];
			}

			for (size_t off = 0; off < N_WORDS_PER_LINE; off++)
				m_cache_mem[0][off] = line[off];
			m_tag[0] = addr_main;
			m_valid[0] = true;
		}

	private:
		int hit(const ap_uint<(ADDR_SIZE > 0) ? ADDR_SIZE : 1> addr_main) const {
#pragma HLS inline
			for (size_t way = 0; way < DISTANCE; way++) {
#pragma HLS unroll
				if (m_valid[way] && (addr_main == m_tag[way]))
					return way;
			}

			return -1;
		}
};

#pragma GCC diagnostic pop

#endif /* RAW_CACHE_H */

