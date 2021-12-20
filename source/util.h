#pragma once

#define LIBOGC_SUCKS_BEGIN \
	_Pragma("GCC diagnostic push") \
	_Pragma("GCC diagnostic ignored \"-Wpedantic\"")

#define LIBOGC_SUCKS_END \
	_Pragma("GCC diagnostic pop")
