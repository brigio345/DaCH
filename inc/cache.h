#ifndef CACHE_H
#define CACHE_H

#include "streamedArray.h"

template <typename T>
class cache {
	streamedArray<T> &arr;

	public:
	cache(streamedArray<T> &arr): arr(arr) {}

	void read() {
		while (1) {
			arr.rdData.write(arr.arr[arr.rdAddr.read()]);
		}
	}

	void write() {
		while (1) {
			arr.arr[arr.wrAddr.read()] = arr.wrData.read();
		}
	}
};

#endif /* CACHE_H */

