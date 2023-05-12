#ifndef CACHE_H
#define CACHE_H

/**
 * \file	cache.h
 *
 * \brief 	Cache module compatible with Vitis HLS (2020.1-2021.2).
 *
 *		Cache module whose characteristics are:
 *			- address mapping: set-associative;
 *			- replacement policy: least-recently-used or
 *						last-in first-out;
 *			- write policy: write-back.
 *
 *		Advanced features:
 *			- Multi-levels: L1 cache (direct-mapped, write-through).
 *			- Multi-ports (read-only).
 */

#include <cstddef>
#define AP_INT_MAX_W 8192
#include "types.h"
#include "address.h"
#include "replacer.h"
#include "l1_cache.h"
#include "raw_cache.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#include <ap_utils.h>
#pragma GCC diagnostic pop
#include <ap_int.h>
#include "utils.h"
#ifdef __SYNTHESIS__
#define HLS_STREAM_THREAD_SAFE
#include <hls_stream.h>
#include "sliced_stream.h"
#else
#include <cassert>
#endif /* __SYNTHESIS__ */

#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wpedantic"
#pragma GCC diagnostic error "-Wall"
#pragma GCC diagnostic error "-Wextra"
#pragma GCC diagnostic ignored "-Wunused-label"

using namespace types;

template <typename T, bool RD_ENABLED, bool WR_ENABLED, size_t PORTS,
	 size_t MAIN_SIZE, size_t N_SETS, size_t N_WAYS, size_t N_WORDS_PER_LINE,
	 bool LRU, size_t N_L1_SETS, size_t N_L1_WAYS, bool SWAP_TAG_SET,
	 size_t LATENCY, storage_impl_type L2_STORAGE_IMPL = AUTO,
	 storage_impl_type L1_STORAGE_IMPL = AUTO>
class cache {
	private:
		static const bool L1_CACHE = ((N_L1_SETS * N_L1_WAYS) > 0);
		static const bool RAW_CACHE = (RD_ENABLED && WR_ENABLED);
		static const size_t ADDR_SIZE = utils::log2_ceil(MAIN_SIZE);
		static const size_t SET_SIZE = utils::log2_ceil(N_SETS);
		static const size_t OFF_SIZE = utils::log2_ceil(N_WORDS_PER_LINE);
		static const size_t TAG_SIZE = (ADDR_SIZE - (SET_SIZE + OFF_SIZE));
		static const size_t WAY_SIZE = utils::log2_ceil(N_WAYS);
		static const size_t WORD_SIZE = (sizeof(T) * 8);

		static_assert((RD_ENABLED || WR_ENABLED),
				"RD_ENABLED and/or WR_ENABLED must be true");
		static_assert((PORTS > 0), "PORTS must be greater than 0");
		static_assert((!(WR_ENABLED && (PORTS > 1))),
				"PORTS must be equal to 1 when WR_ENABLED is true");
		static_assert(((MAIN_SIZE > 0) && ((1 << ADDR_SIZE) == MAIN_SIZE)),
				"MAIN_SIZE must be a power of 2 greater than 0");
		static_assert(((N_SETS > 0) && ((1 << SET_SIZE) == N_SETS)),
				"N_SETS must be a power of 2 greater than 0");
		static_assert(((N_WAYS > 0) && ((1 << WAY_SIZE) == N_WAYS)),
				"N_WAYS must be a power of 2 greater than 0");
		static_assert(((N_WORDS_PER_LINE > 0) &&
					((1 << OFF_SIZE) == N_WORDS_PER_LINE)),
				"N_WORDS_PER_LINE must be a power of 2 greater than 0");
		static_assert((MAIN_SIZE >= (N_SETS * N_WAYS * N_WORDS_PER_LINE)),
				"N_SETS and/or N_WAYS and/or N_WORDS_PER_LINE are too big for the specified MAIN_SIZE");

		typedef address<ADDR_SIZE, TAG_SIZE, SET_SIZE, WAY_SIZE, SWAP_TAG_SET>
			address_type;
		typedef T line_type[N_WORDS_PER_LINE];
		typedef l1_cache<T, MAIN_SIZE, N_L1_SETS, N_L1_WAYS,
			N_WORDS_PER_LINE, SWAP_TAG_SET, L1_STORAGE_IMPL> l1_cache_type;
		typedef raw_cache<T, (N_SETS * N_WAYS), N_WORDS_PER_LINE, 2>
			raw_cache_type;
		typedef replacer<LRU, address_type, N_SETS, N_WAYS,
			N_WORDS_PER_LINE> replacer_type;

		typedef enum {
			READ_OP,
			WRITE_OP,
			READ_WRITE_OP,
			STOP_OP
		} op_type;

#ifndef __SYNTHESIS__
		typedef enum {
			MISS,
			HIT,
			L1_HIT
		} hit_status_type;
#endif /* __SYNTHESIS__ */

		typedef struct {
			op_type op;
			ap_uint<ADDR_SIZE> addr;
			T data;
		} core_req_type;

		typedef struct {
			op_type op;
			ap_uint<ADDR_SIZE> load_addr;
		} mem_req_type;

		typedef struct {
			ap_uint<ADDR_SIZE> write_back_addr;
			line_type line;
		} mem_st_req_type;

		ap_uint<(TAG_SIZE > 0) ? TAG_SIZE : 1> m_tag[N_SETS * N_WAYS];	// 0
		ap_uint<N_SETS * N_WAYS> m_valid;				// 1
		ap_uint<N_SETS * N_WAYS> m_dirty;				// 2
		T m_cache_mem[N_SETS * N_WAYS][N_WORDS_PER_LINE];		// 3
		raw_cache_type m_raw_cache_core;				// 4
		l1_cache_type m_l1_cache_get[PORTS];				// 5
		replacer_type m_replacer;					// 6
		unsigned int m_core_port;					// 7
#ifdef __SYNTHESIS__
		hls::stream<core_req_type, (LATENCY * PORTS)> m_core_req[PORTS];// 8
		sliced_stream<T, N_WORDS_PER_LINE, (LATENCY * PORTS)>
			m_core_resp[PORTS];					// 9
		hls::stream<mem_req_type, 2> m_mem_req;				// 10
		hls::stream<mem_st_req_type, 2> m_mem_st_req;			// 11
		sliced_stream<T, N_WORDS_PER_LINE, 2> m_mem_resp;		// 12
#else
		T * const m_main_mem;
		int m_n_reqs[PORTS] = {0};
		int m_n_hits[PORTS] = {0};
		int m_n_l1_reqs[PORTS] = {0};
		int m_n_l1_hits[PORTS] = {0};
#endif /* __SYNTHESIS__ */

	public:
#ifdef __SYNTHESIS__
		cache(T * const main_mem) {
#pragma HLS array_reshape variable=m_cache_mem type=complete dim=2
#pragma HLS array_partition variable=m_tag type=complete dim=0
			if (PORTS > 1) {
#pragma HLS array_partition variable=m_core_req type=complete dim=0
#pragma HLS array_partition variable=m_core_resp type=complete dim=0
#pragma HLS array_partition variable=m_l1_cache_get type=complete dim=0
			}

			switch (L2_STORAGE_IMPL) {
				case URAM:
#pragma HLS bind_storage variable=m_cache_mem type=RAM_2P impl=URAM
					break;
				case BRAM:
#pragma HLS bind_storage variable=m_cache_mem type=RAM_2P impl=BRAM
					break;
				case LUTRAM:
#pragma HLS bind_storage variable=m_cache_mem type=RAM_2P impl=LUTRAM
					break;
				default:
					break;
			}

			run(main_mem);
		}
#else
		cache(T * const main_mem): m_main_mem(main_mem) {}
#endif /* __SYNTHESIS__ */

		/**
		 * \brief	Initialize the cache.
		 *
		 * \note	Must be called before calling \ref run.
		 */
		void init() {
#ifndef __SYNTHESIS__
			// invalidate all cache lines
			m_valid = 0;

			m_replacer.init();
			if (RAW_CACHE)
				m_raw_cache_core.init();
#endif /* __SYNTHESIS__ */

			m_core_port = 0;

			if (L1_CACHE) {
				for (size_t port = 0; port < PORTS; port++)
					m_l1_cache_get[port].init();
			}
		}

#ifdef __SYNTHESIS__
		/**
		 * \brief		Start cache internal processes.
		 *
		 * \param[in] main_mem	The pointer to the main memory.
		 *
		 * \note		This function must be before
		 * 			the function in which cache is
		 * 			accessed.
		 */
		void run(T * const main_mem) {
#pragma HLS inline
			run_core();
			run_mem_if(main_mem);
		}
#endif /* __SYNTHESIS__ */

		/**
		 * \brief	Stop cache internal processes.
		 *
		 * \note	Must be called after the function in which cache
		 * 		is accessed has completed.
		 */
		void stop() {
#ifdef __SYNTHESIS__
			core_req_type stop_req;
			stop_req.op = STOP_OP;
			m_core_req[m_core_port].write(stop_req);
#else
			flush();
#endif /* __SYNTHESIS__ */
		}

#ifdef __SYNTHESIS__
		bool write_req(const core_req_type req, const unsigned int port) {
#pragma HLS function_instantiate variable=port
			return m_core_req[port].write_dep(req, false);
		}

		void read_resp(line_type line, bool dep, const unsigned int port) {
#pragma HLS function_instantiate variable=port
			m_core_resp[port].read_dep(line, dep);
		}
#endif /* __SYNTHESIS__ */

		/**
		 * \brief		Request to read a whole cache line.
		 *
		 * \param[in] addr_main	The address in main memory belonging to
		 * 			the cache line to be read.
		 * \param[in] port	The port from which to read.
		 * \param[out] line	The buffer to store the read line.
		 */
		void get_line(const ap_uint<ADDR_SIZE> addr_main,
				const unsigned int port, line_type line) {
#pragma HLS inline
#ifndef __SYNTHESIS__
			assert(addr_main < MAIN_SIZE);
#endif /* __SYNTHESIS__ */

			// try to get line from L1 cache
			const auto l1_hit = (L1_CACHE &&
					m_l1_cache_get[port].get_line(addr_main, line));

#ifndef __SYNTHESIS__
			auto hit_status = L1_HIT;
#endif /* __SYNTHESIS__ */
			if (!l1_hit) {
				core_req_type req;
				req.op = READ_OP;
				req.addr = addr_main;

#ifdef __SYNTHESIS__
				// send read request to cache
				auto dep = write_req(req, port);
				// force FIFO write and FIFO read to separate
				// pipeline stages to avoid deadlock due to
				// the blocking read
				dep = utils::delay<LATENCY>(dep);

				// read response from cache
				read_resp(line, dep, port);
#else
				hit_status = exec_core_req(req, line);
#endif /* __SYNTHESIS__ */

				if (L1_CACHE) {
					// store line to L1 cache
					m_l1_cache_get[port].set_line(addr_main, line);
				}
			}

#ifndef __SYNTHESIS__
			update_profiling(hit_status, port);
#endif /* __SYNTHESIS__ */
		}

		/**
		 * \brief		Request to read a data element from a
		 * 			specific port.
		 *
		 * \param[in] addr_main	The address in main memory referring to
		 * 			the data element to be read.
		 * \param[in] port	The port from which to read.
		 *
		 * \return		The read data element.
		 */
		T get(const ap_uint<ADDR_SIZE> addr_main, const unsigned int port) {
#pragma HLS inline
			line_type line;
#pragma HLS array_partition variable=line type=complete dim=0

			// get the whole cache line
			get_line(addr_main, port, line);

			// extract information from address
			address_type addr(addr_main);

			return line[addr.m_off];
		}

		/**
		 * \brief		Request to read a data element.
		 *
		 * \param[in] addr_main	The address in main memory referring to
		 * 			the data element to be read.
		 *
		 * \return		The read data element.
		 */
		T get(const ap_uint<ADDR_SIZE> addr_main) {
#pragma HLS inline
			const auto data = get(addr_main, m_core_port);
			m_core_port = ((m_core_port + 1) % PORTS);

			return data;
		}

		/**
		 * \brief		Request to write a data element.
		 *
		 * \param[in] addr_main	The address in main memory referring to
		 * 			the data element to be written.
		 * \param[in] data	The data to be written.
		 */
		void set(const ap_uint<ADDR_SIZE> addr_main, const T data) {
#pragma HLS inline
#ifndef __SYNTHESIS__
			assert(addr_main < MAIN_SIZE);
#endif /* __SYNTHESIS__ */

			if (L1_CACHE) {
				// inform L1 caches about the writing
				m_l1_cache_get[0].notify_write(addr_main);
			}

			// send write request to cache
			core_req_type req = {WRITE_OP, addr_main, data};

#ifdef __SYNTHESIS__
			m_core_req[0].write(req);
#else
			line_type dummy;
			const auto hit_status = exec_core_req(req, dummy);
			update_profiling(hit_status, 0);
#endif /* __SYNTHESIS__ */
		}

#ifndef __SYNTHESIS__
		int get_n_reqs(const unsigned int port) const {
			return m_n_reqs[port];
		}

		int get_n_hits(const unsigned int port) const {
			return m_n_hits[port];
		}

		int get_n_l1_reqs(const unsigned int port) const {
			return m_n_l1_reqs[port];
		}

		int get_n_l1_hits(const unsigned int port) const {
			return m_n_l1_hits[port];
		}

		double get_hit_ratio(const unsigned int port) const {
			if (m_n_reqs[port] > 0)
				return ((m_n_hits[port] + m_n_l1_hits[port]) /
						static_cast<double>(m_n_reqs[port] +
							m_n_l1_reqs[port]));

			return 0;
		}
#endif /* __SYNTHESIS__ */

	private:
#ifdef __SYNTHESIS__
		void exec_core_req(core_req_type &req, line_type line) {
#else
		hit_status_type exec_core_req(core_req_type &req, line_type line) {
#endif /* __SYNTHESIS__ */
#pragma HLS inline
			// check the request type
			const auto read = ((RD_ENABLED && (req.op == READ_OP)) ||
					(!WR_ENABLED));

			// extract information from address
			address_type addr(req.addr);

			auto way = hit(addr);
			const auto is_hit = (way != -1);

			if (!is_hit)
				way = m_replacer.get_way(addr);

			addr.set_way(way);
			m_replacer.notify_use(addr);

			mem_req_type mem_req;
			mem_st_req_type mem_st_req;
			typename address_type::addr_line_type addr_cache_rd = addr.m_addr_line;
			if (!is_hit) {
				// read from main memory
				mem_req.op = READ_OP;
				mem_req.load_addr = addr.m_addr_main;

				// check if write back is necessary
				if (WR_ENABLED && m_valid[addr.m_addr_line] &&
						m_dirty[addr.m_addr_line]) {
					// build write-back address
					address_type write_back_addr(m_tag[addr.m_addr_line],
							addr.m_set, 0, addr.m_way);
					addr_cache_rd = write_back_addr.m_addr_line;
					mem_req.op = READ_WRITE_OP;
					mem_st_req.write_back_addr = write_back_addr.m_addr_main;
				}
			}

			// mem_req.op is READ_WRITE_OP only in case of write back
			if (is_hit || (mem_req.op == READ_WRITE_OP)) {
				// read from cache memory
				if (RAW_CACHE) {
					m_raw_cache_core.get_line(m_cache_mem,
							addr_cache_rd,
							is_hit ? line : mem_st_req.line);
				} else {
					if (is_hit) {
						for (size_t off = 0; off < N_WORDS_PER_LINE; off++)
							line[off] = m_cache_mem[addr_cache_rd][off];
					} else {
						for (size_t off = 0; off < N_WORDS_PER_LINE; off++)
							mem_st_req.line[off] = m_cache_mem[addr_cache_rd][off];
					}
				}
			}
	
			if (!is_hit) {
#ifdef __SYNTHESIS__
				// send read request to
				// memory interface and
				// write request if
				// write-back is necessary
				m_mem_req.write(mem_req);
				if (WR_ENABLED)
					m_mem_st_req.write(mem_st_req);

				// force FIFO write and
				// FIFO read to separate
				// pipeline stages to
				// avoid deadlock due to
				// the blocking read
				ap_wait();

				// read response from
				// memory interface
				m_mem_resp.read(line);
#else
				exec_mem_req(m_main_mem, mem_req, mem_st_req, line);
#endif /* __SYNTHESIS__ */

				m_tag[addr.m_addr_line] = addr.m_tag;
				m_valid[addr.m_addr_line] = true;
				m_dirty[addr.m_addr_line] = false;

				m_replacer.notify_insertion(addr);

				if (read) {
					// store loaded line to cache
					if (RAW_CACHE) {
						m_raw_cache_core.set_line(
								m_cache_mem,
								addr.m_addr_line,
								line);
					} else {
						for (size_t off = 0; off < N_WORDS_PER_LINE; off++)
							m_cache_mem[addr.m_addr_line][off] = line[off];
					}
				}
			}

			if (!read) {
				if (RAW_CACHE) {
					// modify the line
					line[addr.m_off] = req.data;

					// store the modified line to cache
					m_raw_cache_core.set_line(m_cache_mem,
							addr.m_addr_line, line);
				} else {
					m_cache_mem[addr.m_addr_line][addr.m_off] =
						req.data;
				}


				m_dirty[addr.m_addr_line] = true;
			}

#ifndef __SYNTHESIS__
			return (is_hit ? HIT : MISS);
#endif /* __SYNTHESIS__ */
		}

		void exec_mem_req(T * const main_mem, mem_req_type &req,
				mem_st_req_type &st_req, line_type line) {
#pragma HLS inline
			if ((req.op == READ_OP) || (req.op == READ_WRITE_OP)) {
				// read line from main memory
				get_line(main_mem, req.load_addr, line);
			}

			if (WR_ENABLED && ((req.op == WRITE_OP) ||
						(req.op == READ_WRITE_OP))) {
				// write line to main memory
				set_line(main_mem, st_req.write_back_addr,
						st_req.line);
			}
		}

#ifdef __SYNTHESIS__
		/**
		 * \brief		Infinite loop managing the cache access
		 * 			requests (sent from the outside).
		 *
		 * \note		The infinite loop must be stopped by
		 * 			calling \ref stop (from the outside)
		 * 			when all the accesses have been completed.
		 */
		void run_core() {
#pragma HLS inline off
			// invalidate all cache lines
			m_valid = 0;

			m_replacer.init();
			if (RAW_CACHE)
				m_raw_cache_core.init();

CORE_LOOP:		for (size_t port = 0; ; port = ((port + 1) % PORTS)) {
#pragma HLS pipeline II=1 style=flp
				if (RAW_CACHE) {
#pragma HLS dependence variable=m_cache_mem inter RAW distance=3 true
				}
				core_req_type req;
				// get request and
				// make pipeline flushable (to avoid deadlock)
				if (m_core_req[port].read_nb(req)) {
					// exit the loop if request is "end-of-request"
					if (req.op == STOP_OP)
						break;

					line_type line;
#pragma HLS array_partition variable=line type=complete dim=0
					exec_core_req(req, line);

					if ((RD_ENABLED && (req.op == READ_OP)) ||
							(!WR_ENABLED)) {
						// send the response to the read request
						m_core_resp[port].write(line);
					}
				}
			}

			// synchronize main memory with cache memory
			if (WR_ENABLED)
				flush();

			// stop memory interface
			mem_req_type stop_req;
			stop_req.op = STOP_OP;
			m_mem_req.write(stop_req);
		}

		/**
		 * \brief		Infinite loop managing main memory
		 * 			access requests (sent from \ref run_core).
		 *
		 * \param[in] main_mem	The pointer to the main memory.
		 *
		 * \note		\p main_mem must be associated with
		 * 			a dedicated AXI port.
		 *
		 * \note		The infinite loop is stopped by
		 * 			\ref run_core when it is in turn stopped
		 * 			from the outside.
		 */
		void run_mem_if(T * const main_mem) {
#pragma HLS inline off
MEM_IF_LOOP:		while (1) {
#pragma HLS pipeline II=128
				mem_req_type req;
				mem_st_req_type st_req;
				// get request
				m_mem_req.read(req);

				// exit the loop if request is "end-of-request"
				if (req.op == STOP_OP)
					break;

				if (WR_ENABLED)
					m_mem_st_req.read(st_req);

				line_type line;
				exec_mem_req(main_mem, req, st_req, line);

				if ((req.op == READ_OP) ||
						(req.op == READ_WRITE_OP)) {
					// send the response to the read request
					m_mem_resp.write(line);
				}
			}

		}
#endif /* __SYNTHESIS__ */

		/**
		 * \brief		Check if \p addr causes an HIT or a MISS.
		 *
		 * \param[in] addr	The address to be checked.
		 *
		 * \return		hitting way on HIT.
		 * \return		-1 on MISS.
		 */
		inline int hit(const address_type &addr) const {
#pragma HLS inline
			auto addr_tmp = addr;
			auto hit_way = -1;
			for (size_t way = 0; way < N_WAYS; way++) {
				addr_tmp.set_way(way);
				if (m_valid[addr_tmp.m_addr_line] &&
						(addr_tmp.m_tag == m_tag[addr_tmp.m_addr_line])) {
					hit_way = way;
				}
			}

			return hit_way;
		}

		/**
		 * \brief	Write back all valid dirty cache lines to main memory.
		 */
		void flush() {
#pragma HLS inline
			for (size_t set = 0; set < N_SETS; set++) {
				for (size_t way = 0; way < N_WAYS; way++) {
					const address_type addr(
							m_tag[set * N_WAYS + way],
							set, 0, way);
					// check if line has to be written back
					if (m_valid[addr.m_addr_line] &&
							m_dirty[addr.m_addr_line]) {
						// write line back
						line_type line;

						// read line
						for (size_t off = 0; off < N_WORDS_PER_LINE; off++)
							line[off] = m_cache_mem[addr.m_addr_line][off];

						mem_req_type req = {
							WRITE_OP, 0};
						mem_st_req_type st_req;
						st_req.write_back_addr = addr.m_addr_main;
						for (size_t off = 0; off < N_WORDS_PER_LINE; off++)
							st_req.line[off] = line[off];
#ifdef __SYNTHESIS__
						// send write request to memory
						// interface
						m_mem_req.write(req);
						m_mem_st_req.write(st_req);
#else
						line_type dummy;
						exec_mem_req(m_main_mem,
								req, st_req,
								dummy);
#endif /* __SYNTHESIS__ */

						m_dirty[addr.m_addr_line] = false;
					}
				}
			}
		}

		void get_line(const T * const mem,
				const ap_uint<(ADDR_SIZE > 0) ? ADDR_SIZE : 1> addr,
				line_type line) {
#pragma HLS inline
			const T * const mem_line = &(mem[addr & (-1U << OFF_SIZE)]);

			for (size_t off = 0; off < N_WORDS_PER_LINE; off++) {
#pragma HLS unroll
				line[off] = mem_line[off];
			}
		}

		void set_line(T * const mem,
				const ap_uint<(ADDR_SIZE > 0) ? ADDR_SIZE : 1> addr,
				const line_type line) {
#pragma HLS inline
			T * const mem_line = &(mem[addr & (-1U << OFF_SIZE)]);

			for (size_t off = 0; off < N_WORDS_PER_LINE; off++) {
#pragma HLS unroll
				mem_line[off] = line[off];
			}
		}

#ifndef __SYNTHESIS__
		void update_profiling(const hit_status_type status, const unsigned int port) {
			m_n_l1_reqs[port]++;

			if (status == L1_HIT) {
				m_n_l1_hits[port]++;
			} else {
				m_n_reqs[port]++;
				if (status == HIT)
					m_n_hits[port]++;
			}
		}

#endif /* __SYNTHESIS__ */

		class square_bracket_proxy {
			private:
				cache *m_cache;
				const ap_uint<ADDR_SIZE> m_addr_main;
			public:
				square_bracket_proxy(cache *c,
						const ap_uint<ADDR_SIZE> addr_main):
					m_cache(c), m_addr_main(addr_main) {
#pragma HLS inline
					}

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
					return m_cache->get(m_addr_main);
				}

				void set(const T data) {
#pragma HLS inline
					m_cache->set(m_addr_main, data);
				}
		};

	public:
		square_bracket_proxy operator[](const ap_uint<ADDR_SIZE> addr_main) {
#pragma HLS inline
			return square_bracket_proxy(this, addr_main);
		}
};

template<typename>
struct is_cache : std::false_type {};

template <typename T, bool RD_ENABLED, bool WR_ENABLED, size_t PORTS,
	 size_t MAIN_SIZE, size_t N_SETS, size_t N_WAYS, size_t N_WORDS_PER_LINE,
	 bool LRU, size_t N_L1_SETS, size_t N_L1_WAYS, bool SWAP_TAG_SET,
	 size_t LATENCY, storage_impl_type L2_STORAGE_IMPL,
	 storage_impl_type L1_STORAGE_IMPL>
	 struct is_cache<cache<T, RD_ENABLED, WR_ENABLED, PORTS, MAIN_SIZE,
	 N_SETS, N_WAYS, N_WORDS_PER_LINE, LRU, N_L1_SETS, N_L1_WAYS,
	 SWAP_TAG_SET, LATENCY, L2_STORAGE_IMPL, L1_STORAGE_IMPL>&> :
	 std::true_type {};

void init() {}

template <typename HEAD_TYPE, typename... TAIL_TYPES>
typename std::enable_if<!is_cache<HEAD_TYPE>::value, void>::type
init(HEAD_TYPE &&head, TAIL_TYPES&&... tail) {
	(void) head;
	init(tail...);
}

template <typename HEAD_TYPE, typename... TAIL_TYPES>
typename std::enable_if<is_cache<HEAD_TYPE>::value, void>::type
init(HEAD_TYPE &&head, TAIL_TYPES&&... tail) {
	head.init();
	init(tail...);
}

void stop() {}

template <typename HEAD_TYPE, typename... TAIL_TYPES>
typename std::enable_if<!is_cache<HEAD_TYPE>::value, void>::type
stop(HEAD_TYPE &&head, TAIL_TYPES&&... tail) {
	(void) head;
	stop(tail...);
}

template <typename HEAD_TYPE, typename... TAIL_TYPES>
typename std::enable_if<is_cache<HEAD_TYPE>::value, void>::type
stop(HEAD_TYPE &&head, TAIL_TYPES&&... tail) {
	head.stop();
	stop(tail...);
}

template <class T, typename... ARGS_TYPES>
void cache_wrapper(T &&fn, ARGS_TYPES&&... args) {
#pragma HLS inline off
	init(args...);
	fn(args...);
	stop(args...);
}

#pragma GCC diagnostic pop

#endif /* CACHE_H */

