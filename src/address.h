#ifndef ADDRESS_H
#define ADDRESS_H

#include "ap_int.h"

template <size_t ADDR_SIZE, size_t TAG_SIZE, size_t SET_SIZE, size_t WAY_SIZE>
class address {
	private:
		static const size_t OFF_SIZE = (ADDR_SIZE - (TAG_SIZE + SET_SIZE));
		static const size_t CACHE_ADDR_SIZE = (SET_SIZE + WAY_SIZE + OFF_SIZE);
		static const size_t LINE_ADDR_SIZE = (SET_SIZE + WAY_SIZE);
		static const size_t TAG_SIZE_ACTUAL = (TAG_SIZE > 0) ? TAG_SIZE : 1;
		static const size_t SET_SIZE_ACTUAL = (SET_SIZE > 0) ? SET_SIZE : 1;
		static const size_t WAY_SIZE_ACTUAL = (WAY_SIZE > 0) ? WAY_SIZE : 1;
		static const size_t OFF_SIZE_ACTUAL = (OFF_SIZE > 0) ? OFF_SIZE : 1;
		static const size_t CACHE_ADDR_SIZE_ACTUAL = (CACHE_ADDR_SIZE > 0) ?
			CACHE_ADDR_SIZE : 1;
		static const size_t LINE_ADDR_SIZE_ACTUAL = (LINE_ADDR_SIZE > 0) ?
			LINE_ADDR_SIZE : 1;

	public:
		const ap_uint<ADDR_SIZE> m_addr_main;
		const ap_uint<TAG_SIZE_ACTUAL> m_tag;
		const ap_uint<SET_SIZE_ACTUAL> m_set;
		const ap_uint<OFF_SIZE_ACTUAL> m_off;
		ap_uint<CACHE_ADDR_SIZE_ACTUAL> m_addr_cache;
		ap_uint<LINE_ADDR_SIZE_ACTUAL> m_addr_line;
		ap_uint<WAY_SIZE_ACTUAL> m_way;

		address(const unsigned int addr_main):
			m_addr_main(addr_main),
			m_tag((TAG_SIZE > 0) ? (addr_main >> (OFF_SIZE + SET_SIZE)) : 0),
			m_set((SET_SIZE > 0) ? (addr_main >> OFF_SIZE) : 0),
			m_off((OFF_SIZE > 0) ? addr_main : 0) {}

		address(const unsigned int tag, const unsigned int set,
				const unsigned int off, const unsigned int way):
				m_addr_main((static_cast<ap_uint<ADDR_SIZE>>(tag) << (SET_SIZE + OFF_SIZE)) |
						(static_cast<ap_uint<ADDR_SIZE>>(set) << OFF_SIZE) | off),
				m_tag(tag), m_set(set), m_off(off) {
			set_way(way);
		}

		void set_way(const unsigned int way) {
			m_way = way;
			m_addr_line = ((static_cast<ap_uint<LINE_ADDR_SIZE_ACTUAL>>(m_set) << WAY_SIZE) | way);
			m_addr_cache = ((static_cast<ap_uint<CACHE_ADDR_SIZE_ACTUAL>>(m_set) << (WAY_SIZE + OFF_SIZE)) |
					(static_cast<ap_uint<CACHE_ADDR_SIZE_ACTUAL>>(m_way) << OFF_SIZE) | m_off);
		}
};

#endif /* ADDRESS_H */

