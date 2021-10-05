#ifndef CACHE_H
#define CACHE_H

/**
 * \file	cache.h
 *
 * \brief 	Cache module compatible with Vitis HLS 2021.1.
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

#include "address.h"
#include "replacer.h"
#include "l1_cache.h"
#define HLS_STREAM_THREAD_SAFE
#include "hls_stream.h"
#include "stream_cond.h"
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

#define MAX_AXI_BITWIDTH 512

template <typename T, bool RD_ENABLED, bool WR_ENABLED, size_t PORTS,
	 size_t MAIN_SIZE, size_t N_SETS, size_t N_WAYS, size_t N_WORDS_PER_LINE,
	 bool LRU, size_t L1_CACHE_LINES, size_t LATENCY>
class cache {
	private:
		static const bool MEM_IF_PROCESS = (WR_ENABLED || (PORTS > 1) ||
				((sizeof(T) * N_WORDS_PER_LINE * 8) > MAX_AXI_BITWIDTH));
		static const bool L1_CACHE = (L1_CACHE_LINES > 0);
		static const size_t ADDR_SIZE = utils::log2_ceil(MAIN_SIZE);
		static const size_t SET_SIZE = utils::log2_ceil(N_SETS);
		static const size_t OFF_SIZE = utils::log2_ceil(N_WORDS_PER_LINE);
		static const size_t TAG_SIZE = (ADDR_SIZE - (SET_SIZE + OFF_SIZE));
		static const size_t WAY_SIZE = utils::log2_ceil(N_WAYS);

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

#ifdef __SYNTHESIS__
		template <typename TYPE, size_t SIZE>
			using array_type = hls::vector<TYPE, SIZE>;
#else
		template <typename TYPE, size_t SIZE>
			using array_type = std::array<TYPE, SIZE>;
#endif /* __SYNTHESIS__ */

		typedef address<ADDR_SIZE, TAG_SIZE, SET_SIZE, WAY_SIZE> address_type;
		typedef array_type<T, N_WORDS_PER_LINE> line_type;
		typedef l1_cache<T, MAIN_SIZE, L1_CACHE_LINES, N_WORDS_PER_LINE>
			l1_cache_type;
		typedef replacer<LRU, address_type, N_SETS, N_WAYS,
			N_WORDS_PER_LINE> replacer_type;

		typedef enum {
			READ_OP,
			WRITE_OP,
			READ_WRITE_OP,
			STOP_OP,
			NOP_OP
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

		ap_uint<(TAG_SIZE > 0) ? TAG_SIZE : 1> m_tag[N_SETS * N_WAYS];	// 1
		bool m_valid[N_SETS * N_WAYS];					// 2
		bool m_dirty[N_SETS * N_WAYS];					// 3
		T m_cache_mem[N_SETS * N_WAYS * N_WORDS_PER_LINE];		// 4
		hls::stream<op_type, 4> m_core_req_op[PORTS];			// 5
		hls::stream<ap_uint<ADDR_SIZE>, 4> m_core_req_addr[PORTS];	// 6
		hls::stream<T, 4> m_core_req_data[PORTS];			// 7
		hls::stream<line_type, 4> m_core_resp[PORTS];			// 8
		stream_cond<mem_req_type, 2, MEM_IF_PROCESS> m_mem_req;		// 9
		stream_cond<line_type, 2, MEM_IF_PROCESS> m_mem_resp;		// 10
		l1_cache_type m_l1_cache_get[PORTS];				// 11
		replacer_type m_replacer;					// 12
		unsigned int m_core_port;					// 13
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
#pragma HLS array_partition variable=m_cache_mem cyclic factor=N_WORDS_PER_LINE dim=1
#pragma HLS array_partition variable=m_core_req_op complete
#pragma HLS array_partition variable=m_core_req_addr complete
#pragma HLS array_partition variable=m_core_req_data complete
#pragma HLS array_partition variable=m_core_resp complete
#pragma HLS array_partition variable=m_l1_cache_get complete
		}

		/**
		 * \brief	Initialize the cache.
		 *
		 * \note	Must be called before calling \ref run.
		 */
		void init() {
			m_core_port = 0;
			if (L1_CACHE) {
				for (auto port = 0; port < PORTS; port++)
					m_l1_cache_get[port].init();
			}
		}

		/**
		 * \brief		Start cache internal processes.
		 *
		 * \param[in] main_mem	The pointer to the main memory.
		 *
		 * \note		In case of synthesis this must be
		 * 			called in a dataflow region with
		 * 			disable_start_propagation option,
		 * 			together with the function in which
		 * 			cache is accessed.
		 *
		 * \note		In case of C simulation this must be
		 * 			executed by a thread separated from the
		 * 			thread in which cache is accessed.
		 */
		void run(T * const main_mem) {
#pragma HLS inline
#ifdef __SYNTHESIS__
			run_core(main_mem);
			if (MEM_IF_PROCESS)
				run_mem_if(main_mem);
#else
			std::thread core_thd([&]{run_core(main_mem);});
			if (MEM_IF_PROCESS) {
				std::thread mem_if_thd([&]{
						run_mem_if(main_mem);
						});

				mem_if_thd.join();
			}

			core_thd.join();
#endif /* __SYNTHESIS__ */
		}

		/**
		 * \brief	Stop cache internal processes.
		 *
		 * \note	Must be called after the function in which cache
		 * 		is accessed has completed.
		 */
		void stop() {
			for (auto port = 0; port < PORTS; port++)
				m_core_req_op[port].write(STOP_OP);
		}

		/**
		 * \brief		Request to read a whole cache line.
		 *
		 * \param[in] addr_main	The address in main memory belonging to
		 * 			the cache line to be read.
		 * \param[out] line	The buffer to store the read line.
		 */
		void get_line(const ap_uint<ADDR_SIZE> addr_main, line_type &line) {
#pragma HLS inline
#ifndef __SYNTHESIS__
			assert(addr_main < MAIN_SIZE);
#endif /* __SYNTHESIS__ */

			const auto port = m_core_port;
			m_core_port = ((m_core_port + 1) % PORTS);

			// try to get line from L1 cache
			const auto l1_hit = (L1_CACHE &&
					m_l1_cache_get[port].hit(addr_main));

			if (l1_hit) {
				m_l1_cache_get[port].get_line(addr_main, line);
#ifndef __SYNTHESIS__
				m_core_req_op[port].write(NOP_OP);
#endif /* __SYNTHESIS__ */
			} else {
				// send read request to cache
				m_core_req_op[port].write(READ_OP);
				m_core_req_addr[port].write(addr_main);
				// force FIFO write and FIFO read to separate
				// pipeline stages to avoid deadlock due to
				// the blocking read
				ap_wait_n(LATENCY);
				// read response from cache
				m_core_resp[port].read(line);

				if (L1_CACHE) {
					// store line to L1 cache
					m_l1_cache_get[port].set_line(addr_main, line);
				}
			}

#if (defined(PROFILE) && (!defined(__SYNTHESIS__)))
			update_profiling(l1_hit ? L1_HIT : m_hit_status.read());
#endif /* (defined(PROFILE) && (!defined(__SYNTHESIS__))) */
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
			m_core_req_op[0].write(WRITE_OP);
			m_core_req_addr[0].write(addr_main);
			m_core_req_data[0].write(data);
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
		 * \brief		Infinite loop managing the cache access
		 * 			requests (sent from the outside).
		 *
		 * \param[in] main_mem	The pointer to the main memory (ignored
		 * 			if \ref MEM_IF_PROCESS is \c true).
		 *
		 * \note		The infinite loop must be stopped by
		 * 			calling \ref stop (from the outside)
		 * 			when all the accesses have been completed.
		 */
		void run_core(T * const main_mem) {
#pragma HLS inline off
			// invalidate all cache lines
			for (auto line = 0; line < (N_SETS * N_WAYS); line++)
				m_valid[line] = false;

			m_replacer.init();

CORE_LOOP:		while (1) {
#pragma HLS pipeline II=PORTS
INNER_CORE_LOOP:		for (auto port = 0; port < PORTS; port++) {
					op_type op;
#ifdef __SYNTHESIS__
					// get request and
					// make pipeline flushable (to avoid deadlock)
					if (m_core_req_op[port].read_nb(op)) {
#else
					// get request
					m_core_req_op[port].read(op);
#endif /* __SYNTHESIS__ */

					// exit the loop if request is "end-of-request"
					if (op == STOP_OP)
						goto core_end;

#ifndef __SYNTHESIS__
					if (op == NOP_OP)
						continue;
#endif /* __SYNTHESIS__ */

					// check the request type
					const auto read = ((RD_ENABLED && (op == READ_OP)) ||
							(!WR_ENABLED));

					// in case of write request, read data to be written
					const auto addr_main = m_core_req_addr[port].read();
					T data;
					if (!read)
						data = m_core_req_data[port].read();

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
						get_line(m_cache_mem,
								addr.m_addr_cache,
								line);
					} else {
						// read from main memory
						auto op = READ_OP;
						// build write-back address
						address_type write_back_addr(m_tag[addr.m_addr_line], addr.m_set,
								0, addr.m_way);
						// check if write back is necessary
						if (WR_ENABLED && m_valid[addr.m_addr_line] &&
								m_dirty[addr.m_addr_line]) {
							// get the line to be written back
							get_line(m_cache_mem,
									write_back_addr.m_addr_cache,
									line);

							op = READ_WRITE_OP;
						}

						const mem_req_type req = {op, addr.m_addr_main,
									write_back_addr.m_addr_main, line};

						if (MEM_IF_PROCESS) {
							// send read request to
							// memory interface and
							// write request if
							// write-back is necessary
							m_mem_req.write(req);

							// force FIFO write and
							// FIFO read to separate
							// pipeline stages to
							// avoid deadlock due to
							// the blocking read
							ap_wait();

							// read response from
							// memory interface
							m_mem_resp.read(line);
						} else {
							execute_mem_if_req(main_mem,
									req, line);
						}

						m_tag[addr.m_addr_line] = addr.m_tag;
						m_valid[addr.m_addr_line] = true;
						m_dirty[addr.m_addr_line] = false;

						m_replacer.notify_insertion(addr);

						if (read) {
							// store loaded line to cache
							set_line(m_cache_mem,
									addr.m_addr_cache,
									line);
						}
					}

					if (read) {
						// send the response to the read request
						m_core_resp[port].write(line);
					} else {
						// modify the line
						line[addr.m_off] = data;

						// store the modified line to cache
						set_line(m_cache_mem,
								addr.m_addr_cache,
								line);


						m_dirty[addr.m_addr_line] = true;
					}

#if (defined(PROFILE) && (!defined(__SYNTHESIS__)))
					m_hit_status.write(is_hit ? HIT : MISS);
#endif /* (defined(PROFILE) && (!defined(__SYNTHESIS__))) */
#ifdef __SYNTHESIS__
					}
#endif /* __SYNTHESIS__ */
				}
			}

core_end:
			// synchronize main memory with cache memory
			if (WR_ENABLED)
				flush();

			// make sure that flush has completed before stopping
			// memory interface
			ap_wait();

			if (MEM_IF_PROCESS) {
				// stop memory interface
				line_type dummy;
				m_mem_req.write({STOP_OP, 0, 0, dummy});
			}
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
#pragma HLS pipeline off
				mem_req_type req;
				// get request
				m_mem_req.read(req);

				// exit the loop if request is "end-of-request"
				if (req.op == STOP_OP)
					break;

				line_type line;
				execute_mem_if_req(main_mem, req, line);

				if ((req.op == READ_OP) || (req.op == READ_WRITE_OP)) {
					// send the response to the read request
					m_mem_resp.write(line);
				}
			}

		}

		/**
		 * \brief		Execute memory access(es) specified in
		 * 			\p req.
		 *
		 * \param[in] main_mem	The pointer to the main memory.
		 * \param[in] req	The request to be executed.
		 * \param[out] line	The buffer to store the read line.
		 *
		 * \note		\p main_mem must be associated with
		 * 			a dedicated AXI port.
		 */
		void execute_mem_if_req(T * const main_mem,
				const mem_req_type &req, line_type &line) {
#pragma HLS inline
			if ((req.op == READ_OP) || (req.op == READ_WRITE_OP)) {
				// read line from main memory
				get_line(main_mem, req.load_addr, line);
			}

			if (WR_ENABLED && ((req.op == WRITE_OP) ||
						(req.op == READ_WRITE_OP))) {
				// write line to main memory
				set_line(main_mem, req.write_back_addr, req.line);
			}
		}

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
			for (auto way = 0; way < N_WAYS; way++) {
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
			for (auto set = 0; set < N_SETS; set++) {
				for (auto way = 0; way < N_WAYS; way++) {
					const address_type addr(
							m_tag[set * N_WAYS + way],
							set, 0, way);
					// check if line has to be written back
					if (m_valid[addr.m_addr_line] &&
							m_dirty[addr.m_addr_line]) {
						// write line back
						line_type line;

						// read line
						get_line(m_cache_mem,
								addr.m_addr_cache,
								line);

						// send write request to memory
						// interface
						m_mem_req.write({WRITE_OP, 0,
								addr.m_addr_main,
								line});

						m_dirty[addr.m_addr_line] = false;
					}
				}
			}
		}

		void get_line(const T * const mem,
				const ap_uint<(ADDR_SIZE > 0) ? ADDR_SIZE : 1> addr,
				line_type &line) {
#pragma HLS inline
			const T * const mem_line = &(mem[addr & (-1U << OFF_SIZE)]);

			for (auto off = 0; off < N_WORDS_PER_LINE; off++) {
#pragma HLS unroll
				line[off] = mem_line[off];
			}
		}

		void set_line(T * const mem,
				const ap_uint<(ADDR_SIZE > 0) ? ADDR_SIZE : 1> addr,
				const line_type &line) {
#pragma HLS inline
			T * const mem_line = &(mem[addr & (-1U << OFF_SIZE)]);

			for (auto off = 0; off < N_WORDS_PER_LINE; off++) {
#pragma HLS unroll
				mem_line[off] = line[off];
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

#endif /* CACHE_H */

