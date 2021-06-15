#ifndef ADDRESS_H
#define ADDRESS_H

#include "ap_int.h"

template <size_t ADDR_SIZE, size_t TAG_SIZE, size_t SET_SIZE, size_t WAY_SIZE>
class address {
	private:
		static const size_t OFF_SIZE = (ADDR_SIZE - (TAG_SIZE + SET_SIZE));
		static const size_t LINE_ADDR_SIZE = (SET_SIZE + WAY_SIZE);
		static const size_t CACHE_ADDR_SIZE = (LINE_ADDR_SIZE + OFF_SIZE);

	public:
		ap_uint<ADDR_SIZE> _addr_main;
		ap_uint<CACHE_ADDR_SIZE> _addr_cache;
		ap_uint<(LINE_ADDR_SIZE > 0) ? LINE_ADDR_SIZE : 1> _addr_line;
		ap_uint<(TAG_SIZE > 0) ? TAG_SIZE : 1> _tag;
		ap_uint<(SET_SIZE > 0) ? SET_SIZE : 1> _set;
		ap_uint<(OFF_SIZE > 0) ? OFF_SIZE : 1> _off;
		ap_uint<(WAY_SIZE > 0) ? WAY_SIZE : 1> _way;

		address(ap_uint<ADDR_SIZE> addr_main): _addr_main(addr_main) {
			_off = 0;
			for (auto i = 0; i < OFF_SIZE; i++)
				_off[i] = addr_main[i];
			_set = 0;
			for (auto i = 0; i < SET_SIZE; i++)
				_set[i] = addr_main[i + OFF_SIZE];
			_tag = 0;
			for (auto i = 0; i < TAG_SIZE; i++)
				_tag[i] = addr_main[i + OFF_SIZE + SET_SIZE];

			_addr_cache = -1;
			_addr_line = -1;
		}

		address(ap_uint<(TAG_SIZE > 0) ? TAG_SIZE : 1> tag,
				ap_uint<(SET_SIZE > 0) ? SET_SIZE : 1> set,
				ap_uint<(OFF_SIZE > 0) ? OFF_SIZE : 1> off):
				_tag(tag), _set(set), _off(off) {
			_addr_main = 0;

			for (auto i = 0; i < OFF_SIZE; i++)
				_addr_main[i] = _off[i];
			for (auto i = 0; i < SET_SIZE; i++)
				_addr_main[i + OFF_SIZE] = set[i];
			for (auto i = 0; i < TAG_SIZE; i++)
				_addr_main[i + OFF_SIZE + SET_SIZE] = _tag[i];
		}

		void set_way(ap_uint<(WAY_SIZE > 0) ? WAY_SIZE : 1> way) {
			_way = way;

			_addr_line = 0;
			for (auto i = 0; i < WAY_SIZE; i++)
				_addr_line[i] = _way[i];
			for (auto i = 0; i < SET_SIZE; i++)
				_addr_line[i + WAY_SIZE] = _set[i];

			_addr_cache = 0;
			for (auto i = 0; i < OFF_SIZE; i++)
				_addr_cache[i] = _off[i];
			for (auto i = 0; i < LINE_ADDR_SIZE; i++)
				_addr_cache[i + OFF_SIZE] = _addr_line[i];
		}
};

#endif /* ADDRESS_H */

