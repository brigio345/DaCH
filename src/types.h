#ifndef TYPES_H
#define TYPES_H

#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wpedantic"
#pragma GCC diagnostic error "-Wall"
#pragma GCC diagnostic error "-Wextra"
#pragma GCC diagnostic ignored "-Wunused-label"
#pragma GCC diagnostic ignored "-Wattributes"

namespace types {
	typedef enum {
		URAM,
		LUTRAM,
		BRAM,
		AUTO
	} storage_impl_type;
}

#pragma GCC diagnostic pop

#endif /* TYPES_H */

