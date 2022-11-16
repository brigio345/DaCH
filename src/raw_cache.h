#ifndef RAW_CACHE_H
#define RAW_CACHE_H

#include "utils.h"
#include "ap_int.h"
#ifndef __SYNTHESIS__
#include <cstring>
#endif /* __SYNTHESIS__ */

template <typename T, size_t MAIN_SIZE, size_t DISTANCE>
class raw_cache {
	private:
		static const size_t ADDR_SIZE = utils::log2_ceil(MAIN_SIZE);
#ifdef __SYNTHESIS__
		typedef ap_uint<(ADDR_SIZE > 0) ? ADDR_SIZE : 1> main_addr_type;
#else
		typedef unsigned int main_addr_type;
#endif /* __SYNTHESIS__ */


#ifdef __SYNTHESIS__
		ap_uint<DISTANCE> m_valid;
#else
		bool m_valid[DISTANCE];
#endif /* __SYNTHESIS__ */
		T m_cache_mem[DISTANCE];
		main_addr_type m_tag[DISTANCE];

	public:
		raw_cache() {
#pragma HLS array_partition variable=m_cache_mem type=complete dim=0
#pragma HLS array_partition variable=m_tag type=complete dim=0
		}

		void init() {
#pragma HLS inline
#ifdef __SYNTHESIS__
			m_valid = 0;
#else
			std::memset(m_valid, 0, sizeof(m_valid));
#endif /* __SYNTHESIS__ */
		}

		void get_line(const T * const main_mem,
				const main_addr_type addr_main,
				T &data) const {
#pragma HLS inline
			const auto way = hit(addr_main);
#ifdef __SYNTHESIS__
			data = (way != -1) ? m_cache_mem[way] : main_mem[addr_main];
#else
			std::memcpy(data, (way != -1) ?
					m_cache_mem[way] : main_mem[addr_main],
					sizeof(data));
#endif /* __SYNTHESIS__ */
		}

		void set_line(T * const main_mem,
				const main_addr_type addr_main,
				const T &line) {
#pragma HLS inline
#ifdef __SYNTHESIS__
			main_mem[addr_main] = line;
#else
			std::memcpy(main_mem[addr_main], line,
					sizeof(main_mem[addr_main]));
#endif /* __SYNTHESIS__ */

			for (auto way = (DISTANCE - 1); way > 0; way--) {
#pragma HLS unroll
#ifdef __SYNTHESIS__
				m_cache_mem[way] = m_cache_mem[way - 1];
#else
				std::memcpy(m_cache_mem[way], m_cache_mem[way - 1],
						sizeof(m_cache_mem[way]));
#endif /* __SYNTHESIS__ */
				m_tag[way] = m_tag[way - 1];
				m_valid[way] = m_valid[way - 1];
			}

#ifdef __SYNTHESIS__
			m_cache_mem[0] = line;
#else
			std::memcpy(m_cache_mem[0], line, sizeof(m_cache_mem[0]));
#endif /* __SYNTHESIS__ */
			m_tag[0] = addr_main;
			m_valid[0] = true;
		}

	private:
		int hit(const main_addr_type addr_main) const {
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

