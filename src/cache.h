#ifndef CACHE_H
#define CACHE_H

/**
 * \file	cache.h
 *
 * \brief 	Cache module compatible with Vitis HLS 2020.2.
 *
 *		Cache module whose characteristics are:
 *			- address mapping: set-associative;
 *			- replacement policy: least-recently-used or
 *						last-in first-out;
 *			- write policy: write-back.
 *
 *		Synthesizability is guaranteed with Vitis HLS 2020.2, with II=1
 *		for CORE_LOOP.
 */

#include "address.h"
#include "arbiter.h"
#include "replacer.h"
#include "l1_cache.h"
#include "raw_cache.h"
#include "hls_stream.h"
#include "ap_utils.h"
#include "ap_int.h"
#include "utils.h"
#ifdef __SYNTHESIS__
#include "hls_vector.h"
#else
#include <thread>
#include <array>
#include <cassert>
#endif /* __SYNTHESIS__ */

template <typename T, size_t RD_PORTS, bool WR_ENABLED, size_t MAIN_SIZE,
	 size_t N_SETS, size_t N_WAYS, size_t N_ENTRIES_PER_LINE,
	 bool LRU, size_t L1_CACHE_LINES>
class cache {
	private:
		static const bool RD_ENABLED = (RD_PORTS > 0);
		static const bool L1_CACHE = (L1_CACHE_LINES > 0);
		static const size_t ADDR_SIZE = utils::log2_ceil(MAIN_SIZE);
		static const size_t SET_SIZE = utils::log2_ceil(N_SETS);
		static const size_t OFF_SIZE = utils::log2_ceil(N_ENTRIES_PER_LINE);
		static const size_t TAG_SIZE = (ADDR_SIZE - (SET_SIZE + OFF_SIZE));
		static const size_t WAY_SIZE = utils::log2_ceil(N_WAYS);

		static_assert((RD_ENABLED || WR_ENABLED),
				"Cache must be read-enabled or write-enabled");
		static_assert(((MAIN_SIZE > 0) && ((1 << ADDR_SIZE) == MAIN_SIZE)),
				"MAIN_SIZE must be a power of 2 greater than 0");
		static_assert(((N_SETS > 0) && ((1 << SET_SIZE) == N_SETS)),
				"N_SETS must be a power of 2 greater than 0");
		static_assert(((N_ENTRIES_PER_LINE > 0) &&
					((1 << OFF_SIZE) == N_ENTRIES_PER_LINE)),
				"N_ENTRIES_PER_LINE must be a power of 2 greater than 0");
		static_assert(((N_WAYS > 0) && ((1 << WAY_SIZE) == N_WAYS)),
				"N_WAYS must be a power of 2 greater than 0");
		static_assert((MAIN_SIZE >= (N_SETS * N_WAYS * N_ENTRIES_PER_LINE)),
				"N_SETS and/or N_WAYS and/or N_ENTRIES_PER_LINE are too big for the specified MAIN_SIZE");

#ifdef __SYNTHESIS__
		template <typename TYPE, size_t SIZE>
			using array_type = hls::vector<TYPE, SIZE>;
#else
		template <typename TYPE, size_t SIZE>
			using array_type = std::array<TYPE, SIZE>;
#endif /* __SYNTHESIS__ */

		typedef address<ADDR_SIZE, TAG_SIZE, SET_SIZE, WAY_SIZE> address_type;
		typedef array_type<T, N_ENTRIES_PER_LINE> line_type;
		typedef l1_cache<T, ADDR_SIZE, L1_CACHE_LINES, N_ENTRIES_PER_LINE>
			l1_cache_type;
		typedef raw_cache<T, ADDR_SIZE, (TAG_SIZE + SET_SIZE),
			N_ENTRIES_PER_LINE> raw_cache_type;
		typedef arbiter<T, RD_PORTS, N_ENTRIES_PER_LINE, ADDR_SIZE> arbiter_type;
		typedef replacer<LRU, address_type, N_SETS, N_WAYS,
			N_ENTRIES_PER_LINE> replacer_type;

		typedef enum {
			READ_OP,
			WRITE_OP,
			READ_WRITE_OP,
			STOP_OP
		} op_type;

#if (defined(PROFILE) && (!defined(__SYNTHESIS__)))
		typedef enum {
			MISS,
			HIT,
			L1_HIT
		} hit_status_type;
#endif /* (defined(PROFILE) && (!defined(__SYNTHESIS__))) */

		typedef struct {
			op_type op;
			ap_uint<ADDR_SIZE> load_addr;
			ap_uint<ADDR_SIZE> write_back_addr;
			line_type line;
		} mem_req_type;

		ap_uint<(TAG_SIZE > 0) ? TAG_SIZE : 1> m_tag[N_SETS * N_WAYS];
		bool m_valid[N_SETS * N_WAYS];
		bool m_dirty[N_SETS * N_WAYS];
		T m_cache_mem[N_SETS * N_WAYS * N_ENTRIES_PER_LINE];
		hls::stream<op_type, 4> m_core_req_op;
		hls::stream<ap_uint<ADDR_SIZE>, 4> m_core_req_addr;
		hls::stream<T, 4> m_core_req_data;
		hls::stream<line_type, 4> m_core_resp;
		hls::stream<mem_req_type, 2> m_mem_req;
		hls::stream<line_type, 2> m_mem_resp;
		l1_cache_type m_l1_cache_get;
		raw_cache_type m_raw_cache_core;
		replacer_type m_replacer;
#if (defined(PROFILE) && (!defined(__SYNTHESIS__)))
		hls::stream<hit_status_type> m_hit_status;
		int m_n_reqs = 0;
		int m_n_hits = 0;
		int m_n_l1_hits = 0;
#endif /* (defined(PROFILE) && (!defined(__SYNTHESIS__))) */

	public:
		cache() {
#pragma HLS array_partition variable=m_tag complete dim=1
#pragma HLS array_partition variable=m_valid complete dim=1
#pragma HLS array_partition variable=m_dirty complete dim=1
#pragma HLS array_partition variable=m_cache_mem cyclic factor=N_ENTRIES_PER_LINE dim=1
		}

		/**
		 * \brief	Initialize the cache.
		 *
		 * \note	Must be called before calling \ref run,
		 * 		if \ref L1_CACHE is \c true.
		 */
		void init() {
			if (L1_CACHE)
				m_l1_cache_get.init();
		}

		/**
		 * \brief	Start cache internal processes.
		 *
		 * \param main_mem	The pointer to the main memory.
		 * \param id		The identifier for the \ref arbiter.
		 * \param arbiter	The pointer to the AXI arbiter.
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
		void run(T * const main_mem, const unsigned int id = 0,
				arbiter_type * const arbiter = nullptr) {
#pragma HLS inline
#ifdef __SYNTHESIS__
			run_core();
			run_mem_if(main_mem, arbiter, arbiter != nullptr, id);
#else
			std::thread core_thd([&]{run_core();});
			std::thread mem_if_thd([&]{
					run_mem_if(main_mem, arbiter,
							(arbiter != nullptr), id);
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
			m_core_req_op.write(STOP_OP);
		}

		/**
		 * \brief		Request to read a whole cache line.
		 *
		 * \param addr_main	The address in main memory belonging to
		 * 			the cache line to be read.
		 * \param line		The buffer to store the read line.
		 */
		void get_line(const ap_uint<ADDR_SIZE> addr_main, line_type &line) {
#pragma HLS inline
#ifndef __SYNTHESIS__
			assert(addr_main < MAIN_SIZE);
#endif /* __SYNTHESIS__ */

			// try to get line from L1 cache
			const auto l1_hit = (L1_CACHE &&
					m_l1_cache_get.hit(addr_main));

			if (l1_hit) {
				m_l1_cache_get.get_line(addr_main, line);
			} else {
				// send read request to cache
				m_core_req_op.write(READ_OP);
				m_core_req_addr.write(addr_main);
				// force FIFO write and FIFO read to separate
				// pipeline stages to avoid deadlock due to
				// the blocking read
				// 3 is the read latency in case of HIT:
				// it is put here to inform the scheduler
				// about the latency
				ap_wait_n(3);
				// read response from cache
				m_core_resp.read(line);
				if (L1_CACHE) {
					// store line to L1 cache
					m_l1_cache_get.set_line(addr_main, line);
				}
			}

#if (defined(PROFILE) && (!defined(__SYNTHESIS__)))
			update_profiling(l1_hit ? L1_HIT : m_hit_status.read());
#endif /* (defined(PROFILE) && (!defined(__SYNTHESIS__))) */
		}

		/**
		 * \brief		Request to read a data element.
		 *
		 * \param addr_main	The address in main memory referring to
		 * 			the data element to be read.
		 *
		 * \return		The read data element.
		 */
		T get(const ap_uint<ADDR_SIZE> addr_main) {
#pragma HLS inline
			line_type line;

			// get the whole cache line
			get_line(addr_main, line);

			// extract information from address
			address_type addr(addr_main);

			return line[addr.m_off];
		}

		/**
		 * \brief		Request to write a data element.
		 *
		 * \param addr_main	The address in main memory referring to
		 * 			the data element to be written.
		 * \param data		The data to be written.
		 */
		void set(const ap_uint<ADDR_SIZE> addr_main, const T data) {
#pragma HLS inline
#ifndef __SYNTHESIS__
			assert(addr_main < MAIN_SIZE);
#endif /* __SYNTHESIS__ */

			if (L1_CACHE) {
				// inform L1 cache about the writing
				m_l1_cache_get.notify_write(addr_main);
			}

			// send write request to cache
			m_core_req_op.write(WRITE_OP);
			m_core_req_addr.write(addr_main);
			m_core_req_data.write(data);
#if (defined(PROFILE) && (!defined(__SYNTHESIS__)))
			update_profiling(m_hit_status.read());
#endif /* (defined(PROFILE) && (!defined(__SYNTHESIS__))) */
		}

#if (defined(PROFILE) && (!defined(__SYNTHESIS__)))
		int get_n_reqs() const {
			return m_n_reqs;
		}

		int get_n_hits() const {
			return m_n_hits;
		}

		int get_n_l1_hits() const {
			return m_n_l1_hits;
		}

		double get_hit_ratio() const {
			if (m_n_reqs > 0)
				return ((m_n_hits + m_n_l1_hits) / static_cast<double>(m_n_reqs));

			return 0;
		}
#endif /* (defined(PROFILE) && (!defined(__SYNTHESIS__))) */

	private:
		/**
		 * \brief	Infinite loop managing the cache access requests
		 * 		(sent from the outside).
		 *
		 * \note	The infinite loop must be stopped by calling
		 * 		\ref stop (from the outside) when all the
		 * 		accesses have been completed.
		 */
		void run_core() {
#pragma HLS inline off
			// invalidate all cache lines
			for (auto line = 0; line < (N_SETS * N_WAYS); line++)
				m_valid[line] = false;

			m_replacer.init();

			m_raw_cache_core.init();

CORE_LOOP:		while (1) {
#pragma HLS pipeline
#pragma HLS dependence variable=m_cache_mem distance=1 inter RAW false
				op_type op;
#ifdef __SYNTHESIS__
				// get request and
				// make pipeline flushable (to avoid deadlock)
				if (!m_core_req_op.read_nb(op))
					continue;
#else
				// get request
				m_core_req_op.read(op);
#endif /* __SYNTHESIS__ */

				// exit the loop if request is "end-of-request"
				if (op == STOP_OP)
					break;

				// check the request type
				const auto read = ((RD_ENABLED && (op == READ_OP)) ||
						(!WR_ENABLED));

				// in case of write request, read data to be written
				const auto addr_main = m_core_req_addr.read();
				const auto data = read ? 0 : m_core_req_data.read();

				// extract information from address
				address_type addr(addr_main);

				auto way = hit(addr);
				const auto is_hit = (way != -1);

				if (!is_hit)
					way = m_replacer.get_way(addr);

				addr.set_way(way);
				m_replacer.notify_use(addr);

				line_type line;
				if (is_hit) {
					// read from cache memory
					m_raw_cache_core.get_line(m_cache_mem,
							addr.m_addr_cache,
							line);
				} else {
					// read from main memory
					load(addr, line);

					if (read) {
						// store loaded line to cache
						m_raw_cache_core.set_line(
								m_cache_mem,
								addr.m_addr_cache,
								line);
					}
				}

				if (read) {
					// send the response to the read request
					m_core_resp.write(line);
				} else {
					// modify the line
					line[addr.m_off] = data;

					// store the modified line to cache
					m_raw_cache_core.set_line(m_cache_mem,
							addr.m_addr_cache, line);
					m_dirty[addr.m_addr_line] = true;
				}

#if (defined(PROFILE) && (!defined(__SYNTHESIS__)))
				m_hit_status.write(is_hit ? HIT : MISS);
#endif /* (defined(PROFILE) && (!defined(__SYNTHESIS__))) */
			}

			// synchronize main memory with cache memory
			if (WR_ENABLED)
				flush();

			// make sure that flush has completed before stopping
			// memory interface
			ap_wait();
			// stop memory interface
			line_type dummy;
			m_mem_req.write({STOP_OP, 0, 0, dummy});
		}

		/**
		 * \brief		Infinite loop managing main memory
		 * 			access requests (sent from \ref run_core).
		 *
		 * \param main_mem	The pointer to the main memory.
		 * \param arbiter	The pointer to the AXI arbiter.
		 * \param arbitrate	The boolean value set to \c true if the
		 * 			access to main memory must pass through
		 * 			\ref arbiter.
		 *
		 * \note		\p main_mem must be associated with
		 * 			a dedicated AXI port.
		 *
		 * \note		The infinite loop is stopped by
		 * 			\ref run_core when it is in turn stopped
		 * 			from the outside.
		 */
		void run_mem_if(T * const main_mem, arbiter_type * const arbiter,
				const bool arbitrate, const unsigned int id) {
#pragma HLS function_instantiate variable=id
#pragma HLS function_instantiate variable=arbitrate
			raw_cache_type raw_cache_mem_if;

			if (!arbitrate)
				raw_cache_mem_if.init();
			
MEM_IF_LOOP:		while (1) {
#pragma HLS pipeline
#pragma HLS dependence variable=main_mem distance=1 inter RAW false
				mem_req_type req;
#ifdef __SYNTHESIS__
				// get request and
				// make pipeline flushable (to avoid deadlock)
				if (!m_mem_req.read_nb(req))
					continue;
#else
				// get request
				m_mem_req.read(req);
#endif /* __SYNTHESIS__ */

				// exit the loop if request is "end-of-request"
				if (req.op == STOP_OP)
					break;

				line_type line;
				if ((req.op == READ_OP) || (req.op == READ_WRITE_OP)) {
					// read line from main memory
					if (arbitrate) {
						arbiter->get_line(req.load_addr,
								id, line);
					} else {
						raw_cache_mem_if.get_line(
								main_mem,
								req.load_addr,
								line);
					}
					// send the response to the read request
					m_mem_resp.write(line);
				}

				if (WR_ENABLED && ((req.op == WRITE_OP) ||
							(req.op == READ_WRITE_OP))) {
					// write line to main memory
					raw_cache_mem_if.set_line(main_mem,
							req.write_back_addr, req.line);
				}
			}

			if ((arbiter != nullptr) && (id == 0))
				arbiter->stop();
		}

		/**
		 * \brief	Check if \p addr causes an HIT or a MISS.
		 *
		 * \param addr	The address to be checked.
		 *
		 * \return	hitting way on HIT.
		 * \return	-1 on MISS.
		 */
		inline int hit(const address_type &addr) const {
#pragma HLS inline
			auto addr_tmp = addr;
			auto hit_way = -1;
			for (auto way = 0; way < N_WAYS; way++) {
				addr_tmp.set_way(way);
				if (m_valid[addr_tmp.m_addr_line] &&
						(addr_tmp.m_tag == m_tag[addr_tmp.m_addr_line])) {
					hit_way = way;
					break;
				}
			}

			return hit_way;
		}

		/**
		 * \brief	Load line from main to cache memory and write
		 * 		back the line to be overwritten, if necessary.
		 *
		 * \param addr	The address belonging to the line to be loaded.
		 * \param line	The buffer to store the loaded line.
		 */
		void load(const address_type &addr, line_type &line) {
#pragma HLS inline
			auto op = READ_OP;
			// build write-back address
			address_type write_back_addr(m_tag[addr.m_addr_line], addr.m_set,
					0, addr.m_way);
			// check if write back is necessary
			if (WR_ENABLED && m_valid[addr.m_addr_line] &&
					m_dirty[addr.m_addr_line]) {
				// get the line to be written back
				m_raw_cache_core.get_line(m_cache_mem,
						write_back_addr.m_addr_cache,
						line);
				op = READ_WRITE_OP;
			}

			// send read request to memory interface and
			// write request if write-back is necessary
			m_mem_req.write({op, addr.m_addr_main,
					write_back_addr.m_addr_main, line});

			// force FIFO write and FIFO read to separate pipeline
			// stages to avoid deadlock due to the blocking read
			ap_wait();

			// read response from memory interface
			m_mem_resp.read(line);

			m_tag[addr.m_addr_line] = addr.m_tag;
			m_valid[addr.m_addr_line] = true;
			m_dirty[addr.m_addr_line] = false;

			m_replacer.notify_insertion(addr);
		}

		/**
		 * \brief	Request to write back a cache line to main memory.
		 *
		 * \param addr	The address belonging to the cache line to be
		 * 		written back.
		 */
		void write_back(const address_type &addr) {
#pragma HLS inline
			line_type line;

			// read line
			m_raw_cache_core.get_line(m_cache_mem,
					addr.m_addr_cache, line);

			// send write request to memory interface
			m_mem_req.write({WRITE_OP, 0, addr.m_addr_main, line});

			m_dirty[addr.m_addr_line] = false;
		}

		/**
		 * \brief	Write back all valid dirty cache lines to main memory.
		 */
		void flush() {
#pragma HLS inline
			for (auto set = 0; set < N_SETS; set++) {
				for (auto way = 0; way < N_WAYS; way++) {
					const address_type addr(
							m_tag[set * N_WAYS + way],
							set, 0, way);
					// check if line has to be written back
					if (m_valid[addr.m_addr_line] &&
							m_dirty[addr.m_addr_line]) {
						// write line back
						write_back(addr);
					}
				}
			}
		}

#if (defined(PROFILE) && (!defined(__SYNTHESIS__)))
		void update_profiling(const hit_status_type status) {
			m_n_reqs++;

			if (status == HIT)
				m_n_hits++;
			else if (status == L1_HIT)
				m_n_l1_hits++;
		}
#endif /* (defined(PROFILE) && (!defined(__SYNTHESIS__))) */

		class square_bracket_proxy {
			private:
				void *m_cache;
				const ap_uint<ADDR_SIZE> m_addr_main;
			public:
				square_bracket_proxy(cache *c,
						const ap_uint<ADDR_SIZE> addr_main):
					m_cache(c), m_addr_main(addr_main) {}

				operator T() const {
#pragma HLS inline
					return get();
				}

				square_bracket_proxy &operator=(const T data) {
#pragma HLS inline
					set(data);
					return *this;
				}

				square_bracket_proxy &operator=(
						const square_bracket_proxy &proxy) {
#pragma HLS inline
					set(proxy.get());
					return *this;
				}

			private:
				T get() const {
#pragma HLS inline
					return (static_cast<cache *>(m_cache))->
						get(m_addr_main);
				}

				void set(const T data) {
#pragma HLS inline
					(static_cast<cache *>(m_cache))->
						set(m_addr_main, data);
				}
		};

	public:
		square_bracket_proxy operator[](const ap_uint<ADDR_SIZE> addr_main) {
#pragma HLS inline
			return square_bracket_proxy(this, addr_main);
		}
};

#endif /* CACHE_H */

