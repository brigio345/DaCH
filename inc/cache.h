#ifndef CACHE_H
#define CACHE_H

#include "address.h"
#define HLS_STREAM_THREAD_SAFE
#include "hls_stream.h"
#include "hls_vector.h"
#include "ap_int.h"
#include "ap_utils.h"
#include "utils.h"
#ifndef __SYNTHESIS__
#include <thread>
#endif /* __SYNTHESIS__ */

// direct mapping, write back
template <typename T, size_t RD_PORTS, size_t WR_PORTS, size_t MAIN_SIZE,
	 size_t N_LINES = 2, size_t N_ENTRIES_PER_LINE = 2>
class cache {
	private:
		static const size_t ADDR_SIZE = utils::log2_ceil(MAIN_SIZE);
		static const size_t LINE_SIZE = utils::log2_ceil(N_LINES);
		static const size_t OFF_SIZE = utils::log2_ceil(N_ENTRIES_PER_LINE);
		static const size_t TAG_SIZE = (ADDR_SIZE - (LINE_SIZE + OFF_SIZE));
		static const size_t N_PORTS = (RD_PORTS + WR_PORTS);

		static_assert((N_PORTS > 0),
				"RD_PORTS or WR_PORTS must be at least 1");
		static_assert(((1 << LINE_SIZE) == N_LINES),
				"N_LINES must be a power of 2");
		static_assert(((1 << OFF_SIZE) == N_ENTRIES_PER_LINE),
				"N_ENTRIES_PER_LINE must be a power of 2");
		static_assert((TAG_SIZE > 0),
				"N_LINES and/or N_ENTRIES_PER_LINE are too big for the specified MAIN_SIZE");
		static_assert(((MAIN_SIZE % N_ENTRIES_PER_LINE) == 0),
				"MAIN_SIZE must be a multiple of N_ENTRIES_PER_LINE");

		typedef address<ADDR_SIZE, TAG_SIZE, LINE_SIZE> addr_t;
		typedef hls::vector<T, N_ENTRIES_PER_LINE> line_t;

		typedef enum {
			READ_REQ, WRITE_REQ, STOP_REQ
		} request_type_t;

		typedef struct {
			ap_uint<ADDR_SIZE> addr_main;
			request_type_t type;
			T data;
		} request_t;

		typedef struct {
			bool fill;
			bool spill;
			ap_uint<ADDR_SIZE> fill_addr;
			ap_uint<ADDR_SIZE> spill_addr;
			line_t line;
		} mem_req_t;

		class raw_cache {
			private:
				static const size_t RAW_TAG_SIZE = (TAG_SIZE + LINE_SIZE);

				typedef address<ADDR_SIZE, RAW_TAG_SIZE, 0> raw_addr_t;

				bool _valid;
				line_t _line;
				ap_uint<RAW_TAG_SIZE> _tag;

			public:
				void init() {
					_valid = false;
				}

				void get_line(T *main_mem, ap_uint<ADDR_SIZE> addr_main, line_t &line) {
#pragma HLS inline
					raw_addr_t addr(addr_main);

					if (hit(addr)) {
						for (auto off = 0; off < N_ENTRIES_PER_LINE; off++)
							line[off] = _line[off];
					} else {
						T *main_line = &(main_mem[addr._addr_main & (-1U << OFF_SIZE)]);
						for (auto off = 0; off < N_ENTRIES_PER_LINE; off++)
							line[off] = main_line[off];
					}
				}

				void set_line(T *main_mem, ap_uint<ADDR_SIZE> addr_main, line_t &line) {
					raw_addr_t addr(addr_main);

					T *main_line = &(main_mem[addr._addr_main & (-1U << OFF_SIZE)]);
					for (auto off = 0; off < N_ENTRIES_PER_LINE; off++)
						main_line[off] = _line[off] = line[off];
					
					_tag = addr._tag;
					_valid = true;
				}

			private:
				inline bool hit(raw_addr_t addr) {
					return (_valid && (addr._tag == _tag));
				}
		};

		hls::stream<T, 128> _rd_data[RD_PORTS];
		hls::stream<request_t, 128> _request[N_PORTS];
		hls::stream<line_t, 128> _fill_data;
		hls::stream<mem_req_t, 128> _if_request;
		bool _valid[N_LINES];
		bool _dirty[N_LINES];
		ap_uint<TAG_SIZE> _tag[N_LINES];
		T _cache_mem[N_LINES * N_ENTRIES_PER_LINE];
		int _client_req_port;
		int _client_rd_port;
		raw_cache _raw_cache_core;

	public:
		cache() {
#pragma HLS array_partition variable=_valid complete dim=1
#pragma HLS array_partition variable=_dirty complete dim=1
#pragma HLS array_partition variable=_tag complete dim=1
#pragma HLS array_partition variable=_cache_mem cyclic factor=N_ENTRIES_PER_LINE dim=1
		}

		void init() {
			_client_req_port = 0;
			_client_rd_port = 0;
		}

		void run(T *main_mem) {
#pragma HLS inline
#ifdef __SYNTHESIS__
			run_core();
			run_mem_if(main_mem);
#else
			std::thread core_thd(&cache::run_core, this);
			std::thread mem_if_thd(&cache::run_mem_if, this, main_mem);

			core_thd.join();
			mem_if_thd.join();
#endif /* __SYNTHESIS__ */
		}

		void stop() {
			for (int port = 0; port < N_PORTS; port++) {
				_request[port].write({0, STOP_REQ, 0});
			}
		}

		T get(ap_uint<ADDR_SIZE> addr_main) {
#pragma HLS inline
#ifndef __SYNTHESIS__
			if (addr_main >= MAIN_SIZE) {
				throw std::out_of_range("cache::get: address " +
						std::to_string(addr_main) +
						" is out of range");
			}
#endif /* __SYNTHESIS__ */

			T data;

			_request[_client_req_port].write({addr_main, READ_REQ, 0});
			ap_wait_n(6);
			_rd_data[_client_rd_port].read(data);

			_client_rd_port = (_client_rd_port + 1) % RD_PORTS;
			_client_req_port = (_client_req_port + 1) % N_PORTS;

			return data;
		}

		void set(ap_uint<ADDR_SIZE> addr_main, T data) {
#pragma HLS inline
#ifndef __SYNTHESIS__
			if (addr_main >= MAIN_SIZE) {
				throw std::out_of_range("cache::set: address " +
						std::to_string(addr_main) +
						" is out of range");
			}
#endif /* __SYNTHESIS__ */

			_request[_client_req_port].write(
				(request_t){addr_main, WRITE_REQ, data});

			_client_req_port = (_client_req_port + 1) % N_PORTS;
		}


	private:
		void run_core() {
#pragma HLS inline off
			request_t req;
			line_t line;
			T data;
			int req_port = 0;
			int rd_port = 0;
#ifdef __PROFILE__
			int n_requests = 0;
			int n_hits = 0;
#endif /* __PROFILE__ */

			// invalidate all cache lines
			for (int line = 0; line < N_LINES; line++)
				_valid[line] = false;

			_raw_cache_core.init();

CORE_LOOP:		while (1) {
#pragma HLS pipeline
#pragma HLS dependence variable=_cache_mem distance=1 inter RAW false
#ifdef __SYNTHESIS__
				// make pipeline flushable
				if (!_request[req_port].read_nb(req))
					continue;
#else
				// get request
				_request[req_port].read(req);
#endif /* __SYNTHESIS__ */

				// stop if request is "end-of-request"
				if (req.type == STOP_REQ)
					break;

				// extract information from address
				addr_t addr(req.addr_main);

				bool is_hit = hit(addr);

#ifdef __PROFILE__
				n_requests++;
				if (is_hit)
					n_hits++;
#endif /* __PROFILE__ */

				if ((WR_PORTS == 0) ||
						((RD_PORTS > 0) && (req.type == READ_REQ))) {
					if (!is_hit) {
						data = fill(addr, false, 0);
					} else {
						// read data from cache
						_raw_cache_core.get_line(_cache_mem, addr._addr_cache, line);

						data = line[addr._off];
					}

					// send read data
					_rd_data[rd_port].write(data);

					rd_port = (rd_port + 1) % RD_PORTS;
				} else if (WR_PORTS > 0) {
					if (!is_hit) {
						fill(addr, true, req.data);
					} else {
						_raw_cache_core.get_line(_cache_mem, addr._addr_cache, line);
						line[addr._off] = req.data;
						_raw_cache_core.set_line(_cache_mem, addr._addr_cache, line);
					}

					_dirty[addr._line] = true;
				}

				req_port = (req_port + 1) % N_PORTS;
			}

#ifdef __PROFILE__
			printf("hit ratio = %.3f (%d / %d)\n",
					(n_hits / ((float) n_requests)),
					n_hits, n_requests);
#endif /* __PROFILE__ */

			if (WR_PORTS > 0)
				flush();

			ap_wait();
			_if_request.write({false, false, 0, 0, 0});
		}

		void run_mem_if(T *main_mem) {
			mem_req_t req;
			T *main_line;
			line_t line;
			raw_cache raw_cache_mem_if;
			raw_cache_mem_if.init();
			
MEM_IF_LOOP:		while (1) {
#pragma HLS pipeline
#pragma HLS dependence variable=main_mem distance=1 inter RAW false
#ifdef __SYNTHESIS__
				if (!_if_request.read_nb(req))
					continue;
#else
				_if_request.read(req);
#endif /* __SYNTHESIS__ */

				if (!req.fill && !req.spill)
					break;

				if (req.fill) {
					raw_cache_mem_if.get_line(main_mem, req.fill_addr, line);
					_fill_data.write(line);
				}
				
				if ((WR_PORTS > 0) && req.spill) {
					raw_cache_mem_if.set_line(main_mem, req.spill_addr, req.line);
				}
			}
		}

		inline bool hit(addr_t addr) {
			return (_valid[addr._line] && (addr._tag == _tag[addr._line]));
		}

		// load line from main to cache memory
		// (taking care of writing back dirty lines)
		T fill(addr_t addr, bool write, T data) {
#pragma HLS inline
			line_t line = 0;
			bool do_spill = false;
			addr_t spill_addr(_tag[addr._line], addr._line, 0);
			if ((WR_PORTS > 0) && _valid[addr._line] && _dirty[addr._line]) {
				spill_core(spill_addr, line);
				do_spill = true;
			}

			_if_request.write({true, do_spill, addr._addr_main, 
					spill_addr._addr_main, line});
			ap_wait();

			_fill_data.read(line);

			if (write)
				line[addr._off] = data;

			_raw_cache_core.set_line(_cache_mem, addr._addr_cache_first_of_line, line);

			_tag[addr._line] = addr._tag;
			_valid[addr._line] = true;
			_dirty[addr._line] = false;

			return line[addr._off];
		}

		void spill_core(addr_t addr, line_t &line) {
#pragma HLS inline
			_raw_cache_core.get_line(_cache_mem, addr._addr_cache_first_of_line, line);
			_dirty[addr._line] = false;
		}

		// store line from cache to main memory
		void spill(addr_t addr) {
#pragma HLS inline
			line_t line;

			spill_core(addr, line);

			_if_request.write({false, true, 0, addr._addr_main, line});
		}

		// store all valid dirty lines from cache to main memory
		void flush() {
#pragma HLS inline
FLUSH_LOOP:		for (int line = 0; line < N_LINES; line++) {
				if (_valid[line] && _dirty[line])
					spill(addr_t(_tag[line], line, 0));
			}
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

