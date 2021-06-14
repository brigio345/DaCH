#ifndef ADDRESS_H
#define ADDRESS_H

#include "ap_int.h"

template <size_t ADDR_SIZE, size_t TAG_SIZE, size_t SET_SIZE>
class address {
	private:
		static const size_t CACHE_ADDR_SIZE = (ADDR_SIZE - TAG_SIZE);
		static const size_t OFF_SIZE = (ADDR_SIZE - (TAG_SIZE + SET_SIZE));
		static const int TAG_HIGH = (ADDR_SIZE - 1);
		static const int TAG_LOW = ((TAG_SIZE > 0) ?
				(TAG_HIGH - TAG_SIZE + 1) : (TAG_HIGH + 1));
		static const int SET_HIGH = (TAG_LOW - 1);
		static const int SET_LOW = ((SET_SIZE > 0) ?
				(SET_HIGH - SET_SIZE + 1) : (SET_HIGH + 1));
		static const int OFF_HIGH = (SET_LOW - 1);
		static const int OFF_LOW = (OFF_SIZE > 0) ?
				(OFF_HIGH - OFF_SIZE + 1) : (OFF_HIGH + 1);

	public:
		ap_uint<ADDR_SIZE> _addr_main;
		ap_uint<(CACHE_ADDR_SIZE > 0) ? CACHE_ADDR_SIZE : 1> _addr_cache;
		ap_uint<(CACHE_ADDR_SIZE > 0) ? CACHE_ADDR_SIZE : 1> _addr_cache_first_of_line;
		ap_uint<(TAG_SIZE > 0) ? TAG_SIZE : 1> _tag;
		ap_uint<(SET_SIZE > 0) ? SET_SIZE : 1> _set;
		ap_uint<(OFF_SIZE > 0) ? OFF_SIZE : 1> _off;

		address(ap_uint<ADDR_SIZE> addr_main): _addr_main(addr_main) {
			_off = 0;
			_tag = 0;
			_set = 0;
			for (auto i = 0; i < ADDR_SIZE; i++) {
#pragma HLS unroll
				if (i <= OFF_HIGH)
					_off[i - OFF_LOW] = addr_main[i];
				else if ((i >= SET_LOW) && (i <= SET_HIGH))
					_set[i - SET_LOW] = addr_main[i];
				else
					_tag[i - TAG_LOW] = addr_main[i];
			}

			_addr_cache = (CACHE_ADDR_SIZE > 0) ? 
				_addr_main((CACHE_ADDR_SIZE - 1), 0) : 0;
			_addr_cache_first_of_line = _addr_cache & (-1u << OFF_SIZE);
		}

		address(ap_uint<(TAG_SIZE > 0) ? TAG_SIZE : 1> tag,
				ap_uint<(SET_SIZE > 0) ? SET_SIZE : 1> set,
				ap_uint<(OFF_SIZE > 0) ? OFF_SIZE : 1> off):
				_tag(tag), _set(set), _off(off) {
			for (auto i = 0; i < ADDR_SIZE; i++) {
#pragma HLS unroll
				if (i <= OFF_HIGH)
					_addr_main[i] = off[i];
				else if ((i >= SET_LOW) && (i <= SET_HIGH))
					_addr_main[i] = set[i - SET_LOW];
				else
					_addr_main[i] = tag[i - TAG_LOW];
			}

			_addr_cache = (CACHE_ADDR_SIZE > 0) ? 
				_addr_main((CACHE_ADDR_SIZE - 1), 0) : 0;
			_addr_cache_first_of_line = _addr_cache & (-1u << OFF_SIZE);
		}
};

#endif /* ADDRESS_H */

