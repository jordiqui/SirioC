#include "nnue_paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#else
#include <unistd.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static int copy_if_exists(const char* path, char* out, size_t out_size) {
    if (!path || !*path || !out || out_size == 0) {
        return 0;
    }

    FILE* file = fopen(path, "rb");
    if (!file) {
        return 0;
    }
    fclose(file);

    size_t length = strlen(path);
    if (length + 1 > out_size) {
        return 0;
    }

    memcpy(out, path, length + 1);
    return 1;
}

static int join_path(char* buffer, size_t size, const char* lhs, const char* rhs) {
    if (!buffer || size == 0) {
        return 0;
    }
    buffer[0] = '\0';

    if (!lhs || !*lhs) {
        int written = snprintf(buffer, size, "%s", rhs ? rhs : "");
        return written > 0 && (size_t)written < size;
    }

    size_t lhs_len = strlen(lhs);
    int needs_sep = lhs_len > 0 && lhs[lhs_len - 1] != '/' && lhs[lhs_len - 1] != '\\';
    int written;
    if (needs_sep) {
        written = snprintf(buffer, size, "%s/%s", lhs, rhs ? rhs : "");
    } else {
        written = snprintf(buffer, size, "%s%s", lhs, rhs ? rhs : "");
    }
    return written > 0 && (size_t)written < size;
}

static int get_executable_directory(char* buffer, size_t size) {
    if (!buffer || size == 0) {
        return 0;
    }

#ifdef _WIN32
    DWORD length = GetModuleFileNameA(NULL, buffer, (DWORD)size);
    if (length == 0 || length >= size) {
        buffer[0] = '\0';
        return 0;
    }
    while (length > 0) {
        char c = buffer[length - 1];
        if (c == '\\' || c == '/') {
            buffer[length - 1] = '\0';
            return 1;
        }
        --length;
    }
    buffer[0] = '\0';
    return 0;
#elif defined(__APPLE__)
    uint32_t path_size = (uint32_t)size;
    if (_NSGetExecutablePath(buffer, &path_size) != 0 || path_size == 0) {
        buffer[0] = '\0';
        return 0;
    }
    buffer[path_size] = '\0';
    char* slash = strrchr(buffer, '/');
    if (!slash) {
        buffer[0] = '\0';
        return 0;
    }
    *slash = '\0';
    return 1;
#else
    ssize_t length = readlink("/proc/self/exe", buffer, size - 1);
    if (length <= 0 || (size_t)length >= size) {
        buffer[0] = '\0';
        return 0;
    }
    buffer[length] = '\0';
    char* slash = strrchr(buffer, '/');
    if (!slash) {
        buffer[0] = '\0';
        return 0;
    }
    *slash = '\0';
    return 1;
#endif
}

int sirio_nnue_locate(const char* file_name, char* out, size_t out_size) {
    if (!file_name || !*file_name || !out || out_size == 0) {
        return 0;
    }

    if (copy_if_exists(file_name, out, out_size)) {
        return 1;
    }

    const char* env_dirs[] = {
        getenv("SIRIOC_RESOURCE_DIR"),
        getenv("SIRIO_RESOURCE_DIR"),
    };

    for (size_t i = 0; i < sizeof(env_dirs) / sizeof(env_dirs[0]); ++i) {
        if (copy_if_exists(env_dirs[i], out, out_size)) {
            return 1;
        }

        if (env_dirs[i] && *env_dirs[i]) {
            char buffer[PATH_MAX];
            if (join_path(buffer, sizeof(buffer), env_dirs[i], file_name) &&
                copy_if_exists(buffer, out, out_size)) {
                return 1;
            }
            char resources_base[PATH_MAX];
            if (join_path(resources_base, sizeof(resources_base), env_dirs[i], "resources")) {
                if (join_path(buffer, sizeof(buffer), resources_base, file_name) &&
                    copy_if_exists(buffer, out, out_size)) {
                    return 1;
                }
            }
        }
    }

    const char* prefixes[] = {
        "./",
        "resources/",
        "./resources/",
        "../",
        "../resources/",
        "../../",
        "../../resources/",
        "../../../",
        "../../../resources/",
    };

    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
        char buffer[PATH_MAX];
        int written = snprintf(buffer, sizeof(buffer), "%s%s", prefixes[i], file_name);
        if (written <= 0 || (size_t)written >= sizeof(buffer)) {
            continue;
        }
        if (copy_if_exists(buffer, out, out_size)) {
            return 1;
        }
    }

    char exe_dir[PATH_MAX];
    if (get_executable_directory(exe_dir, sizeof(exe_dir))) {
        char buffer[PATH_MAX];
        if (join_path(buffer, sizeof(buffer), exe_dir, file_name) &&
            copy_if_exists(buffer, out, out_size)) {
            return 1;
        }

        const char* suffixes[] = { "resources", "..", "../resources", "../../resources" };
        for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); ++i) {
            char temp[PATH_MAX];
            if (!join_path(temp, sizeof(temp), exe_dir, suffixes[i])) {
                continue;
            }
            if (join_path(buffer, sizeof(buffer), temp, file_name) &&
                copy_if_exists(buffer, out, out_size)) {
                return 1;
            }
        }
    }

    return 0;
}

