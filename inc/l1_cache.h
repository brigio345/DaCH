#ifndef L1_CACHE_H
#define L1_CACHE_H

#include "address.h"

template <typename line_t, size_t ADDR_SIZE, size_t TAG_SIZE, size_t N_ENTRIES_PER_LINE>
class l1_cache {
	private:
		typedef address<ADDR_SIZE, TAG_SIZE, 0, 0> l1_addr_t;

		bool _valid;
		line_t _line;
		unsigned int _tag;

	public:
		l1_cache() {
#pragma HLS reset variable=_valid
#ifndef __SYNTHESIS__
			_valid = false;
#endif /* __SYNTHESIS__ */
		}

		bool get_line(unsigned int addr_main, line_t &line) {
			l1_addr_t addr(addr_main);

			for (auto off = 0; off < N_ENTRIES_PER_LINE; off++)
				line[off] = _line[off];

			return hit(addr);
		}

		void set_line(unsigned int addr_main, line_t &line) {
			for (auto off = 0; off < N_ENTRIES_PER_LINE; off++)
				_line[off] = line[off];
			l1_addr_t addr(addr_main);
			_valid = true;
			_tag = addr._tag;
		}

		void invalidate_line(unsigned int addr_main) {
			l1_addr_t addr(addr_main);

			if (hit(addr))
				_valid = false;
		}

	private:
		inline bool hit(l1_addr_t addr) {
			return (_valid && (addr._tag == _tag));
		}
};

#endif /* L1_CACHE_H */

