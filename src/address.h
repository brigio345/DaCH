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
		static const size_t TAG_SIZE_ACTUAL = (TAG_SIZE > 0) ? TAG_SIZE : 1;
		static const size_t SET_SIZE_ACTUAL = (SET_SIZE > 0) ? SET_SIZE : 1;
		static const size_t WAY_SIZE_ACTUAL = (WAY_SIZE > 0) ? WAY_SIZE : 1;
		static const size_t OFF_SIZE_ACTUAL = (OFF_SIZE > 0) ? OFF_SIZE : 1;
		static const size_t CACHE_ADDR_SIZE_ACTUAL = (CACHE_ADDR_SIZE > 0) ?
			CACHE_ADDR_SIZE : 1;
		static const size_t LINE_ADDR_SIZE_ACTUAL = (LINE_ADDR_SIZE > 0) ?
			LINE_ADDR_SIZE : 1;

	public:
#ifdef __SYNTHESIS__
		const ap_uint<ADDR_SIZE> m_addr_main;
		const ap_uint<TAG_SIZE_ACTUAL> m_tag;
		const ap_uint<SET_SIZE_ACTUAL> m_set;
		const ap_uint<OFF_SIZE_ACTUAL> m_off;
		ap_uint<CACHE_ADDR_SIZE_ACTUAL> m_addr_cache;
		ap_uint<LINE_ADDR_SIZE_ACTUAL> m_addr_line;
		ap_uint<WAY_SIZE_ACTUAL> m_way;
#else
		const unsigned int m_addr_main;
		const unsigned int m_tag;
		const unsigned int m_set;
		const unsigned int m_off;
		unsigned int m_addr_cache;
		unsigned int m_addr_line;
		unsigned int m_way;
#endif /* __SYNTHESIS__ */

		address(const unsigned int addr_main):
			m_addr_main(addr_main),
#ifdef __SYNTHESIS__
			m_tag((TAG_SIZE > 0) ? (addr_main >> (OFF_SIZE + SET_SIZE)) : 0),
			m_set((SET_SIZE > 0) ? (addr_main >> OFF_SIZE) : 0),
			m_off((OFF_SIZE > 0) ? addr_main : 0) {
#else
			m_tag(addr_main >> (OFF_SIZE + SET_SIZE)),
			m_set((addr_main >> OFF_SIZE_ACTUAL) & (~(-1U << SET_SIZE_ACTUAL))),
			m_off(addr_main & (~(-1U << OFF_SIZE_ACTUAL))) {
#endif /* __SYNTHESIS__ */
#pragma HLS inline
			}

		address(const unsigned int tag, const unsigned int set,
				const unsigned int off, const unsigned int way):
#ifdef __SYNTHESIS__
				m_addr_main((static_cast<ap_uint<ADDR_SIZE>>(tag) << (SET_SIZE + OFF_SIZE)) |
						(static_cast<ap_uint<ADDR_SIZE>>(set) << OFF_SIZE) | off),
#else
				m_addr_main((tag << (SET_SIZE + OFF_SIZE)) |
						(set << OFF_SIZE) | off),
#endif /* __SYNTHESIS__ */
				m_tag(tag), m_set(set), m_off(off) {
#pragma HLS inline
			set_way(way);
		}

		void set_way(const unsigned int way) {
#pragma HLS inline
			m_way = way;
#ifdef __SYNTHESIS__
			m_addr_line = ((static_cast<ap_uint<LINE_ADDR_SIZE_ACTUAL>>(m_set) << WAY_SIZE) | way);
			m_addr_cache = ((static_cast<ap_uint<CACHE_ADDR_SIZE_ACTUAL>>(m_set) << (WAY_SIZE + OFF_SIZE)) |
					(static_cast<ap_uint<CACHE_ADDR_SIZE_ACTUAL>>(m_way) << OFF_SIZE) | m_off);
#else
			m_addr_line = ((m_set << WAY_SIZE) | way) & ~(-1U << LINE_ADDR_SIZE_ACTUAL);
			m_addr_cache = ((m_set << (WAY_SIZE + OFF_SIZE)) |
					(m_way << OFF_SIZE) | m_off) & ~(-1U << CACHE_ADDR_SIZE_ACTUAL);
#endif /* __SYNTHESIS__ */
		}
};

template <size_t ADDR_SIZE, size_t TAG_SIZE, size_t SET_SIZE, size_t WAY_SIZE>
class address<ADDR_SIZE, TAG_SIZE, SET_SIZE, WAY_SIZE, true> {
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
#ifdef __SYNTHESIS__
		const ap_uint<ADDR_SIZE> m_addr_main;
		const ap_uint<TAG_SIZE_ACTUAL> m_tag;
		const ap_uint<SET_SIZE_ACTUAL> m_set;
		const ap_uint<OFF_SIZE_ACTUAL> m_off;
		ap_uint<CACHE_ADDR_SIZE_ACTUAL> m_addr_cache;
		ap_uint<LINE_ADDR_SIZE_ACTUAL> m_addr_line;
		ap_uint<WAY_SIZE_ACTUAL> m_way;
#else
		const unsigned int m_addr_main;
		const unsigned int m_tag;
		const unsigned int m_set;
		const unsigned int m_off;
		unsigned int m_addr_cache;
		unsigned int m_addr_line;
		unsigned int m_way;
#endif /* __SYNTHESIS__ */

		address(const unsigned int addr_main):
			m_addr_main(addr_main),
#ifdef __SYNTHESIS__
			m_set((SET_SIZE > 0) ? (addr_main >> (OFF_SIZE + TAG_SIZE)) : 0),
			m_tag((TAG_SIZE > 0) ? (addr_main >> OFF_SIZE) : 0),
			m_off((OFF_SIZE > 0) ? addr_main : 0) {
#else
			m_set(addr_main >> (OFF_SIZE + TAG_SIZE)),
			m_tag((addr_main >> OFF_SIZE) & (~(-1U << TAG_SIZE_ACTUAL))),
			m_off(addr_main & (~(-1U << OFF_SIZE_ACTUAL))) {
#endif /* __SYNTHESIS__ */
#pragma HLS inline
			}

		address(const unsigned int tag, const unsigned int set,
				const unsigned int off, const unsigned int way):
#ifdef __SYNTHESIS__
				m_addr_main((static_cast<ap_uint<ADDR_SIZE>>(set) << (TAG_SIZE + OFF_SIZE)) |
						(static_cast<ap_uint<ADDR_SIZE>>(tag) << OFF_SIZE) | off),
#else
				m_addr_main((set << (TAG_SIZE + OFF_SIZE)) |
						(tag << OFF_SIZE) | off),
#endif /* __SYNTHESIS__ */
				m_tag(tag), m_set(set), m_off(off) {
#pragma HLS inline
			set_way(way);
		}

		void set_way(const unsigned int way) {
#pragma HLS inline
			m_way = way;
#ifdef __SYNTHESIS__
			m_addr_line = ((static_cast<ap_uint<LINE_ADDR_SIZE_ACTUAL>>(m_set) << WAY_SIZE) | way);
			m_addr_cache = ((static_cast<ap_uint<CACHE_ADDR_SIZE_ACTUAL>>(m_set) << (WAY_SIZE + OFF_SIZE)) |
					(static_cast<ap_uint<CACHE_ADDR_SIZE_ACTUAL>>(m_way) << OFF_SIZE) | m_off);
#else
			m_addr_line = ((m_set << WAY_SIZE) | way);
			m_addr_cache = ((m_set << (WAY_SIZE + OFF_SIZE)) |
					(m_way << OFF_SIZE) | m_off);
#endif /* __SYNTHESIS__ */
		}
};

#endif /* ADDRESS_H */

