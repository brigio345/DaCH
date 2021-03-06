#ifndef STREAMED_ARRAY_H
#define STREAMED_ARRAY_H

#define HLS_STREAM_THREAD_SAFE

#include "hls_stream.h"
#include "ap_int.h"

template<typename T>
class streamedArray {
	public:
		hls::stream<T> rdData, wrData;
		hls::stream<ap_int<32>> rdAddr, wrAddr;	// TODO: substitute 32 with ADDR_SIZE
		T *arr;

	public:
		streamedArray(T *a): arr(a) {}

		T get(int index) {
			rdAddr.write(index);
			return rdData.read();
		}

		void set(int index, T data) {
			wrAddr.write(index);
			wrData.write(data);
		}

		class inner {
			private:
				streamedArray *arr;
				int addr;
			public:
				inner(streamedArray *arr, int addr):
					arr(arr), addr(addr) {}

				operator T() const {
					return arr->get(addr);
				}

				void operator=(T data) {
					arr->set(addr, data);
				}
		};

		inner operator[](const int addr) {
			return inner(this, addr);
		}
};

#endif /* STREAMED_ARRAY_H */

