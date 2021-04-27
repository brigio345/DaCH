#ifndef ADDRESS_H
#define ADDRESS_H

#include "ap_int.h"

template <size_t ADDR_SIZE, size_t TAG_SIZE, size_t LINE_SIZE>
class address {
	private:
		static const size_t OFF_SIZE = (ADDR_SIZE - (TAG_SIZE + LINE_SIZE));
		static const unsigned int TAG_HIGH = ADDR_SIZE - 1;
		static const unsigned int TAG_LOW = TAG_HIGH - TAG_SIZE + 1;
		static const unsigned int LINE_HIGH = TAG_LOW - 1;
		static const unsigned int LINE_LOW = LINE_HIGH - LINE_SIZE + 1;
		static const unsigned int OFF_HIGH = ((LINE_SIZE > 0) ? (LINE_LOW - 1) : (TAG_LOW - 1));
		static const unsigned int OFF_LOW = 0;

	public:
		ap_uint<ADDR_SIZE> _addr_main;
		ap_uint<ADDR_SIZE> _addr_main_first_of_line;
		ap_uint<ADDR_SIZE> _addr_cache;
		ap_uint<ADDR_SIZE> _addr_cache_first_of_line;
		ap_uint<TAG_SIZE> _tag;
		ap_uint<(LINE_SIZE > 0) ? LINE_SIZE : 1> _line;
		ap_uint<(OFF_SIZE > 0) ? OFF_SIZE : 1> _off;

		address(ap_uint<ADDR_SIZE> addr_main): _addr_main(addr_main) {
			if (OFF_SIZE == 0) {
				_tag = addr_main(TAG_HIGH, TAG_LOW);
				_line = addr_main(LINE_HIGH, LINE_LOW);
				_off = 0;
				_addr_cache = _line;
				_addr_cache_first_of_line = _addr_cache;
				_addr_main_first_of_line = addr_main;
			} else if (LINE_SIZE == 0) {
				_tag = addr_main(TAG_HIGH, TAG_LOW);
				_line = 0;
				_off = addr_main(OFF_HIGH, OFF_LOW);
				_addr_cache = _off;
				_addr_cache_first_of_line = _addr_cache & (-1u << OFF_SIZE);
				_addr_main_first_of_line = addr_main & (-1u << OFF_SIZE);
			} else {
				_tag = addr_main(TAG_HIGH, TAG_LOW);
				_line = addr_main(LINE_HIGH, LINE_LOW);
				_off = addr_main(OFF_HIGH, OFF_LOW);
				_addr_cache = (_line, _off);
				_addr_cache_first_of_line = _addr_cache & (-1u << OFF_SIZE);
				_addr_main_first_of_line = addr_main & (-1u << OFF_SIZE);
			}
		}

		address(ap_uint<TAG_SIZE> tag,
				ap_uint<(LINE_SIZE > 0) ? LINE_SIZE : 1> line,
				ap_uint<(OFF_SIZE > 0) ? OFF_SIZE : 1> off):
				_tag(tag), _line(line), _off(off) {
			if (OFF_SIZE == 0) {
				_addr_main = (tag, line);
				_addr_main_first_of_line = _addr_main;
				_addr_cache = line;
				_addr_cache_first_of_line = line;
			} else if (LINE_SIZE == 0) {
				_addr_main = (tag, off);
				_addr_main_first_of_line = _addr_main & (-1u << OFF_SIZE);
				_addr_cache = off;
				_addr_cache_first_of_line = 0;
			} else {
				_addr_main = (tag, line, off);
				_addr_main_first_of_line = _addr_main & (-1u << OFF_SIZE);
				_addr_cache = (line, off);
				_addr_cache_first_of_line = _addr_cache & (-1u << OFF_SIZE);
			}
		}
};

#endif /* ADDRESS_H */

