#ifndef CACHE_H
#define CACHE_H

#include "address.h"
#define HLS_STREAM_THREAD_SAFE
#include "hls_stream.h"
#include "ap_int.h"
#include "ap_utils.h"

// direct mapping, write back
// TODO: support different policies through virtual functions
// TODO: use more friendly template parameters:
// 	LINE_SIZE -> N_LINES; TAG_SIZE -> CACHE_LINE_SIZE
template <typename T, size_t RD_PORTS, size_t WR_PORTS,
	 size_t ADDR_SIZE = 32, size_t LINE_SIZE = 3, size_t OFF_SIZE = 5>
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
		static const size_t TAG_SIZE = ADDR_SIZE - (LINE_SIZE + OFF_SIZE);
		static const size_t N_LINES = 1 << LINE_SIZE;
		static const size_t N_ENTRIES_PER_LINE = 1 << OFF_SIZE;

		hls::stream<T, RD_PORTS> _rd_data[RD_PORTS];
		hls::stream<T, WR_PORTS> _wr_data[WR_PORTS];
		hls::stream<request_t, N_PORTS> _request[N_PORTS];
		bool _valid[N_LINES];
		bool _dirty[N_LINES];
		ap_uint<TAG_SIZE> _tag[N_LINES];
		T _cache_mem[N_LINES * N_ENTRIES_PER_LINE];
		int _client_req_port;
		int _client_rd_port;
		int _client_wr_port;

		typedef address<ADDR_SIZE, TAG_SIZE, LINE_SIZE, OFF_SIZE, N_ENTRIES_PER_LINE>
			addr_t;

	public:
		cache() {
#pragma HLS array_partition variable=_valid complete dim=1
#pragma HLS array_partition variable=_dirty complete dim=1
#pragma HLS array_partition variable=_tag complete dim=1
#pragma HLS array_partition variable=_cache_mem cyclic factor=N_ENTRIES_PER_LINE dim=1
			_client_req_port = 0;
			_client_rd_port = 0;
			_client_wr_port = 0;
		}

		void operate(T *main_mem) {
#pragma HLS inline
			request_t req;
			T data;
			int req_port = 0;
			int rd_port = 0;
			int wr_port = 0;

			// invalidate all cache lines
			for (int line = 0; line < N_LINES; line++)
				_valid[line] = false;

OPERATE_LOOP:		while (1) {
				bool dep;
#pragma HLS pipeline
#ifdef __SYNTHESIS__
				// make pipeline flushable
				if (_request[req_port].empty())
					continue;
#endif /* __SYNTHESIS__ */

				// get request
				dep = _request[req_port].read_dep(req, false);
				// stop if request is "end-of-request"
				if (req.type == EOR_E)
					break;

				// extract information from address
				addr_t addr(req.addr_main);

				// prepare the cache for accessing addr
				// (load the line if not present)
				if (!hit(addr))
					fill(main_mem, addr);

				if ((RD_PORTS > 0) && (req.type == READ_E)) {
					// read data from cache
					data = _cache_mem[addr._addr_cache];

					// send read data
					_rd_data[rd_port].write_dep(data, dep);

					rd_port = (rd_port + 1) % RD_PORTS;
				} else if (WR_PORTS > 0) {
					// store received data to cache
					_wr_data[wr_port].read_dep(data, dep);
					_cache_mem[addr._addr_cache] = data;

					_dirty[addr._line] = true;

					wr_port = (wr_port + 1) % WR_PORTS;
				}

				req_port = (req_port + 1) % N_PORTS;
			}

			if (WR_PORTS > 0)
				flush(main_mem);
		}

		void stop_operation() {
			for (int port = 0; port < N_PORTS; port++) {
				_request[port].write((request_t){0, EOR_E});
			}
		}

		T get(ap_uint<ADDR_SIZE> addr_main) {
#pragma HLS inline
			T data;
			bool dep;

			dep = _request[_client_req_port].write_dep(
				(request_t){addr_main, READ_E}, false);
			ap_wait();
			_rd_data[_client_rd_port].read_dep(data, dep);

			_client_rd_port = (_client_rd_port + 1) % RD_PORTS;
			_client_req_port = (_client_req_port + 1) % N_PORTS;

			return data;
		}

		void set(ap_uint<ADDR_SIZE> addr_main, T data) {
#pragma HLS inline
			bool dep;

			dep = _request[_client_req_port].write_dep(
				(request_t){addr_main, WRITE_E}, false);
			ap_wait();
			_wr_data[_client_wr_port].write_dep(data, dep);

			_client_wr_port = (_client_wr_port + 1) % WR_PORTS;
			_client_req_port = (_client_req_port + 1) % N_PORTS;
		}

		// store all valid dirty lines from cache to main memory
		void flush(T *main_mem) {
#pragma HLS inline
FLUSH_LOOP:		for (int line = 0; line < N_LINES; line++) {
				if (_valid[line] && _dirty[line])
					spill(main_mem, addr_t::build(_tag[line], line, 0));
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
			if ((WR_PORTS > 0) && _valid[addr._line] && _dirty[addr._line])
				spill(main_mem, addr_t::build(_tag[addr._line], addr._line));

			T *cache_line = &(_cache_mem[addr._addr_cache_first_of_line]);
			T *main_line = &(main_mem[addr._addr_main_first_of_line]);

FILL_LOOP:		for (int off = 0; off < N_ENTRIES_PER_LINE; off++) {
				cache_line[off] = main_line[off];
			}

			_tag[addr._line] = addr._tag;
			_valid[addr._line] = true;
			_dirty[addr._line] = false;
		}

		// store line from cache to main memory
		void spill(T *main_mem, addr_t addr) {
#pragma HLS inline
			T *cache_line = &(_cache_mem[addr._addr_cache_first_of_line]);
			T *main_line = &(main_mem[addr._addr_main_first_of_line]);

SPILL_LOOP:		for (int off = 0; off < N_ENTRIES_PER_LINE; off++) {
#pragma HLS dependence inter variable=main_mem false
#pragma HLS dependence inter variable=cache_mem false
				main_line[off] = cache_line[off];
			}

			_dirty[addr._line] = false;
		}

#ifndef __SYNTHESIS__
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
#endif /* __SYNTHESIS__ */
};

#endif /* CACHE_H */

