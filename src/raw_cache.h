#ifndef RAW_CACHE_H
#define RAW_CACHE_H

#include "address.h"
#include "utils.h"
#include "ap_int.h"

template <typename T, size_t MAIN_SIZE, size_t DISTANCE>
class raw_cache {
	private:
		static const size_t ADDR_SIZE = utils::log2_ceil(MAIN_SIZE);

		bool m_valid[DISTANCE];
		T m_cache_mem[DISTANCE];
		ap_uint<(ADDR_SIZE > 0) ? ADDR_SIZE : 1> m_tag[DISTANCE];

	public:
		void init() {
			for (auto way = 0; way < DISTANCE; way++)
				m_valid[way] = false;
		}

		void get_line(const T * const main_mem,
				const ap_uint<(ADDR_SIZE > 0) ? ADDR_SIZE : 1> addr_main,
				T &data) const {
#pragma HLS inline
			const auto way = hit(addr_main);
			data = (way != -1) ? m_cache_mem[way] : main_mem[addr_main];
		}

		void set_line(T * const main_mem,
				const ap_uint<(ADDR_SIZE > 0) ? ADDR_SIZE : 1> addr_main,
				const T &line) {
#pragma HLS inline
			main_mem[addr_main] = line;

			for (auto way = (DISTANCE - 1); way > 0; way--) {
#pragma HLS unroll
				m_cache_mem[way] = m_cache_mem[way - 1];
				m_tag[way] = m_tag[way - 1];
				m_valid[way] = m_valid[way - 1];
			}

			m_cache_mem[0] = line;
			m_tag[0] = addr_main;
			m_valid[0] = true;
		}

	private:
		int hit(const ap_uint<(ADDR_SIZE > 0) ? ADDR_SIZE : 1> addr_main) const {
#pragma HLS inline
			for (auto way = 0; way < DISTANCE; way++) {
#pragma HLS unroll
				if (m_valid[way] && (addr_main == m_tag[way]))
					return way;
			}

			return -1;
		}
};

#endif /* RAW_CACHE_H */

