#ifndef RAW_CACHE_H
#define RAW_CACHE_H

#include "ap_int.h"
#include "ap_utils.h"
#include "hls_vector.h"
#include "address.h"

template <typename T, size_t ADDR_SIZE, size_t TAG_SIZE, size_t N_ENTRIES_PER_LINE>
class raw_cache {
	private:
		static const size_t OFF_SIZE = (ADDR_SIZE - TAG_SIZE);

		typedef address<ADDR_SIZE, TAG_SIZE, 0> raw_addr_t;
		typedef hls::vector<T, N_ENTRIES_PER_LINE> line_t;

		bool _valid;
		line_t _line;
		ap_uint<TAG_SIZE> _tag;

	public:
		void init() {
			_valid = false;
		}

		void get_line(T *main_mem, ap_uint<ADDR_SIZE> addr_main, line_t &line) {
#pragma HLS inline
			raw_addr_t addr(addr_main);

			bool is_hit = hit(addr);

			if (is_hit) {
				for (auto off = 0; off < N_ENTRIES_PER_LINE; off++)
					line[off] = _line[off];
			}

			ap_wait();	

			if (!is_hit) {
				T *main_line = &(main_mem[addr._addr_main & (-1U << OFF_SIZE)]);
				for (auto off = 0; off < N_ENTRIES_PER_LINE; off++)
					line[off] = main_line[off];
			}
		}

		void set_line(T *main_mem, ap_uint<ADDR_SIZE> addr_main, line_t &line) {
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

#endif /* RAW_CACHE_H */

