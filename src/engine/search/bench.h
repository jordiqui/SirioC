#pragma once

#include "search.h"

#ifdef __cplusplus
extern "C" {
#endif

void bench_run(SearchContext* context, const SearchLimits* limits);

#ifdef __cplusplus
} /* extern "C" */
#endif

