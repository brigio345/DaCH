#ifndef RAW_CACHE_ARBITER_H
#define RAW_CACHE_ARBITER_H

#include "address.h"
#include "arbiter.h"
#ifdef __SYNTHESIS__
#include "hls_vector.h"
#else
#include <array>
#endif /* __SYNTHESIS__ */

template <typename T, typename ARBITER_T, size_t ADDR_SIZE, size_t TAG_SIZE, size_t N_ENTRIES_PER_LINE>
class raw_cache_arbiter {
	private:
		static const size_t OFF_SIZE = (ADDR_SIZE - TAG_SIZE);

		typedef address<ADDR_SIZE, TAG_SIZE, 0, 0> raw_addr_t;
#ifdef __SYNTHESIS__
		typedef hls::vector<T, N_ENTRIES_PER_LINE> line_t;
#else
		typedef std::array<T, N_ENTRIES_PER_LINE> line_t;
#endif /* __SYNTHESIS__ */

		bool _valid;
		line_t _line;
		unsigned int _tag;

	public:
		void init() {
			_valid = false;
		}

		void get_line(ARBITER_T &arbiter, unsigned int addr_main, line_t &line) {
#pragma HLS inline
			raw_addr_t addr(addr_main);

			if (hit(addr)) {
				for (auto off = 0; off < N_ENTRIES_PER_LINE; off++)
					line[off] = _line[off];
			} else {
				line = arbiter.get(addr_main);
			}
		}

		void set_line(T *main_mem, unsigned int addr_main, line_t &line) {
#pragma HLS inline
			raw_addr_t addr(addr_main);

			T *main_line = &(main_mem[addr._addr_main & (-1U << OFF_SIZE)]);
			for (auto off = 0; off < N_ENTRIES_PER_LINE; off++)
				main_line[off] = _line[off] = line[off];
			
			_tag = addr._tag;
			_valid = true;
		}

	private:
		inline bool hit(raw_addr_t addr) {
#pragma HLS inline
			return (_valid && (addr._tag == _tag));
		}
};

#endif /* RAW_CACHE_ARBITER_H */
