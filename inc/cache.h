#ifndef CACHE_H
#define CACHE_H

#include "stream_dep.h"
#include "ap_int.h"

// direct mapping, write back
// TODO: support different policies through virtual functions
// TODO: use more friendly template parameters:
// 	LINE_SIZE -> N_LINES; TAG_SIZE -> CACHE_LINE_SIZE
template <typename T, size_t ADDR_SIZE = 32, size_t TAG_SIZE = 28, size_t LINE_SIZE = 2,
		size_t RD_PORTS = 1, size_t WR_PORTS = 1>
class cache {
	private:
		typedef enum {
			READ_E, WRITE_E, EOR_E
		} request_type_t;

		typedef struct {
			ap_uint<ADDR_SIZE> addr_main;
			request_type_t type;
		} request_t;

		static const size_t N_PORTS = RD_PORTS + WR_PORTS;
		static const size_t OFF_SIZE = ADDR_SIZE - (TAG_SIZE + LINE_SIZE);
		static const size_t N_LINES = 1 << LINE_SIZE;
		static const size_t N_ENTRIES_PER_LINE = 1 << OFF_SIZE;

		stream_dep<T> _rd_data[RD_PORTS];
		stream_dep<T> _wr_data[WR_PORTS];
		stream_dep<request_t> _request[N_PORTS];
		ap_uint<N_LINES> _valid;
		ap_uint<N_LINES> _dirty;
		ap_uint<TAG_SIZE> _tag[N_LINES];
		T _cache_mem[N_LINES * N_ENTRIES_PER_LINE];
		T * const _main_mem;
		bool _dep;
		int _req_port;

	public:
		cache(T * const main_mem): _main_mem(main_mem) {
#pragma HLS array_partition variable=_tag complete dim=1
#pragma HLS array_partition variable=_cache_mem complete dim=1
#pragma HLS stream depth=RD_PORTS variable=_rd_data
#pragma HLS stream depth=WR_PORTS variable=_wr_data
#pragma HLS stream depth=N_PORTS variable=_request
			// invalidate all cache lines
			_valid = 0;
			_req_port = 0;
		}

		~cache() {
			flush();
		}

		void operate() {
#pragma HLS inline
			int req_port = 0;
			int rd_port = 0;
			int wr_port = 0;
			request_t req;
			T data;

OPERATE_LOOP:		while (1) {
#pragma HLS pipeline
				// get request
				_request[req_port].read(req);
				// stop if request is "end-of-request"
				if (req.type == EOR_E)
					break;

				// extract information from address
				address addr(req.addr_main);

				// prepare the cache for accessing addr
				// (load the line if not present)
				if (!hit(addr))
					fill(addr);

				if (req.type == READ_E) {
					// read data from cache
					data = _cache_mem[addr._addr_cache];

					// send read data
					_rd_data[rd_port].write(data);

					rd_port = (rd_port + 1) % RD_PORTS;
				} else {
					// store received data to cache
					_wr_data[wr_port].read(data);
					_cache_mem[addr._addr_cache] = data;

					_dirty[addr._line] = true;

					wr_port = (wr_port + 1) % WR_PORTS;
				}

				req_port = (req_port + 1) % N_PORTS;
			}
		}

		void stop_operation() {
			for (int port = 0; port < N_PORTS; port++) {
#pragma HLS unroll
				_request[port].write((request_t){0, EOR_E});
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
		// (taking care of writing back dirty lines)
		void fill(address addr) {
#pragma HLS inline
			if (_valid[addr._line] && _dirty[addr._line])
				spill(address::build(_tag[addr._line], addr._line));

FILL_LOOP:		for (int off = 0; off < N_ENTRIES_PER_LINE; off++) {
#pragma HLS unroll
				_cache_mem[addr._addr_cache_first_of_line + off] =
					_main_mem[addr._addr_main_first_of_line + off];
			}

			_tag[addr._line] = addr._tag;
			_valid[addr._line] = true;
			_dirty[addr._line] = false;
		}

		// store line from cache to main memory
		void spill(address addr) {
#pragma HLS inline
SPILL_LOOP:		for (int off = 0; off < N_ENTRIES_PER_LINE; off++) {
#pragma HLS unroll
				_main_mem[addr._addr_main_first_of_line + off] =
					_cache_mem[addr._addr_cache_first_of_line + off];
			}

			_dirty[addr._line] = false;
		}

		// store all valid dirty lines from cache to main memory
		void flush() {
#pragma HLS inline
FLUSH_LOOP:		for (int line = 0; line < N_LINES; line++) {
#pragma HLS pipeline
				if (_valid[line] && _dirty[line])
					spill(address::build(_tag[line], line, 0));
			}
		}

		T get(ap_uint<ADDR_SIZE> addr_main) {
#pragma HLS inline
			static int rd_port = 0;
			T data;

			_dep = _request[_req_port].write_dep(
				(request_t){addr_main, READ_E}, _dep);
			_dep = _rd_data[rd_port].read_dep(data, _dep);

			rd_port = (rd_port + 1) % RD_PORTS;
			_req_port = (_req_port + 1) % N_PORTS;

			return data;
		}

		void set(ap_uint<ADDR_SIZE> addr_main, T data) {
#pragma HLS inline
			static int wr_port = 0;

			_dep = _request[_req_port].write_dep(
				(request_t){addr_main, WRITE_E}, _dep);
			_dep = _wr_data[wr_port].write_dep(data, _dep);

			wr_port = (wr_port + 1) % WR_PORTS;
			_req_port = (_req_port + 1) % N_PORTS;
		}

		class inner {
			private:
				cache *_cache;
				ap_uint<ADDR_SIZE> _addr_main;
			public:
				inner(cache *c, ap_uint<ADDR_SIZE> addr_main):
					_cache(c), _addr_main(addr_main) {}

				operator T() const {
#pragma HLS inline
					return _cache->get(_addr_main);
				}

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

#endif /* CACHE_H */

