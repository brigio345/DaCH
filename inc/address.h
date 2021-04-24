#ifndef ADDRESS_H
#define ADDRESS_H

#include "ap_int.h"

template <size_t ADDR_SIZE = 32, size_t TAG_SIZE = 28, size_t LINE_SIZE = 2,
	size_t OFF_SIZE = 2, size_t N_ENTRIES_PER_LINE = 4>
class address {
	private:
		static const unsigned int TAG_HIGH = ADDR_SIZE - 1;
		static const unsigned int TAG_LOW = TAG_HIGH - TAG_SIZE + 1;
		static const unsigned int LINE_HIGH = TAG_LOW - 1;
		static const unsigned int LINE_LOW = LINE_HIGH - LINE_SIZE + 1;
		static const unsigned int OFF_HIGH = LINE_LOW - 1;
		static const unsigned int OFF_LOW = 0;

	public:
		ap_uint<ADDR_SIZE> _addr_main;
		ap_uint<ADDR_SIZE> _addr_main_first_of_line;
		ap_uint<ADDR_SIZE> _addr_cache;
		ap_uint<ADDR_SIZE> _addr_cache_first_of_line;
		ap_uint<TAG_SIZE> _tag;
		ap_uint<LINE_SIZE> _line;
		ap_uint<OFF_SIZE> _off;

		address(ap_uint<ADDR_SIZE> addr_main): _addr_main(addr_main) {
			_tag = addr_main.range(TAG_HIGH, TAG_LOW);
			_line = addr_main.range(LINE_HIGH, LINE_LOW);
			_off = addr_main.range(OFF_HIGH, OFF_LOW);
			_addr_cache = _line * N_ENTRIES_PER_LINE + _off;
			_addr_cache_first_of_line = _addr_cache & (-1u << OFF_SIZE);
			_addr_main_first_of_line = addr_main & (-1u << OFF_SIZE);
		}

		static address build(ap_uint<TAG_SIZE> tag,
				ap_uint<LINE_SIZE> line,
				ap_uint<OFF_SIZE> off = 0) {
			return address((tag, line, off));
		}
};

#endif /* ADDRESS_H */

