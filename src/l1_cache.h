#ifndef L1_CACHE_H
#define L1_CACHE_H

#include "address.h"
#include "utils.h"
#include "ap_int.h"
#include "hls_vector.h"

template <typename LINE_TYPE, size_t MAIN_SIZE, size_t N_SETS, size_t N_WAYS,
	 size_t N_WORDS_PER_LINE, bool SWAP_TAG_SET>
class l1_cache {
	private:
		static const size_t ADDR_SIZE = utils::log2_ceil(MAIN_SIZE);
		static const size_t SET_SIZE = utils::log2_ceil(N_SETS);
		static const size_t OFF_SIZE = utils::log2_ceil(N_WORDS_PER_LINE);
		static const size_t TAG_SIZE = (ADDR_SIZE - (SET_SIZE + OFF_SIZE));
		static const size_t WAY_SIZE = utils::log2_ceil(N_WAYS);
		static const size_t N_LINES = (((N_SETS * N_WAYS) > 0) ?
				(N_SETS * N_WAYS) : 1);

		static_assert(((MAIN_SIZE > 0) && ((1 << ADDR_SIZE) == MAIN_SIZE)),
				"MAIN_SIZE must be a power of 2 greater than 0");
		static_assert(((N_SETS == 0) || (1 << SET_SIZE) == N_SETS),
				"N_SETS must be a power of 2");
		static_assert(((N_WAYS == 0) || ((1 << WAY_SIZE) == N_WAYS)),
				"N_WAYS must be a power of 2");
		static_assert(((N_WORDS_PER_LINE > 0) &&
					((1 << OFF_SIZE) == N_WORDS_PER_LINE)),
				"N_WORDS_PER_LINE must be a power of 2 greater than 0");
		static_assert((MAIN_SIZE >= (N_SETS * N_WAYS * N_WORDS_PER_LINE)),
				"N_SETS and/or N_WAYS and/or N_WORDS_PER_LINE are too big for the specified MAIN_SIZE");

		typedef address<ADDR_SIZE, TAG_SIZE, SET_SIZE, WAY_SIZE, SWAP_TAG_SET>
			addr_type;
#ifdef __SYNTHESIS__
		typedef ap_uint<(ADDR_SIZE > 0) ? ADDR_SIZE : 1> addr_main_type;
#else
		typedef unsigned long int addr_main_type;
#endif /* __SYNTHESIS__ */
		typedef replacer<false, addr_type, ((N_SETS > 0) ? N_SETS : 1),
			((N_WAYS > 0) ? N_WAYS : 1), N_WORDS_PER_LINE> replacer_type;

		ap_uint<(TAG_SIZE > 0) ? TAG_SIZE : 1> m_tag[N_LINES];	// 1
		hls::vector<bool, N_LINES> m_valid;			// 2
		LINE_TYPE m_cache_mem[N_LINES];				// 3
		replacer_type m_replacer;				// 4

	public:
		l1_cache() {
#pragma HLS array_partition variable=m_tag type=complete dim=0
		}

		void init() {
#pragma HLS inline
			m_valid = 0;
			m_replacer.init();
		}

		bool get_line(const addr_main_type addr_main, LINE_TYPE &line) const {
#pragma HLS inline
			addr_type addr(addr_main);
			const auto way = hit(addr);

			if (way == -1)
				return false;

			addr.set_way(way);
			line = m_cache_mem[addr.m_addr_line];

			return true;
		}

		void set_line(const addr_main_type addr_main,
				const LINE_TYPE &line) {
#pragma HLS inline
			addr_type addr(addr_main);

			addr.set_way(m_replacer.get_way(addr));

			m_cache_mem[addr.m_addr_line] = line;
			m_valid[addr.m_addr_line] = true;
			m_tag[addr.m_addr_line] = addr.m_tag;

			m_replacer.notify_insertion(addr);
		}

		void notify_write(const addr_main_type addr_main) {
#pragma HLS inline
			const addr_type addr(addr_main);

			if (hit(addr) != -1)
				m_valid[addr.m_addr_line] = false;
		}

	private:
		inline int hit(const addr_type &addr) const {
#pragma HLS inline
			addr_type addr_tmp = addr;
			auto hit_way = -1;
			for (auto way = 0; way < N_WAYS; way++) {
				addr_tmp.set_way(way);
				if (m_valid[addr_tmp.m_addr_line] &&
						(addr_tmp.m_tag == m_tag[addr_tmp.m_addr_line])) {
					hit_way = way;
				}
			}

			return hit_way;
		}
};

#endif /* L1_CACHE_H */

