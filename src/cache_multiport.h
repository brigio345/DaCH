#ifndef CACHE_MULTIPORT_H
#define CACHE_MULTIPORT_H

/**
 * \file	cache_multiport.h
 *
 * \brief 	Interface module allowing to use multiple instances of
 * 		\ref cache as a single multi-port read-only cache.
 * 		This is useful when cache reads are performed in an unrolled loop.
 */

#include "cache.h"
#include "arbiter.h"
#ifndef __SYNTHESIS__
#include <thread>
#endif /* __SYNTHESIS__ */

template <typename T, size_t RD_PORTS, size_t MAIN_SIZE, size_t N_SETS,
	 size_t N_WAYS, size_t N_ENTRIES_PER_LINE>
class cache_multiport {
	private:
		static_assert((RD_PORTS > 0), "RD_PORTS must be greater than 0");

		typedef arbiter<T, RD_PORTS, N_ENTRIES_PER_LINE> arbiter_t;
		typedef cache<T, RD_PORTS, false, MAIN_SIZE, N_SETS,
			N_WAYS, N_ENTRIES_PER_LINE, false> cache_t;

		cache_t m_caches[RD_PORTS];
		unsigned int m_rd_port;

	public:
		cache_multiport() {
#pragma HLS array_partition variable=m_caches complete
		}

		/**
		 * \brief	Initialize the cache.
		 *
		 * \note	Must be called before calling \ref run,
		 * 		if \ref L1_CACHE is \c true.
		 */
		void init() {
			for (auto port = 0; port < RD_PORTS; port++) {
#pragma HLS unroll
				m_caches[port].init();
			}

			m_rd_port = 0;
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
		void run(T *main_mem) {
#pragma HLS inline
			arbiter_t arbiter;
#ifdef __SYNTHESIS__
			arbiter.run(main_mem);
			for (auto port = 0; port < RD_PORTS; port++) {
#pragma HLS unroll
				m_caches[port].run(main_mem, port, &arbiter);
			}
#else
			std::thread arbiter_thd([&]{arbiter.run(main_mem);});
			std::thread cache_thds[RD_PORTS];
			for (auto port = 0; port < RD_PORTS; port++) {
#pragma HLS unroll
				auto arbiter_ptr = &arbiter;
				cache_thds[port] = std::thread([=]{
						m_caches[port].run(main_mem, port,
								arbiter_ptr);
						});
			}

			for (auto port = 0; port < RD_PORTS; port++) {
#pragma HLS unroll
				cache_thds[port].join();
			}
			arbiter_thd.join();
#endif /* __SYNTHESIS__ */
		}

		/**
		 * \brief	Stop cache internal processes.
		 *
		 * \note	Must be called after the function in which cache
		 * 		is accessed has completed.
		 */
		void stop() {
			for (auto port = 0; port < RD_PORTS; port++) {
#pragma HLS unroll
				m_caches[port].stop();
			}
		}

		T operator[](const int addr_main) {
#pragma HLS inline
			return get(addr_main);
		}

#if (defined(PROFILE) && (!defined(__SYNTHESIS__)))
		int get_n_reqs() {
			auto n_reqs = 0;
			for (auto port = 0; port < RD_PORTS; port++)
				n_reqs += m_caches[port].get_n_reqs();

			return n_reqs;
		}

		int get_n_hits() {
			auto n_hits = 0;
			for (auto port = 0; port < RD_PORTS; port++)
				n_hits += m_caches[port].get_n_hits();

			return n_hits;
		}

		double get_hit_ratio() {
			auto n_reqs = static_cast<double>(get_n_reqs());

			if (n_reqs > 0)
				return (get_n_hits() / n_reqs);

			return 0;
		}
#endif /* (defined(PROFILE) && (!defined(__SYNTHESIS__))) */

	private:
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
			auto port = m_rd_port;

			m_rd_port = (m_rd_port + 1) % RD_PORTS;

			return get(addr_main, port);
		}

		/**
		 * \brief		Request to read a data element.
		 *
		 * \param addr_main	The address in main memory referring to
		 * 			the data element to be read.
		 * \param port		The port index at which the request
		 *			is issued.
		 *
		 * \return		The read data element.
		 */
		T get(unsigned int addr_main, unsigned int port) {
#pragma HLS inline
#pragma HLS function_instantiate variable=port
			return m_caches[port][addr_main];
		}
};

#endif /* CACHE_MULTIPORT_H */

