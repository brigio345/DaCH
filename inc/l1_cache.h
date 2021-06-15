#ifndef L1_CACHE_H
#define L1_CACHE_H

#include "ap_int.h"
#include "address.h"

template <typename line_t, size_t ADDR_SIZE, size_t TAG_SIZE, size_t N_ENTRIES_PER_LINE>
class l1_cache {
	private:
		typedef address<ADDR_SIZE, TAG_SIZE, 0, 0> l1_addr_t;

		bool _valid;
		line_t _line;
		ap_uint<TAG_SIZE> _tag;

	public:
		void init() {
			_valid = false;
		}

		bool get_line(ap_uint<ADDR_SIZE> addr_main, line_t &line) {
			l1_addr_t addr(addr_main);

			for (auto off = 0; off < N_ENTRIES_PER_LINE; off++)
				line[off] = _line[off];

			return hit(addr);
		}

		void set_line(ap_uint<ADDR_SIZE> addr_main, line_t &line) {
			for (auto off = 0; off < N_ENTRIES_PER_LINE; off++)
				_line[off] = line[off];
			l1_addr_t addr(addr_main);
			_valid = true;
			_tag = addr._tag;
		}

		void invalidate_line(ap_uint<ADDR_SIZE> addr_main) {
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

