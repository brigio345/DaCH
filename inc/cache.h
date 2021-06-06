#ifndef CACHE_H
#define CACHE_H

#include "address.h"
#include "l1_cache.h"
#include "raw_cache.h"
#define HLS_STREAM_THREAD_SAFE
#include "hls_stream.h"
#include "ap_int.h"
#include "ap_utils.h"
#include "utils.h"
#ifndef __SYNTHESIS__
#include <thread>
#endif /* __SYNTHESIS__ */

// direct mapping, write back
template <typename T, bool RD_ENABLED, bool WR_ENABLED, size_t MAIN_SIZE,
	 size_t N_LINES, size_t N_ENTRIES_PER_LINE>
class cache {
	private:
		static const size_t ADDR_SIZE = utils::log2_ceil(MAIN_SIZE);
		static const size_t LINE_SIZE = utils::log2_ceil(N_LINES);
		static const size_t OFF_SIZE = utils::log2_ceil(N_ENTRIES_PER_LINE);
		static const size_t TAG_SIZE = (ADDR_SIZE - (LINE_SIZE + OFF_SIZE));

		static_assert(((1 << LINE_SIZE) == N_LINES),
				"N_LINES must be a power of 2");
		static_assert(((1 << OFF_SIZE) == N_ENTRIES_PER_LINE),
				"N_ENTRIES_PER_LINE must be a power of 2");
		static_assert(((MAIN_SIZE > (N_LINES * N_ENTRIES_PER_LINE)) && (TAG_SIZE > 0)),
				"N_LINES and/or N_ENTRIES_PER_LINE are too big for the specified MAIN_SIZE");
		static_assert(((MAIN_SIZE % N_ENTRIES_PER_LINE) == 0),
				"MAIN_SIZE must be a multiple of N_ENTRIES_PER_LINE");

		typedef address<ADDR_SIZE, TAG_SIZE, LINE_SIZE> addr_t;
		typedef struct {
			T v[N_ENTRIES_PER_LINE];
		} line_t;
		typedef l1_cache<line_t, ADDR_SIZE, (TAG_SIZE + LINE_SIZE),
				N_ENTRIES_PER_LINE> l1_cache_t;
		typedef raw_cache<T, line_t, ADDR_SIZE, (TAG_SIZE + LINE_SIZE),
				N_ENTRIES_PER_LINE> raw_cache_t;

		typedef enum {
			READ_REQ, WRITE_REQ, STOP_REQ
		} request_type_t;

#if (defined(__PROFILE__) && (!defined(__SYNTHESIS__)))
	public:
		typedef int hit_status_t;
#define MISS 0
#define HIT 1
#define L1_HIT 2

	private:
#endif /* (defined(__PROFILE__) && (!defined(__SYNTHESIS__))) */

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

		hls::stream<line_t, 128> _rd_data;
		hls::stream<request_t, 128> _request;
#if (defined(__PROFILE__) && (!defined(__SYNTHESIS__)))
		hls::stream<hit_status_t, 128> _hit_status;
#endif /* (defined(__PROFILE__) && (!defined(__SYNTHESIS__))) */
		hls::stream<line_t, 128> _fill_data;
		hls::stream<mem_req_t, 128> _if_request;
		bool _valid[N_LINES];
		bool _dirty[N_LINES];
		ap_uint<TAG_SIZE> _tag[N_LINES];
		T _cache_mem[N_LINES * N_ENTRIES_PER_LINE];
		l1_cache_t _l1_cache_get;
		raw_cache_t _raw_cache_core;

	public:
		cache() {
#pragma HLS array_partition variable=_valid complete dim=1
#pragma HLS array_partition variable=_dirty complete dim=1
#pragma HLS array_partition variable=_tag complete dim=1
#pragma HLS array_partition variable=_cache_mem cyclic factor=N_ENTRIES_PER_LINE dim=1
		}

		void init() {
			_l1_cache_get.init();
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
			_request.write({0, STOP_REQ, 0});
		}

#if (defined(__PROFILE__) && (!defined(__SYNTHESIS__)))
		void get_line(ap_uint<ADDR_SIZE> addr_main, line_t &line,
				hit_status_t &hit_status)
#else
		void get_line(ap_uint<ADDR_SIZE> addr_main, line_t &line)
#endif /* (defined(__PROFILE__) && (!defined(__SYNTHESIS__))) */
		{
#pragma HLS inline
#ifndef __SYNTHESIS__
			if (addr_main >= MAIN_SIZE) {
				throw std::out_of_range("cache::get: address " +
						std::to_string(addr_main) +
						" is out of range");
			}
#endif /* __SYNTHESIS__ */

			if (!_l1_cache_get.get_line(addr_main, line)) {
				_request.write({addr_main, READ_REQ, 0});
				ap_wait_n(6);
				_rd_data.read(line);
#if (defined(__PROFILE__) && (!defined(__SYNTHESIS__)))
				_hit_status.read(hit_status);
#endif /* (defined(__PROFILE__) && (!defined(__SYNTHESIS__))) */

				_l1_cache_get.fill_line(addr_main, line);
			}
#if (defined(__PROFILE__) && (!defined(__SYNTHESIS__)))
			else {
				hit_status = L1_HIT;
			}
#endif /* (defined(__PROFILE__) && (!defined(__SYNTHESIS__))) */
		}

#if (defined(__PROFILE__) && (!defined(__SYNTHESIS__)))
		T get(ap_uint<ADDR_SIZE> addr_main, hit_status_t &hit_status)
#else
		T get(ap_uint<ADDR_SIZE> addr_main)
#endif /* (defined(__PROFILE__) && (!defined(__SYNTHESIS__))) */
		{
#pragma HLS inline
			line_t line;
#if (defined(__PROFILE__) && (!defined(__SYNTHESIS__)))
			get_line(addr_main, line, hit_status);
#else
			get_line(addr_main, line);
#endif /* (defined(__PROFILE__) && (!defined(__SYNTHESIS__))) */
			addr_t addr(addr_main);
			return line.v[addr._off];
		}

#if (defined(__PROFILE__) && (!defined(__SYNTHESIS__)))
		void set(ap_uint<ADDR_SIZE> addr_main, T data,
				hit_status_t &hit_status)
#else
		void set(ap_uint<ADDR_SIZE> addr_main, T data)
#endif /* (defined(__PROFILE__) && (!defined(__SYNTHESIS__))) */
		{
#pragma HLS inline
#ifndef __SYNTHESIS__
			if (addr_main >= MAIN_SIZE) {
				throw std::out_of_range("cache::set: address " +
						std::to_string(addr_main) +
						" is out of range");
			}
#endif /* __SYNTHESIS__ */

			_l1_cache_get.invalidate_line(addr_main);

			_request.write({addr_main, WRITE_REQ, data});
#if (defined(__PROFILE__) && (!defined(__SYNTHESIS__)))
			_hit_status.read(hit_status);
#endif /* (defined(__PROFILE__) && (!defined(__SYNTHESIS__))) */
		}


	private:
		void run_core() {
#pragma HLS inline off
			request_t req;
			line_t line;
			T data;

			// invalidate all cache lines
			for (int line = 0; line < N_LINES; line++)
				_valid[line] = false;

			_raw_cache_core.init();

CORE_LOOP:		while (1) {
#pragma HLS pipeline
#pragma HLS dependence variable=_cache_mem distance=1 inter RAW false
#ifdef __SYNTHESIS__
				// make pipeline flushable
				if (!_request.read_nb(req))
					continue;
#else
				// get request
				_request.read(req);
#endif /* __SYNTHESIS__ */

				// stop if request is "end-of-request"
				if (req.type == STOP_REQ)
					break;

				// extract information from address
				addr_t addr(req.addr_main);

				bool is_hit = hit(addr);
				bool write = ((WR_ENABLED && (req.type == WRITE_REQ)) ||
							(!RD_ENABLED));

				if (is_hit)
					_raw_cache_core.get_line(_cache_mem, addr._addr_cache, line);
				else
					fill(addr, write, req.data, line);

				if (write) {
					if (is_hit) {
						line.v[addr._off] = req.data;
						_raw_cache_core.set_line(_cache_mem, addr._addr_cache, line);
					}
					_dirty[addr._line] = true;
				} else {
					_rd_data.write(line);
				}

#if (defined(__PROFILE__) && (!defined(__SYNTHESIS__)))
				_hit_status.write(is_hit ? HIT : MISS);
#endif /* (defined(__PROFILE__) && (!defined(__SYNTHESIS__))) */
			}

			if (WR_ENABLED)
				flush();

			ap_wait();
			_if_request.write({false, false, 0, 0, 0});
		}

		void run_mem_if(T *main_mem) {
			mem_req_t req;
			T *main_line;
			line_t line;
			raw_cache_t raw_cache_mem_if;
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
				
				if (WR_ENABLED && req.spill) {
					raw_cache_mem_if.set_line(main_mem, req.spill_addr, req.line);
				}
			}
		}

		inline bool hit(addr_t addr) {
			return (_valid[addr._line] && (addr._tag == _tag[addr._line]));
		}

		// load line from main to cache memory
		// (taking care of writing back dirty lines)
		void fill(addr_t addr, bool write, T data, line_t &line) {
#pragma HLS inline
			bool do_spill = false;
			addr_t spill_addr(_tag[addr._line], addr._line, 0);
			if (WR_ENABLED && _valid[addr._line] && _dirty[addr._line]) {
				spill_core(spill_addr, line);
				do_spill = true;
			}

			_if_request.write({true, do_spill, addr._addr_main, 
					spill_addr._addr_main, line});
			ap_wait();

			_fill_data.read(line);

			if (write)
				line.v[addr._off] = data;

			_raw_cache_core.set_line(_cache_mem, addr._addr_cache_first_of_line, line);

			_tag[addr._line] = addr._tag;
			_valid[addr._line] = true;
			_dirty[addr._line] = false;
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

