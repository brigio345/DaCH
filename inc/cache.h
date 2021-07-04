/**
 * \file	cache.h
 *
 * \brief 	Cache module compatible with Vitis HLS 2020.2.
 *
 *		Cache module whose characteristics are:
 *			- address mapping: set-associative;
 *			- replacement policy: least recently used;
 *			- write policy: write-back.
 *
 *		Synthesizability is guaranteed with Vitis HLS 2020.2, with II=1
 *		for CORE_LOOP.
 */
#ifndef CACHE_H
#define CACHE_H

#include "address.h"
#include "arbiter.h"
#include "l1_cache.h"
#include "raw_cache.h"
#define HLS_STREAM_THREAD_SAFE
#include "hls_stream.h"
#include "ap_utils.h"
#include "utils.h"
#ifdef __SYNTHESIS__
#include "hls_vector.h"
#else
#include <thread>
#include <array>
#include <cassert>
#endif /* __SYNTHESIS__ */

#if (defined(PROFILE) && (!defined(__SYNTHESIS__)))
#define __PROFILE__
#endif /* (defined(PROFILE) && (!defined(__SYNTHESIS__))) */

template <typename T, size_t RD_PORTS, bool WR_ENABLED, size_t MAIN_SIZE,
	 size_t N_SETS, size_t N_WAYS, size_t N_ENTRIES_PER_LINE, bool L1_CACHE>
class cache {
	private:
		static const bool RD_ENABLED = (RD_PORTS > 0);
		static const size_t ADDR_SIZE = utils::log2_ceil(MAIN_SIZE);
		static const size_t SET_SIZE = utils::log2_ceil(N_SETS);
		static const size_t OFF_SIZE = utils::log2_ceil(N_ENTRIES_PER_LINE);
		static const size_t TAG_SIZE = (ADDR_SIZE - (SET_SIZE + OFF_SIZE));
		static const size_t WAY_SIZE = utils::log2_ceil(N_WAYS);

		static_assert(((1 << SET_SIZE) == N_SETS),
				"N_SETS must be a power of 2");
		static_assert(((1 << OFF_SIZE) == N_ENTRIES_PER_LINE),
				"N_ENTRIES_PER_LINE must be a power of 2");
		static_assert(((1 << WAY_SIZE) == N_WAYS),
				"N_WAYS must be a power of 2");
		static_assert((MAIN_SIZE >= (N_SETS * N_WAYS * N_ENTRIES_PER_LINE)),
				"N_SETS and/or N_WAYS and/or N_ENTRIES_PER_LINE are too big for the specified MAIN_SIZE");
		static_assert(((MAIN_SIZE % N_ENTRIES_PER_LINE) == 0),
				"MAIN_SIZE must be a multiple of N_ENTRIES_PER_LINE");

#ifdef __SYNTHESIS__
		template <typename TYPE, size_t SIZE>
			using array_t = hls::vector<TYPE, SIZE>;
#else
		template <typename TYPE, size_t SIZE>
			using array_t = std::array<TYPE, SIZE>;
#endif /* __SYNTHESIS__ */

		typedef address<ADDR_SIZE, TAG_SIZE, SET_SIZE, WAY_SIZE> addr_t;
		typedef array_t<T, N_ENTRIES_PER_LINE> line_t;
		typedef l1_cache<line_t, ADDR_SIZE, (TAG_SIZE + SET_SIZE),
				N_ENTRIES_PER_LINE> l1_cache_t;
		typedef raw_cache<T, ADDR_SIZE, (TAG_SIZE + SET_SIZE),
				N_ENTRIES_PER_LINE> raw_cache_t;
		typedef arbiter<T, RD_PORTS, N_ENTRIES_PER_LINE> arbiter_t;

		typedef enum {
			READ_REQ,
			WRITE_REQ,
			STOP_REQ
		} request_type_t;

#ifdef __PROFILE__
		typedef enum {
			MISS,
			HIT,
			L1_HIT
		} hit_status_t;
#endif /* __PROFILE__ */

		typedef struct {
			request_type_t type;
			unsigned int addr_main;
		} request_t;

		typedef struct {
			bool load;
			bool write_back;
			unsigned int load_addr;
			unsigned int write_back_addr;
			line_t line;
		} mem_req_t;

		unsigned int _tag[N_SETS * N_WAYS];
		bool _valid[N_SETS * N_WAYS];
		bool _dirty[N_SETS * N_WAYS];
		unsigned int _lru[N_SETS][N_WAYS];
		T _cache_mem[N_SETS * N_WAYS * N_ENTRIES_PER_LINE];
		hls::stream<line_t, 4> _rd_data;
		hls::stream<T, 4> _wr_data;
		hls::stream<request_t, 4> _request;
		hls::stream<line_t, 2> _load_data;
		hls::stream<mem_req_t, 2> _if_request;
		l1_cache_t _l1_cache_get;
		raw_cache_t _raw_cache_core;
#ifdef __PROFILE__
		hls::stream<hit_status_t> _hit_status;
		int _n_reqs = 0;
		int _n_hits = 0;
		int _n_l1_hits = 0;
#endif /* __PROFILE__ */

	public:
		cache() {
#pragma HLS array_partition variable=_tag complete dim=1
#pragma HLS array_partition variable=_valid complete dim=1
#pragma HLS array_partition variable=_dirty complete dim=1
#pragma HLS array_partition variable=_lru complete dim=0
#pragma HLS array_partition variable=_cache_mem cyclic factor=N_ENTRIES_PER_LINE dim=1
		}

		/**
		 * \brief	Initialize the cache.
		 *
		 * \note	Must be called before calling \ref run,
		 * 		if the cache is enabled to read.
		 */
		void init() {
			_l1_cache_get.init();
		}

		/**
		 * \brief	Start cache internal processes.
		 *
		 * \note	In case of synthesis this must be called in a
		 * 		dataflow region with disable_start_propagation
		 * 		option, together with the function in which cache
		 * 		is accesses.
		 *
		 * \note	In case of C simulation this must be executed by
		 * 		a thread separated from the thread in which
		 * 		cache is accessed.
		 */
		void run(T *main_mem, unsigned int id = 0, arbiter_t *arbiter = NULL) {
#pragma HLS inline
#ifdef __SYNTHESIS__
			run_core();
			run_mem_if(main_mem, arbiter, arbiter != NULL, id);
#else
			std::thread core_thd([&]{run_core();});
			std::thread mem_if_thd([&]{
					run_mem_if(main_mem, arbiter,
							(arbiter != NULL), id);
					});

			core_thd.join();
			mem_if_thd.join();
#endif /* __SYNTHESIS__ */
		}

		/**
		 * \brief	Stop cache internal processes.
		 *
		 * \note	Must be called after the function in which cache
		 * 		is accessed has completed.
		 */
		void stop() {
			_request.write({STOP_REQ, 0});
		}

		/**
		 * \brief		Request to read a whole cache line.
		 *
		 * \param addr_main	The address in main memory belonging to
		 * 			the cache line to be read.
		 * \param line		The buffer to store the read line.
		 */
		void get_line(unsigned int addr_main, line_t &line) {
#pragma HLS inline
#ifndef __SYNTHESIS__
			assert(addr_main < MAIN_SIZE);
#endif /* __SYNTHESIS__ */

			// try to get line from L1 cache
			auto l1_hit = false;
			if (L1_CACHE)
				l1_hit = _l1_cache_get.get_line(addr_main, line);

			if ((!L1_CACHE) || (!l1_hit)) {
				// send read request to cache
				_request.write({READ_REQ, addr_main});
				// force FIFO write and FIFO read to separate
				// pipeline stages to avoid deadlock due to
				// the blocking read
				// 3 is the read latency in case of HIT:
				// it is put here to inform the scheduler
				// about the latency
				ap_wait_n(3);
				// read response from cache
				_rd_data.read(line);
				if (L1_CACHE) {
					// store line to L1 cache
					_l1_cache_get.set_line(addr_main, line);
				}
			}

#ifdef __PROFILE__
			update_profiling(l1_hit ? L1_HIT : _hit_status.read());
#endif /* __PROFILE__ */
		}

		/**
		 * \brief		Request to read a data element.
		 *
		 * \param addr_main	The address in main memory referring to
		 * 			the data element to be read.
		 *
		 * \return		The read data element.
		 */
		T get(unsigned int addr_main) {
#pragma HLS inline
			line_t line;

			// get the whole cache line
			get_line(addr_main, line);

			// extract information from address
			addr_t addr(addr_main);

			return line[addr._off];
		}

		/**
		 * \brief		Request to write a data element.
		 *
		 * \param addr_main	The address in main memory referring to
		 * 			the data element to be written.
		 * \param data		The data to be written.
		 */
		void set(unsigned int addr_main, T data) {
#pragma HLS inline
#ifndef __SYNTHESIS__
			assert(addr_main < MAIN_SIZE);
#endif /* __SYNTHESIS__ */

			// inform L1 cache about the writing
			_l1_cache_get.invalidate_line(addr_main);

			// send write request to cache
			_request.write({WRITE_REQ, addr_main});
			_wr_data.write(data);
#ifdef __PROFILE__
			update_profiling(_hit_status.read());
#endif /* __PROFILE__ */
		}

#ifdef __PROFILE__
		int get_n_reqs() {
			return _n_reqs;
		}

		int get_n_hits() {
			return _n_hits;
		}

		int get_n_l1_hits() {
			return _n_l1_hits;
		}

		float get_hit_ratio() {
			return ((_n_hits + _n_l1_hits) / ((float) _n_reqs));
		}
#endif /* __PROFILE__ */

	private:
		/**
		 * \brief	Infinite loop managing the cache access requests
		 * 		(sent from the outside).
		 *
		 * \note	The infinite loop must be stopped by calling
		 * 		\ref stop at the end of computation (from the
		 * 		outside).
		 */
		void run_core() {
#pragma HLS inline off
			request_t req;
			line_t line;
			T data;

			// invalidate all cache lines
			for (auto line = 0; line < (N_SETS * N_WAYS); line++)
				_valid[line] = false;

			// initialize way counters
			for (auto set = 0; set < N_SETS; set++) {
				for (auto way = 0; way < N_WAYS; way++)
					_lru[set][way] = way;
			}

			_raw_cache_core.init();

CORE_LOOP:		while (1) {
#pragma HLS pipeline
#pragma HLS dependence variable=_cache_mem distance=1 inter RAW false
#ifdef __SYNTHESIS__
				// get request and
				// make pipeline flushable (to avoid deadlock)
				if (_request.read_nb(req)) {
#else
				// get request
				_request.read(req);
#endif /* __SYNTHESIS__ */

				// exit the loop if request is "end-of-request"
				if (req.type == STOP_REQ)
					break;

				// check the request type
				auto read = ((RD_ENABLED && (req.type == READ_REQ)) ||
						(!WR_ENABLED));

				// in case of write request, read data to be written
				if (!read)
					_wr_data.read(data);

				// extract information from address
				addr_t addr(req.addr_main);

				auto way = hit(addr);
				auto is_hit = (way != -1);

				if (!is_hit)
					way = get_way(addr);

				addr.set_way(way);

				if (is_hit) {
					// read from cache memory
					_raw_cache_core.get_line(_cache_mem,
							addr._addr_cache,
							line);
				} else {
					// read from main memory
					load(addr, line);

					if (read) {
						// store loaded line to cache
						_raw_cache_core.set_line(
								_cache_mem,
								addr._addr_cache,
								line);
					}
				}

				if (read) {
					// send the response to the read request
					_rd_data.write(line);
				} else {
					// modify the line
					line[addr._off] = data;

					// store the modified line to cache
					_raw_cache_core.set_line(_cache_mem,
							addr._addr_cache, line);
					_dirty[addr._addr_line] = true;
				}

#ifdef __PROFILE__
				_hit_status.write(is_hit ? HIT : MISS);
#endif /* __PROFILE__ */
#ifdef __SYNTHESIS__
				}
#endif
			}

			// synchronize main memory with cache memory
			if (WR_ENABLED)
				flush();

			// make sure that flush has completed before stopping
			// memory interface
			ap_wait();
			// stop memory interface
			_if_request.write({false, false, 0, 0, line});
		}

		/**
		 * \brief		Infinite loop managing main memory
		 * 			access requests (sent from \ref run_core).
		 *
		 * \param main_mem	The pointer to the main memory.
		 *
		 * \note		\p main_mem should be associated with
		 * 			a dedicated AXI port in order to get
		 * 			optimal performance.
		 *
		 * \note		The infinite loop is stopped by
		 * 			\ref run_core when it is in turn stopped
		 * 			from the outside.
		 */
		void run_mem_if(T *main_mem, arbiter_t *arbiter, bool arbitrate, unsigned int id) {
#pragma HLS function_instantiate variable=id
#pragma HLS function_instantiate variable=arbitrate
			mem_req_t req;
			T *main_line;
			line_t line;
			raw_cache_t raw_cache_mem_if;

			raw_cache_mem_if.init();
			
MEM_IF_LOOP:		while (1) {
#pragma HLS pipeline
#pragma HLS dependence variable=main_mem distance=1 inter RAW false
#ifdef __SYNTHESIS__
				// get request and
				// make pipeline flushable (to avoid deadlock)
				if (_if_request.read_nb(req)) {
#else
				// get request
				_if_request.read(req);
#endif /* __SYNTHESIS__ */

				// exit the loop if request is "end-of-request"
				if (!req.load && !req.write_back)
					break;

				if (req.load) {
					// read line from main memory
					if (arbitrate)
						line = arbiter->get(req.load_addr, id);
					else
						raw_cache_mem_if.get_line(main_mem, req.load_addr, line);
					// send the response to the read request
					_load_data.write(line);
				}

				if (WR_ENABLED && req.write_back) {
					// write line to main memory
					raw_cache_mem_if.set_line(main_mem,
							req.write_back_addr, req.line);
				}
#ifdef __SYNTHESIS__
				}
#endif
			}

			if (arbiter != NULL)
				arbiter->stop(id);
		}

		/**
		 * \brief	Check if \p addr causes an HIT or a MISS.
		 *
		 * \param addr	The address to be checked.
		 *
		 * \return	hitting way on HIT.
		 * \return	-1 on MISS.
		 */
		inline int hit(addr_t addr) {
			auto hit_way = -1;
			for (auto way = 0; way < N_WAYS; way++) {
				addr.set_way(way);
				if (_valid[addr._addr_line] &&
						(addr._tag == _tag[addr._addr_line])) {
					hit_way = way;
					update_way(addr);
					break;
				}
			}

			return hit_way;
		}

		/**
		 * \brief	Update least-recently-used data structures.
		 *
		 * \param addr	The address which has been used.
		 */
		void update_way(addr_t addr) {
			// find the position of the last used way
			int lru_idx;
			for (lru_idx = 0; lru_idx < N_WAYS; lru_idx++)
				if (_lru[addr._set][lru_idx] == addr._way)
					break;

			// fill the vacant position of the last used way,
			// by shifting other ways to the left
			for (auto way = 0; way < (N_WAYS - 1); way++) {
				if (way >= lru_idx) {
					_lru[addr._set][way] =
						_lru[addr._set][way + 1];
				}
			}

			// put the last used way to the rightmost position
			_lru[addr._set][N_WAYS - 1] = addr._way;
		}

		/**
		 * \brief	Return the least recently used way associable
		 * 		with \p addr.
		 *
		 * \param addr	The address to be associated.
		 *
		 * \return	The least recently used way.
		 */
		int get_way(addr_t addr) {
			unsigned int way = _lru[addr._set][0];

			addr.set_way(way);
			update_way(addr);

			return way;
		}

		/**
		 * \brief	Load line from main to cache memory and write
		 * 		back the line to be overwritten, if necessary.
		 *
		 * \param addr	The address belonging to the line to be loaded.
		 * \param line	The buffer to store the loaded line.
		 */
		void load(addr_t addr, line_t &line) {
#pragma HLS inline
			auto do_write_back = false;
			// build write-back address
			addr_t write_back_addr(_tag[addr._addr_line], addr._set,
					0, addr._way);
			// check if write back is necessary
			if (WR_ENABLED && _valid[addr._addr_line] &&
					_dirty[addr._addr_line]) {
				// get the line to be written back
				_raw_cache_core.get_line(_cache_mem,
						write_back_addr._addr_cache,
						line);
				do_write_back = true;
			}

			// send read request to memory interface and
			// write request if write-back is necessary
			_if_request.write({true, do_write_back, addr._addr_main,
					write_back_addr._addr_main, line});

			// force FIFO write and FIFO read to separate pipeline
			// stages to avoid deadlock due to the blocking read
			ap_wait();

			// read response from memory interface
			_load_data.read(line);

			_tag[addr._addr_line] = addr._tag;
			_valid[addr._addr_line] = true;
			_dirty[addr._addr_line] = false;
		}

		/**
		 * \brief	Request to write back a cache line to main memory.
		 *
		 * \param addr	The address belonging to the cache line to be
		 * 		written back.
		 */
		void write_back(addr_t addr) {
#pragma HLS inline
			line_t line;

			// read line
			_raw_cache_core.get_line(_cache_mem,
					addr._addr_cache, line);

			// send write request to memory interface
			_if_request.write({false, true, 0, addr._addr_main, line});

			_dirty[addr._addr_line] = false;
		}

		/**
		 * \brief	Write back all valid dirty cache lines to main memory.
		 */
		void flush() {
#pragma HLS inline
			for (auto set = 0; set < N_SETS; set++) {
				for (auto way = 0; way < N_WAYS; way++) {
					addr_t addr(_tag[set * N_WAYS + way], set,
							0, way);
					// check if line has to be written back
					if (_valid[addr._addr_line] &&
							_dirty[addr._addr_line]) {
						// write line back
						write_back(addr);
					}
				}
			}
		}

#ifdef __PROFILE__
		void update_profiling(hit_status_t status) {
			_n_reqs++;

			if (status == HIT)
				_n_hits++;
			else if (status == L1_HIT)
				_n_l1_hits++;
		}
#endif /* __PROFILE__ */

#ifndef __SYNTHESIS__
		class inner {
			private:
				cache *_cache;
				unsigned int _addr_main;
			public:
				inner(cache *c, unsigned int addr_main):
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

