#ifndef CACHE_WO_H
#define CACHE_WO_H

#include "address.h"
#define HLS_STREAM_THREAD_SAFE
#include "hls_stream.h"
#include "ap_int.h"

// direct mapping, write back
// TODO: support different policies through virtual functions
// TODO: use more friendly template parameters:
// 	LINE_SIZE -> N_LINES; TAG_SIZE -> CACHE_LINE_SIZE
template <typename T, size_t ADDR_SIZE = 32, size_t LINE_SIZE = 3,
		size_t OFF_SIZE = 2, size_t N_PORTS = 1>
class cache_wo {
	private:
		static const size_t TAG_SIZE = ADDR_SIZE - (LINE_SIZE + OFF_SIZE);
		static const size_t N_LINES = 1 << LINE_SIZE;
		static const size_t N_ENTRIES_PER_LINE = 1 << OFF_SIZE;

		hls::stream<T, 2 * N_PORTS> _wr_data[N_PORTS];
		hls::stream<ap_int<ADDR_SIZE>, 2 * N_PORTS> _wr_addr[N_PORTS];
		bool _valid[N_LINES];
		bool _dirty[N_LINES];
		ap_uint<TAG_SIZE> _tag[N_LINES];
		T _cache_mem[N_LINES * N_ENTRIES_PER_LINE];
		int _curr_req_port;

		typedef address<ADDR_SIZE, TAG_SIZE, LINE_SIZE, OFF_SIZE, N_ENTRIES_PER_LINE>
			addr_t;

	public:
		cache_wo() {
#pragma HLS array_partition variable=_valid complete dim=1
#pragma HLS array_partition variable=_dirty complete dim=1
#pragma HLS array_partition variable=_tag complete dim=1
#pragma HLS array_partition variable=_cache_mem cyclic factor=N_ENTRIES_PER_LINE dim=1
			_curr_req_port = 0;
		}

		void operate(T *main_mem) {
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
				if (_wr_addr[curr_port].empty())
					continue;
#endif /* __SYNTHESIS__ */

				// get request
				dep = _wr_addr[curr_port].read_dep(addr_main, false);
				// stop if request is "end-of-request"
				if (addr_main < 0)
					break;

				// extract information from address
				addr_t addr(addr_main);

				// prepare the cache for accessing addr
				// (load the line if not present)
				if (!hit(addr))
					fill(main_mem, addr);

				// store received data to cache
				_wr_data[curr_port].read_dep(data, dep);
				_cache_mem[addr._addr_cache] = data;

				_dirty[addr._line] = true;

				curr_port = (curr_port + 1) % N_PORTS;
			}

			flush(main_mem);
		}

		void stop_operation() {
			for (int port = 0; port < N_PORTS; port++) {
				_wr_addr[port].write(-1);
			}
		}

	private:
		inline bool hit(addr_t addr) {
			return (_valid[addr._line] && (addr._tag == _tag[addr._line]));
		}

		// load line from main to cache memory
		// (taking care of writing back dirty lines)
		void fill(T *main_mem, addr_t addr) {
#pragma HLS inline
			if (_valid[addr._line] && _dirty[addr._line])
				spill(main_mem, addr_t::build(_tag[addr._line], addr._line));

FILL_LOOP:		for (int off = 0; off < N_ENTRIES_PER_LINE; off++) {
				_cache_mem[addr._addr_cache_first_of_line + off] =
					main_mem[addr._addr_main_first_of_line + off];
			}

			_tag[addr._line] = addr._tag;
			_valid[addr._line] = true;
			_dirty[addr._line] = false;
		}

		// store line from cache to main memory
		void spill(T *main_mem, addr_t addr) {
#pragma HLS inline
SPILL_LOOP:		for (int off = 0; off < N_ENTRIES_PER_LINE; off++) {
				main_mem[addr._addr_main_first_of_line + off] =
					_cache_mem[addr._addr_cache_first_of_line + off];
			}

			_dirty[addr._line] = false;
		}

		// store all valid dirty lines from cache to main memory
		void flush(T *main_mem) {
#pragma HLS inline
FLUSH_LOOP:		for (int line = 0; line < N_LINES; line++) {
				if (_valid[line] && _dirty[line])
					spill(main_mem, addr_t::build(_tag[line], line, 0));
			}
		}

	public:
		void set(ap_uint<ADDR_SIZE> addr_main, T data) {
#pragma HLS inline
			bool dep;

			dep = _wr_addr[_curr_req_port].write_dep(addr_main, false);
			_wr_data[_curr_req_port].write_dep(data, dep);

			_curr_req_port = (_curr_req_port + 1) % N_PORTS;
		}

		class inner {
			private:
				cache_wo *_cache;
				ap_uint<ADDR_SIZE> _addr_main;
			public:
				inner(cache_wo *c, ap_uint<ADDR_SIZE> addr_main):
					_cache(c), _addr_main(addr_main) {}

				void operator=(T data) {
#pragma HLS inline
					_cache->set(_addr_main, data);
				}
		};

	public:
		inner operator[](const int addr_main) {
#pragma HLS inline
			return inner(this, addr_main);
		}
};

#endif /* CACHE_WO_H */

