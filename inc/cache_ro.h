#ifndef CACHE_RO_H
#define CACHE_RO_H

#include "stream_dep.h"
#include "ap_int.h"

// direct mapping, write back
// TODO: support different policies through virtual functions
// TODO: use more friendly template parameters:
// 	LINE_SIZE -> N_LINES; TAG_SIZE -> CACHE_LINE_SIZE
template <typename T, size_t ADDR_SIZE = 32, size_t TAG_SIZE = 28, size_t LINE_SIZE = 2, size_t N_PORTS = 1>
class cache_ro {
	private:
		static const size_t OFF_SIZE = ADDR_SIZE - (TAG_SIZE + LINE_SIZE);
		static const size_t N_LINES = 1 << LINE_SIZE;
		static const size_t N_ENTRIES_PER_LINE = 1 << OFF_SIZE;

		stream_dep<T, N_PORTS> _rd_data[N_PORTS];
		stream_dep<ap_int<ADDR_SIZE>, N_PORTS> _rd_addr[N_PORTS];
		ap_uint<N_LINES> _valid;
		ap_uint<TAG_SIZE> _tag[N_LINES];
		T _cache_mem[N_LINES * N_ENTRIES_PER_LINE];
		T * const _main_mem;
		bool _dep;

	public:
		cache_ro(T * const main_mem): _main_mem(main_mem) {
#pragma HLS array_partition variable=_tag complete dim=1
#pragma HLS array_partition variable=_cache_mem complete dim=1
		}

		bool operate_body() {
#pragma HLS inline
#pragma HLS pipeline enable_flush
			static int curr_port = 0;
			ap_int<ADDR_SIZE> addr_main;
			T data;

			// get request
			_rd_addr[curr_port].read(addr_main);
			// stop if request is "end-of-request"
			if (addr_main < 0)
				return false;

			// extract information from address
			address addr(addr_main);

			// prepare the cache for accessing addr
			// (load the line if not present)
			if (!hit(addr))
				fill(addr);

			// read data from cache
			data = _cache_mem[addr._addr_cache];

			// send read data
			_rd_data[curr_port].write(data);

			curr_port = (curr_port + 1) % N_PORTS;

			return true;
		}

		void operate() {
			// invalidate all cache lines
			_valid = 0;

OPERATE_LOOP:		while (operate_body());
		}

		void stop_operation() {
			for (int port = 0; port < N_PORTS; port++) {
				_rd_addr[port].write(-1);
			}
		}

	private:
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
					_addr_cache_first_of_line = _addr_cache - _off;
					_addr_main_first_of_line = addr_main - _off;
				}

				static address build(ap_uint<TAG_SIZE> tag,
						ap_uint<LINE_SIZE> line,
						ap_uint<OFF_SIZE> off = 0) {
					return address((tag, line, off));
				}
		};

		inline bool hit(address addr) {
			return (_valid[addr._line] && (addr._tag == _tag[addr._line]));
		}

		// load line from main to cache memory
		void fill(address addr) {
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

			_dep = _rd_addr[curr_port].write_dep(addr_main, _dep);
			_dep = _rd_data[curr_port].read_dep(data, _dep);

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

