#ifndef ADDRESS_H
#define ADDRESS_H

#include "ap_int.h"

template <size_t ADDR_SIZE, size_t TAG_SIZE, size_t SET_SIZE, size_t WAY_SIZE,
	 bool SWAP_TAG_SET>
class address {
	private:
		static const size_t OFF_SIZE = (ADDR_SIZE - (TAG_SIZE + SET_SIZE));
		static const size_t CACHE_ADDR_SIZE = (SET_SIZE + WAY_SIZE + OFF_SIZE);
		static const size_t LINE_ADDR_SIZE = (SET_SIZE + WAY_SIZE);

#ifdef __SYNTHESIS__
		typedef ap_uint<ADDR_SIZE> addr_main_type;
		typedef ap_uint<(CACHE_ADDR_SIZE > 0) ? CACHE_ADDR_SIZE : 1> cache_addr_type;
		typedef ap_uint<(LINE_ADDR_SIZE > 0) ? LINE_ADDR_SIZE : 1> line_addr_type;
		typedef ap_uint<(TAG_SIZE > 0) ? TAG_SIZE : 1> tag_type;
		typedef ap_uint<(SET_SIZE > 0) ? SET_SIZE : 1> set_type;
		typedef ap_uint<(WAY_SIZE > 0) ? WAY_SIZE : 1> way_type;
		typedef ap_uint<(OFF_SIZE > 0) ? OFF_SIZE : 1> off_type;
#else
		typedef unsigned long int addr_main_type;
		typedef unsigned long int cache_addr_type;
		typedef unsigned long int line_addr_type;
		typedef unsigned long int tag_type;
		typedef unsigned long int set_type;
		typedef unsigned long int way_type;
		typedef unsigned long int off_type;
#endif /* __SYNTHESIS__ */

	public:
		const addr_main_type m_addr_main;
		const tag_type m_tag;
		const set_type m_set;
		const off_type m_off;
		cache_addr_type m_addr_cache;
		line_addr_type m_addr_line;
		way_type m_way;

		address(const unsigned int addr_main):
			m_addr_main(addr_main),
			m_tag(((TAG_SIZE > 0) ? (addr_main >> (OFF_SIZE + SET_SIZE)) : 0) & (~(-1U << TAG_SIZE))),
			m_set(((SET_SIZE > 0) ? (addr_main >> OFF_SIZE) : 0) & (~(-1U << SET_SIZE))),
			m_off(((OFF_SIZE > 0) ? addr_main : 0) & (~(-1U << OFF_SIZE))) {
#pragma HLS inline
			}

		address(const unsigned int tag, const unsigned int set,
				const unsigned int off, const unsigned int way):
				m_addr_main((static_cast<addr_main_type>(tag) << (SET_SIZE + OFF_SIZE)) |
						(static_cast<addr_main_type>(set) << OFF_SIZE) | off),
				m_tag(tag), m_set(set), m_off(off) {
#pragma HLS inline
			set_way(way);
		}

		void set_way(const unsigned int way) {
#pragma HLS inline
			m_way = way;
			m_addr_line = ((static_cast<line_addr_type>(m_set) << WAY_SIZE) | way);
			m_addr_cache = ((static_cast<cache_addr_type>(m_set) << (WAY_SIZE + OFF_SIZE)) |
					(static_cast<cache_addr_type>(m_way) << OFF_SIZE) | m_off);
		}
};

template <size_t ADDR_SIZE, size_t TAG_SIZE, size_t SET_SIZE, size_t WAY_SIZE>
class address<ADDR_SIZE, TAG_SIZE, SET_SIZE, WAY_SIZE, true> {
	private:
		static const size_t OFF_SIZE = (ADDR_SIZE - (TAG_SIZE + SET_SIZE));
		static const size_t CACHE_ADDR_SIZE = (SET_SIZE + WAY_SIZE + OFF_SIZE);
		static const size_t LINE_ADDR_SIZE = (SET_SIZE + WAY_SIZE);

#ifdef __SYNTHESIS__
		typedef ap_uint<ADDR_SIZE> addr_main_type;
		typedef ap_uint<(CACHE_ADDR_SIZE > 0) ? CACHE_ADDR_SIZE : 1> cache_addr_type;
		typedef ap_uint<(LINE_ADDR_SIZE > 0) ? LINE_ADDR_SIZE : 1> line_addr_type;
		typedef ap_uint<(TAG_SIZE > 0) ? TAG_SIZE : 1> tag_type;
		typedef ap_uint<(SET_SIZE > 0) ? SET_SIZE : 1> set_type;
		typedef ap_uint<(WAY_SIZE > 0) ? WAY_SIZE : 1> way_type;
		typedef ap_uint<(OFF_SIZE > 0) ? OFF_SIZE : 1> off_type;
#else
		typedef unsigned long int addr_main_type;
		typedef unsigned long int cache_addr_type;
		typedef unsigned long int line_addr_type;
		typedef unsigned long int tag_type;
		typedef unsigned long int set_type;
		typedef unsigned long int way_type;
		typedef unsigned long int off_type;
#endif /* __SYNTHESIS__ */

	public:
		const addr_main_type m_addr_main;
		const tag_type m_tag;
		const set_type m_set;
		const off_type m_off;
		cache_addr_type m_addr_cache;
		line_addr_type m_addr_line;
		way_type m_way;

		address(const unsigned int addr_main):
			m_addr_main(addr_main),
			m_set(((SET_SIZE > 0) ? (addr_main >> (OFF_SIZE + TAG_SIZE)) : 0) & (~(-1U << SET_SIZE))),
			m_tag(((TAG_SIZE > 0) ? (addr_main >> OFF_SIZE) : 0) & (~(-1U << TAG_SIZE))),
			m_off(((OFF_SIZE > 0) ? addr_main : 0) & (~(-1U << OFF_SIZE))) {
#pragma HLS inline
			}

		address(const unsigned int tag, const unsigned int set,
				const unsigned int off, const unsigned int way):
				m_addr_main((static_cast<addr_main_type>(set) << (TAG_SIZE + OFF_SIZE)) |
						(static_cast<addr_main_type>(tag) << OFF_SIZE) | off),
				m_tag(tag), m_set(set), m_off(off) {
#pragma HLS inline
			set_way(way);
		}

		void set_way(const unsigned int way) {
#pragma HLS inline
			m_way = way;
			m_addr_line = ((static_cast<line_addr_type>(m_set) << WAY_SIZE) | way);
			m_addr_cache = ((static_cast<cache_addr_type>(m_set) << (WAY_SIZE + OFF_SIZE)) |
					(static_cast<cache_addr_type>(m_way) << OFF_SIZE) | m_off);
		}
};

#endif /* ADDRESS_H */

