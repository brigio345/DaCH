#ifndef CACHE_RO_H
#define CACHE_RO_H

#include "address.h"
#include "stream_dep.h"
#include "ap_int.h"

// direct mapping, write back
// TODO: support different policies through virtual functions
// TODO: use more friendly template parameters:
// 	LINE_SIZE -> N_LINES; TAG_SIZE -> CACHE_LINE_SIZE
template <typename T, size_t ADDR_SIZE = 32, size_t LINE_SIZE = 1,
		size_t OFF_SIZE = 2, size_t N_PORTS = 1>
class cache_ro {
	private:
		static const size_t TAG_SIZE = ADDR_SIZE - (LINE_SIZE + OFF_SIZE);
		static const size_t N_LINES = 1 << LINE_SIZE;
		static const size_t N_ENTRIES_PER_LINE = 1 << OFF_SIZE;

		stream_dep<T, 2 * N_PORTS> _rd_data[N_PORTS];
		stream_dep<ap_int<ADDR_SIZE>, 2 * N_PORTS> _rd_addr[N_PORTS];
		bool _valid[N_LINES];
		ap_uint<TAG_SIZE> _tag[N_LINES];
		T _cache_mem[N_LINES * N_ENTRIES_PER_LINE];
		T * const _main_mem;

		typedef address<ADDR_SIZE, TAG_SIZE, LINE_SIZE, OFF_SIZE, N_ENTRIES_PER_LINE>
			addr_t;
	public:
		cache_ro(T * const main_mem): _main_mem(main_mem) {
#pragma HLS array_partition variable=_valid complete dim=1
#pragma HLS array_partition variable=_tag complete dim=1
#pragma HLS array_partition variable=_cache_mem cyclic factor=N_LINES dim=1
		}

		void operate() {
			ap_int<ADDR_SIZE> addr_main;
			T data;
			int curr_port;

			// invalidate all cache lines
			for (int line = 0; line < N_LINES; line++)
				_valid[line] = false;
			curr_port = 0;

OPERATE_LOOP:		while (1) {
				bool dep;
#pragma HLS pipeline
#ifdef __SYNTHESIS__
				// make pipeline flushable
				if (_rd_addr[curr_port].empty())
					continue;
#endif /* __SYNTHESIS__ */

				// get request
				dep = _rd_addr[curr_port].read_dep(addr_main, dep);
				// stop if request is "end-of-request"
				if (addr_main < 0)
					break;

				// extract information from address
				addr_t addr(addr_main);

				// prepare the cache for accessing addr
				// (load the line if not present)
				if (!hit(addr))
					fill(addr);

				// read data from cache
				data = _cache_mem[addr._addr_cache];

				// send read data
				_rd_data[curr_port].write_dep(data, dep);

				curr_port = (curr_port + 1) % N_PORTS;
			}
		}

		void stop_operation() {
			for (int port = 0; port < N_PORTS; port++) {
				_rd_addr[port].write(-1);
			}
		}

	private:
		inline bool hit(addr_t addr) {
			return (_valid[addr._line] && (addr._tag == _tag[addr._line]));
		}

		// load line from main to cache memory
		void fill(addr_t addr) {
#pragma HLS inline
FILL_LOOP:		for (int off = 0; off < N_ENTRIES_PER_LINE; off++) {
				_cache_mem[addr._addr_cache_first_of_line + off] =
					_main_mem[addr._addr_main_first_of_line + off];
			}

			_tag[addr._line] = addr._tag;
			_valid[addr._line] = true;
		}

		T get(ap_uint<ADDR_SIZE> addr_main) {
#pragma HLS inline
			static int curr_port = 0;
			T data;
			bool dep;

			dep = _rd_addr[curr_port].write_dep(addr_main, dep);
			dep = _rd_data[curr_port].read_dep(data, dep);

			curr_port = (curr_port + 1) % N_PORTS;

			return data;
		}

	public:
		T operator[](const int addr_main) {
#pragma HLS inline
			return get(addr_main);
		}
};

#endif /* CACHE_RO_H */

