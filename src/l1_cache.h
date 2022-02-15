#ifndef L1_CACHE_H
#define L1_CACHE_H

#include "address.h"
#include "utils.h"
#include "ap_int.h"

template <typename LINE_TYPE, size_t MAIN_SIZE, size_t N_SETS, size_t N_WORDS_PER_LINE,
	 bool SWAP_TAG_SET>
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

		typedef address<ADDR_SIZE, TAG_SIZE, SET_SIZE, 0, SWAP_TAG_SET>
			addr_type;

		ap_uint<(TAG_SIZE > 0) ? TAG_SIZE : 1> m_tag[(N_SETS > 0) ? N_SETS : 1];	// 1
		ap_uint<(N_SETS > 0) ? N_SETS : 1> m_valid;					// 2
		LINE_TYPE m_cache_mem[((N_SETS > 0) ? N_SETS : 1)];				// 3

	public:
		l1_cache() {
#pragma HLS array_partition variable=m_tag complete
		}

		void init() {
#pragma HLS inline
			m_valid = 0;
		}

		void get_line(const ap_uint<ADDR_SIZE> addr_main,
				LINE_TYPE &line) const {
#pragma HLS inline
			const addr_type addr(addr_main);
			line = m_cache_mem[addr.m_set];
		}

		void set_line(const ap_uint<ADDR_SIZE> addr_main,
				const LINE_TYPE &line) {
#pragma HLS inline
			const addr_type addr(addr_main);

			m_cache_mem[addr.m_set] = line;
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

