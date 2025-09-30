#pragma once

#include "search.h"

#ifdef __cplusplus
extern "C" {
#endif

void thread_init(ThreadContext* thread, SearchContext* search, int id);
void thread_start(ThreadContext* thread);
void thread_join(ThreadContext* thread);

#ifdef __cplusplus
} /* extern "C" */
#endif

