#ifndef ARBITER_H
#define ARBITER_H

#define HLS_STREAM_THREAD_SAFE
#include "hls_stream.h"
#include "hls_vector.h"
#include "ap_utils.h"
#include "utils.h"
#ifdef __SYNTHESIS__
#else
#include <thread>
#endif /* __SYNTHESIS__ */

template <typename T, size_t N_READERS, size_t N_ENTRIES_PER_LINE>
class arbiter {
	private:
		static const size_t OFF_SIZE = utils::log2_ceil(N_ENTRIES_PER_LINE);

#ifdef __SYNTHESIS__
		template <typename TYPE, size_t SIZE>
			using array_t = hls::vector<TYPE, SIZE>;
#else
		template <typename TYPE, size_t SIZE>
			using array_t = std::array<TYPE, SIZE>;
#endif /* __SYNTHESIS__ */
		typedef array_t<T, N_ENTRIES_PER_LINE> line_t;

		typedef struct {
			unsigned int addr_main;
			bool stop;
		} request_t;

		hls::stream<request_t, 4> _request[N_READERS];
		hls::stream<line_t, 4> _response[N_READERS];
		unsigned int _reader;

	public:
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
		/**
		 * \brief	Infinite loop managing the cache access requests
		 * 		(sent from the outside).
		 *
		 * \note	The infinite loop must be stopped by calling
		 * 		\ref stop at the end of computation (from the
		 * 		outside).
		 */
		void run(T *main_mem) {
#pragma HLS inline off
ARBITER_LOOP:		while (1) {
#pragma HLS pipeline
				for (auto reader = 0; reader < N_READERS; reader++) {
#pragma HLS unroll
					request_t req;
					// get request and
					// make pipeline flushable (to avoid deadlock)
					// skip reader if it is not issuing a request
					if (!_request[reader].read_nb(req))
						continue;

					// exit the loop if request is "end-of-request"
					if (req.stop)
						return;

					line_t line;
					auto main_line = &(main_mem[req.addr_main & (-1U << OFF_SIZE)]);
					for (auto off = 0; off < N_ENTRIES_PER_LINE; off++) {
#pragma HLS unroll
						line[off] = main_line[off];
					}

					_response[reader].write(line);
				}
			}
		}

		/**
		 * \brief	Stop cache internal processes.
		 *
		 * \note	Must be called after the function in which cache
		 * 		is accessed has completed.
		 */
		void stop(unsigned int reader) {
#pragma HLS function_instantiate variable=reader
			_request[reader].write({0, true});
		}

		/**
		 * \brief		Request to read a data element.
		 *
		 * \param addr_main	The address in main memory referring to
		 * 			the data element to be read.
		 *
		 * \return		The read data element.
		 */
		line_t get(unsigned int addr_main, unsigned int reader) {
#pragma HLS inline
#pragma HLS function_instantiate variable=reader
			_request[reader].write({addr_main, false});

			ap_wait();

			return _response[reader].read();
		}
};

#endif /* ARBITER_H */

