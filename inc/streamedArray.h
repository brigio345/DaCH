#ifndef STREAMED_ARRAY_H
#define STREAMED_ARRAY_H

#include "hls_stream.h"

template<typename T>
class streamedArray {
	public:
		hls::stream<T> rdData, wrData;
		hls::stream<int> rdAddr, wrAddr;
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

