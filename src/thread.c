#include "thread.h"

#include "util.h"

void thread_init(ThreadContext* thread, SearchContext* search, int id) {
    if (thread == NULL) {
        return;
    }
    thread->search = search;
    thread->id = id;
}

void thread_start(ThreadContext* thread) {
    if (thread == NULL || thread->search == NULL) {
        return;
    }
    util_log("thread_start called (stub)");
}

void thread_join(ThreadContext* thread) {
    if (thread == NULL) {
        return;
    }
    util_log("thread_join called (stub)");
}

