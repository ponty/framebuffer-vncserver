#pragma once

#define VERBOSE 0

#define error_print(...) \
    do { fprintf(stderr, __VA_ARGS__); } while (0)

#define info_print(...) \
    do { fprintf(stderr, __VA_ARGS__); } while (0)

#define debug_print(...) \
    do { if (VERBOSE) fprintf(stderr, __VA_ARGS__); } while (0)
