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
#include <type_traits>
#define AP_INT_MAX_W (1 << 15)
#include "address.h"
#include "replacer.h"
#include "l1_cache.h"
#include "raw_cache.h"
#define HLS_STREAM_THREAD_SAFE
#include "hls_stream.h"
#include "ap_utils.h"
#include "ap_int.h"
#include "utils.h"
#ifndef __SYNTHESIS__
#include <cassert>
#include <cstring>
#endif /* __SYNTHESIS__ */

template <typename T, bool RD_ENABLED, bool WR_ENABLED, size_t PORTS,
	 size_t MAIN_SIZE, size_t N_SETS, size_t N_WAYS, size_t N_WORDS_PER_LINE,
	 bool LRU, size_t N_L1_SETS, size_t N_L1_WAYS, bool SWAP_TAG_SET, size_t LATENCY>
class cache {
	private:
		static const bool L1_CACHE = ((N_L1_SETS * N_L1_WAYS) > 0);
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

#ifdef __SYNTHESIS__
		typedef ap_uint<(ADDR_SIZE > 0) ? ADDR_SIZE : 1> main_addr_type;
#else
		typedef unsigned int main_addr_type;
#endif /* __SYNTHESIS__ */
		typedef address<ADDR_SIZE, TAG_SIZE, SET_SIZE, WAY_SIZE, SWAP_TAG_SET>
			address_type;
#ifdef __SYNTHESIS__
		typedef ap_uint<WORD_SIZE * N_WORDS_PER_LINE> line_type;
#else
		typedef T line_type[N_WORDS_PER_LINE];
#endif /* __SYNTHESIS__ */
		typedef l1_cache<line_type, MAIN_SIZE, N_L1_SETS, N_L1_WAYS,
			N_WORDS_PER_LINE, SWAP_TAG_SET> l1_cache_type;
		typedef raw_cache<line_type, (N_SETS * N_WAYS * N_WORDS_PER_LINE), 2>
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
			main_addr_type addr;
			T data;
		} core_req_type;

		typedef struct {
			op_type op;
			main_addr_type load_addr;
			main_addr_type write_back_addr;
			line_type line;
		} mem_req_type;

#ifdef __SYNTHESIS__
		ap_uint<(TAG_SIZE > 0) ? TAG_SIZE : 1> m_tag[N_SETS * N_WAYS];	// 0
		ap_uint<N_SETS * N_WAYS> m_valid;				// 1
		ap_uint<N_SETS * N_WAYS> m_dirty;				// 2
#else
		unsigned int m_tag[N_SETS * N_WAYS];
		bool m_valid[N_SETS * N_WAYS];
		bool m_dirty[N_SETS * N_WAYS];
#endif /* __SYNTHESIS__ */
		line_type m_cache_mem[N_SETS * N_WAYS];				// 3
#ifdef __SYNTHESIS__
		hls::stream<core_req_type, (LATENCY * PORTS)> m_core_req[PORTS];// 4
		hls::stream<line_type, (LATENCY * PORTS)> m_core_resp[PORTS];	// 5
		hls::stream<mem_req_type, 2> m_mem_req;				// 6
		hls::stream<line_type, 2> m_mem_resp;				// 7
#endif /* __SYNTHESIS__ */
		raw_cache_type m_raw_cache_core;				// 8
		l1_cache_type m_l1_cache_get[PORTS];				// 9
		replacer_type m_replacer;					// 10
		unsigned int m_core_port;					// 11
#ifndef __SYNTHESIS__
		T *m_main_mem;
		int m_n_reqs[PORTS] = {0};
		int m_n_hits[PORTS] = {0};
		int m_n_l1_reqs[PORTS] = {0};
		int m_n_l1_hits[PORTS] = {0};
#endif /* __SYNTHESIS__ */

	public:
		cache(T *main_mem) {
			cache();
			run(main_mem);
		}

		cache() {
#pragma HLS array_partition variable=m_tag type=complete dim=0
			if (PORTS > 1) {
#pragma HLS array_partition variable=m_core_req type=complete dim=0
#pragma HLS array_partition variable=m_core_resp type=complete dim=0
#pragma HLS array_partition variable=m_l1_cache_get type=complete dim=0
			}
		}

		/**
		 * \brief	Initialize the cache.
		 *
		 * \note	Must be called before calling \ref run.
		 */
		void init() {
#ifndef __SYNTHESIS__
			// invalidate all cache lines
			std::memset(m_valid, 0, sizeof(m_valid));

			m_replacer.init();
			m_raw_cache_core.init();
#endif /* __SYNTHESIS__ */

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
		 * \note		This function must be before
		 * 			the function in which cache is
		 * 			accessed.
		 */
		void run(T *main_mem) {
#pragma HLS inline
#ifdef __SYNTHESIS__
			run_core();
			run_mem_if(main_mem);
#else
			m_main_mem = main_mem;
#endif /* __SYNTHESIS__ */
		}

		/**
		 * \brief	Stop cache internal processes.
		 *
		 * \note	Must be called after the function in which cache
		 * 		is accessed has completed.
		 */
		void stop() {
#ifdef __SYNTHESIS__
			m_core_req[m_core_port].write((core_req_type){.op = STOP_OP});
#else
			flush();
#endif /* __SYNTHESIS__ */
		}

#ifdef __SYNTHESIS__
		bool write_req(const core_req_type req, const unsigned int port) {
#pragma HLS function_instantiate variable=port
			return m_core_req[port].write_dep(req, false);
		}

		void read_resp(line_type &line, bool dep, const unsigned int port) {
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
		void get_line(const main_addr_type addr_main,
				const unsigned int port, line_type &line) {
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
				core_req_type req = {
					.op = READ_OP,
					.addr = addr_main
				};

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
				hit_status = exec_core_req(req, port, line);
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
		T get(const main_addr_type addr_main, const unsigned int port) {
#pragma HLS inline
			line_type line;

			// get the whole cache line
			get_line(addr_main, port, line);

			// extract information from address
			address_type addr(addr_main);

#ifdef __SYNTHESIS__
			const auto LSB = (addr.m_off * WORD_SIZE);
			const auto MSB = (LSB + WORD_SIZE - 1);
			ap_uint<WORD_SIZE> buff = line(LSB, MSB);
			return *reinterpret_cast<T *>(&buff);
#else
			return line[addr.m_off];
#endif /* __SYNTHESIS__ */
		}

		/**
		 * \brief		Request to read a data element.
		 *
		 * \param[in] addr_main	The address in main memory referring to
		 * 			the data element to be read.
		 *
		 * \return		The read data element.
		 */
		T get(const main_addr_type addr_main) {
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
		void set(const main_addr_type addr_main, const T data) {
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
			const auto hit_status = exec_core_req(req, 0, dummy);
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
		void exec_core_req(core_req_type &req,
				const unsigned int port, line_type &line) {
#else
		hit_status_type exec_core_req(core_req_type &req,
				const unsigned int port, line_type &line) {
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
			address_type *wb_addr_ptr;
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
					wb_addr_ptr = &write_back_addr;
					mem_req.op = READ_WRITE_OP;
					mem_req.write_back_addr = write_back_addr.m_addr_main;
				}
			}

			if (is_hit || (mem_req.op == READ_WRITE_OP)) {
				// read from cache memory
				m_raw_cache_core.get_line(m_cache_mem,
						is_hit ? addr.m_addr_line : wb_addr_ptr->m_addr_line,
						is_hit ? line : mem_req.line);
			}
	
			if (!is_hit) {
#ifdef __SYNTHESIS__
				// send read request to
				// memory interface and
				// write request if
				// write-back is necessary
				m_mem_req.write(mem_req);

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
				exec_mem_req(m_main_mem, mem_req, line);
#endif /* __SYNTHESIS__ */

				m_tag[addr.m_addr_line] = addr.m_tag;
				m_valid[addr.m_addr_line] = true;
				m_dirty[addr.m_addr_line] = false;

				m_replacer.notify_insertion(addr);

				if (read) {
					// store loaded line to cache
					m_raw_cache_core.set_line(m_cache_mem,
							addr.m_addr_line, line);
				}
			}

			if (!read) {
#ifdef __SYNTHESIS__
				// modify the line
				const auto LSB = (addr.m_off * WORD_SIZE);
				const auto MSB = (LSB + WORD_SIZE - 1);
				line(LSB, MSB) = *reinterpret_cast<ap_uint<WORD_SIZE> *>(&(req.data));
#else
				line[addr.m_off] = req.data;
#endif /* __SYNTHESIS__ */

				// store the modified line to cache
				m_raw_cache_core.set_line(m_cache_mem,
						addr.m_addr_line, line);


				m_dirty[addr.m_addr_line] = true;
			}

#ifndef __SYNTHESIS__
			return (is_hit ? HIT : MISS);
#endif /* __SYNTHESIS__ */
		}

		void exec_mem_req(T *main_mem, mem_req_type &req,
				line_type &line) {
#pragma HLS inline
			if ((req.op == READ_OP) || (req.op == READ_WRITE_OP)) {
				// read line from main memory
				get_line(main_mem, req.load_addr, line);
			}

			if (WR_ENABLED && ((req.op == WRITE_OP) ||
						(req.op == READ_WRITE_OP))) {
				// write line to main memory
				set_line(main_mem, req.write_back_addr,
						req.line);
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
			m_raw_cache_core.init();

CORE_LOOP:		for (auto port = 0; ; port = ((port + 1) % PORTS)) {
#pragma HLS pipeline II=1 style=flp
#pragma HLS dependence variable=m_cache_mem inter RAW distance=3 true
				core_req_type req;
				// get request and
				// make pipeline flushable (to avoid deadlock)
				if (m_core_req[port].read_nb(req)) {
					// exit the loop if request is "end-of-request"
					if (req.op == STOP_OP)
						break;

					line_type line;
					exec_core_req(req, port, line);

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
			m_mem_req.write((mem_req_type){.op = STOP_OP});
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
				exec_mem_req(main_mem, req, line);

				if ((req.op == READ_OP) || (req.op == READ_WRITE_OP)) {
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
#ifdef __SYNTHESIS__
						line = m_cache_mem[addr.m_addr_line];
#else
						std::memcpy(line,
								m_cache_mem[addr.m_addr_line],
								sizeof(line));
#endif /* __SYNTHESIS__ */

						mem_req_type req = {
							.op = WRITE_OP,
							.load_addr = 0,
							.write_back_addr = addr.m_addr_main};
#ifdef __SYNTHESIS__
						req.line = line;
#else
						std::memcpy(req.line, line,
								sizeof(req.line));
#endif /* __SYNTHESIS__ */

#ifdef __SYNTHESIS__
						// send write request to memory
						// interface
						m_mem_req.write(req);
#else
						line_type dummy;
						exec_mem_req(m_main_mem, req, dummy);
#endif /* __SYNTHESIS__ */

						m_dirty[addr.m_addr_line] = false;
					}
				}
			}
		}

		void get_line(const T * const mem, const main_addr_type addr,
				line_type &line) {
#pragma HLS inline
			const T * const mem_line = &(mem[addr & (-1U << OFF_SIZE)]);

			for (auto off = 0; off < N_WORDS_PER_LINE; off++) {
#pragma HLS unroll
#ifdef __SYNTHESIS__
				const auto LSB = (off * WORD_SIZE);
				const auto MSB = (LSB + WORD_SIZE - 1);
				line(LSB, MSB) = *const_cast<ap_uint<WORD_SIZE> *>(
						reinterpret_cast<const ap_uint<WORD_SIZE> *>(&mem_line[off]));
#else
				line[off] = mem_line[off];
#endif /* __SYNTHESIS__ */
			}
		}

		void set_line(T * const mem, const main_addr_type addr,
				const line_type &line) {
#pragma HLS inline
			T * const mem_line = &(mem[addr & (-1U << OFF_SIZE)]);

			for (auto off = 0; off < N_WORDS_PER_LINE; off++) {
#pragma HLS unroll
#ifdef __SYNTHESIS__
				const auto LSB = (off * WORD_SIZE);
				const auto MSB = (LSB + WORD_SIZE - 1);
				ap_uint<WORD_SIZE> buff = line(LSB, MSB);
				mem_line[off] = *reinterpret_cast<T *>(&buff);
#else
				mem_line[off] = line[off];
#endif /* __SYNTHESIS__ */
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
				const main_addr_type m_addr_main;
			public:
				square_bracket_proxy(cache *c,
						const main_addr_type addr_main):
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
		square_bracket_proxy operator[](const main_addr_type addr_main) {
#pragma HLS inline
			return square_bracket_proxy(this, addr_main);
		}
};

template<typename>
struct is_cache : std::false_type {};

template <typename T, bool RD_ENABLED, bool WR_ENABLED, size_t PORTS,
	 size_t MAIN_SIZE, size_t N_SETS, size_t N_WAYS, size_t N_WORDS_PER_LINE,
	 bool LRU, size_t N_L1_SETS, size_t N_L1_WAYS, bool SWAP_TAG_SET, size_t LATENCY>
	 struct is_cache<cache<T, RD_ENABLED, WR_ENABLED, PORTS, MAIN_SIZE,
	 N_SETS, N_WAYS, N_WORDS_PER_LINE, LRU, N_L1_SETS, N_L1_WAYS,
	 SWAP_TAG_SET, LATENCY>&> : std::true_type {};

void init() {}

template <typename HEAD_TYPE, typename... TAIL_TYPES>
typename std::enable_if<!is_cache<HEAD_TYPE>::value, void>::type
init(HEAD_TYPE &&head, TAIL_TYPES&&... tail) {
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

#endif /* CACHE_H */

