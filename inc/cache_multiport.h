#ifndef CACHE_MULTIPORT_H
#define CACHE_MULTIPORT_H

#include "cache.h"
#include "arbiter.h"
#ifndef __SYNTHESIS__
#include <thread>
#endif /* __SYNTHESIS__ */

template <typename T, size_t RD_PORTS, size_t MAIN_SIZE, size_t N_SETS,
	 size_t N_WAYS, size_t N_ENTRIES_PER_LINE>
class cache_multiport {
	private:
		typedef arbiter<T, RD_PORTS, N_ENTRIES_PER_LINE> arbiter_t;
		typedef cache<T, RD_PORTS, false, MAIN_SIZE, N_SETS,
			N_WAYS, N_ENTRIES_PER_LINE, false> cache_t;

		cache_t _caches[RD_PORTS];
		unsigned int _rd_port;

	public:
		cache_multiport() {
#pragma HLS array_partition variable=_caches complete
		}

		/**
		 * \brief	Initialize the cache.
		 *
		 * \note	Must be called before calling \ref run,
		 * 		if the cache is enabled to read.
		 */
		void init() {
			for (auto port = 0; port < RD_PORTS; port++) {
#pragma HLS unroll
				_caches[port].init();
			}

			_rd_port = 0;
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
				_caches[port].run(main_mem, port, &arbiter);
			}
#else
			std::thread arbiter_thd([&]{arbiter.run(main_mem);});
			std::thread cache_thds[RD_PORTS];
			for (auto port = 0; port < RD_PORTS; port++) {
#pragma HLS unroll
				auto arbiter_ptr = &arbiter;
				cache_thds[port] = std::thread([=]{
						_caches[port].run(main_mem, port,
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
				_caches[port].stop();
			}
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
			auto port = _rd_port;

			_rd_port = (_rd_port + 1) % RD_PORTS;

			return get(addr_main, port);
		}

		/**
		 * \brief		Request to read a data element.
		 *
		 * \param addr_main	The address in main memory referring to
		 * 			the data element to be read.
		 *
		 * \return		The read data element.
		 */
		T get(unsigned int addr_main, unsigned int port) {
#pragma HLS inline
#pragma HLS function_instantiate variable=port
			return _caches[port].get(addr_main);
		}
};

#endif /* CACHE_MULTIPORT_H */

