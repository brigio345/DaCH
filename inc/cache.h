#ifndef CACHE_H
#define CACHE_H

#include "streamedArray.h"

template <typename T>
class cache {
	streamedArray<T> &arr;

	public:
	cache(streamedArray<T> &arr): arr(arr) {}

	void read() {
		int addr;

		while (1) {
			addr = arr.rdAddr.read();
			if (addr == -1)
				break;
			arr.rdData.write(arr.arr[addr]);
		}
	}

	void write() {
		int addr;

		while (1) {
			addr = arr.wrAddr.read();
			if (addr == -1)
				break;
			arr.arr[addr] = arr.wrData.read();
		}
	}

	void stopRead() {
		arr.rdAddr.write(-1);
	}

	void stopWrite() {
		arr.wrAddr.write(-1);
	}
};

#endif /* CACHE_H */

