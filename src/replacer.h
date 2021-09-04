#ifndef REPLACER_H
#define REPLACER_H

/**
 * \file	replacer.h
 *
 * \brief 	Module in charge of managing replacement policy
 * 		(least-recently-used or last-in first out).
 */

#include "address.h"
#include "utils.h"
#include "ap_int.h"

template <bool LRU, typename ADDR_T, size_t N_SETS, size_t N_WAYS, size_t N_WORDS_PER_LINE>
class replacer {
	private:
		static const size_t WAY_SIZE = utils::log2_ceil(N_WAYS);

		ap_uint<(WAY_SIZE > 0) ? WAY_SIZE : 1> m_lru[N_SETS][N_WAYS];
		ap_uint<(WAY_SIZE > 0) ? WAY_SIZE : 1> m_lifo[N_SETS];

	public:
		replacer() {
#pragma HLS array_partition variable=m_lru complete dim=0
#pragma HLS array_partition variable=m_lifo complete dim=0
		}

		/**
		 * \brief	Initialize replacement policy data structures.
		 */
		void init() {
#pragma HLS inline
			if (LRU) {
				for (auto set = 0; set < N_SETS; set++) {
#pragma HLS unroll
					for (auto way = 0; way < N_WAYS; way++)
						m_lru[set][way] = way;
				}
			} else {
				for (auto set = 0; set < N_SETS; set++) {
#pragma HLS unroll
					m_lifo[set] = 0;
				}
			}
		}

		/**
		 * \brief	Update replacement policy data structures.
		 *
		 * \param addr	The address which has been used.
		 */
		void notify_use(const ADDR_T &addr) {
#pragma HLS inline
			if (LRU) {
				// find the position of the last used way
				int lru_way = -1;
				for (auto way = 0; way < N_WAYS; way++) {
					if (m_lru[addr.m_set][way] == addr.m_way)
						lru_way = way;
				}

				// fill the vacant position of the last used way,
				// by shifting other ways to the left
				for (auto way = 0; way < (N_WAYS - 1); way++) {
					if (way >= lru_way) {
						m_lru[addr.m_set][way] =
							m_lru[addr.m_set][way + 1];
					}
				}

				// put the last used way to the rightmost position
				m_lru[addr.m_set][N_WAYS - 1] = addr.m_way;
			}
		}

		/**
		 * \brief	Update replacement policy data structures.
		 *
		 * \param addr	The address which has been inserted.
		 */
		void notify_insertion(const ADDR_T &addr) {
#pragma HLS inline
			if (!LRU) {
				if (WAY_SIZE > 0)
					m_lifo[addr.m_set]++;
			}
		}

		/**
		 * \brief	Return the least recently used way associable
		 * 		with \p addr.
		 *
		 * \param addr	The address to be associated.
		 *
		 * \return	The least recently used way.
		 */
		inline int get_way(const ADDR_T &addr) const {
#pragma HLS inline
			return (LRU ? m_lru[addr.m_set][0] : m_lifo[addr.m_set]);
		}
};

#endif /* REPLACER_H */

