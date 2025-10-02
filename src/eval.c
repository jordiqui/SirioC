#include "eval.h"
#include "board.h"
#include "bits.h"
#include "nn/evaluate.h"

#include <stdbool.h>
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

#define SIRIO_DEFAULT_NETWORK "resources/nn-1c0000000000.nnue"
#define SIRIO_DEFAULT_NETWORK_ALT "../resources/nn-1c0000000000.nnue"
#define SIRIO_DEFAULT_SMALL_NETWORK "resources/nn-37f18f62d772.nnue"
#define SIRIO_DEFAULT_SMALL_NETWORK_ALT "../resources/nn-37f18f62d772.nnue"
#define SIRIO_DEFAULT_PRIMARY_NAME "nn-1c0000000000.nnue"
#define SIRIO_DEFAULT_SMALL_NAME "nn-37f18f62d772.nnue"
#define SIRIO_SMALL_NETWORK_THRESHOLD 12

static sirio_nn_model g_eval_model;
static sirio_nn_model g_small_model;
static bool g_eval_initialized = false;
static bool g_use_nnue = true;

#ifdef SIRIOC_EMBED_NNUE
extern const unsigned char g_sirio_nnue_default[];
extern const size_t g_sirio_nnue_default_size;
#endif

static int try_load_network(sirio_nn_model* model, const char* primary, const char* fallback) {
    if (primary && sirio_nn_model_load(model, primary)) {
        return 1;
    }
    if (fallback && sirio_nn_model_load(model, fallback)) {
        return 1;
    }
    return 0;
}

static int load_legacy_material_weights(sirio_nn_model* model, const char* path) {
    if (!model || !path || !*path) {
        return 0;
    }

    FILE* file = fopen(path, "r");
    if (!file) {
        return 0;
    }

    // Start from defaults so missing entries inherit sane values.
    sirio_nn_model_init(model);

    char line[256];
    int seen_entry = 0;
    while (fgets(line, sizeof(line), file)) {
        char* cursor = line;
        while (*cursor == ' ' || *cursor == '\t') {
            ++cursor;
        }
        if (*cursor == '\0' || *cursor == '\n' || *cursor == '#') {
            continue;
        }

        char* equals = strchr(cursor, '=');
        if (!equals) {
            continue;
        }

        *equals = '\0';
        const char* key = cursor;
        char* value_str = equals + 1;
        while (*value_str == ' ' || *value_str == '\t') {
            ++value_str;
        }

        char* endptr = NULL;
        long value = strtol(value_str, &endptr, 10);
        if (endptr == value_str) {
            continue;
        }

        if (strcmp(key, "pawn") == 0) {
            sirio_nn_model_set_weight(model, 0, (int)value);
            seen_entry = 1;
        } else if (strcmp(key, "knight") == 0) {
            sirio_nn_model_set_weight(model, 1, (int)value);
            seen_entry = 1;
        } else if (strcmp(key, "bishop") == 0) {
            sirio_nn_model_set_weight(model, 2, (int)value);
            seen_entry = 1;
        } else if (strcmp(key, "rook") == 0) {
            sirio_nn_model_set_weight(model, 3, (int)value);
            seen_entry = 1;
        } else if (strcmp(key, "queen") == 0) {
            sirio_nn_model_set_weight(model, 4, (int)value);
            seen_entry = 1;
        } else if (strcmp(key, "king") == 0) {
            // The king entry is ignored by the material evaluator, but
            // we still treat it as a valid entry to keep compatibility.
            sirio_nn_model_set_weight(model, 5, (int)value);
            seen_entry = 1;
        } else if (strcmp(key, "bias") == 0) {
            sirio_nn_model_set_bias(model, (int)value);
            seen_entry = 1;
        }
    }

    fclose(file);

    if (!seen_entry) {
        return 0;
    }

    printf("info string Loaded legacy material weights from %s\n", path);
    return 1;
}

static int load_from_directory(sirio_nn_model* model, const char* directory, const char* file_name) {
    if (!directory || !*directory || !file_name || !*file_name) {
        return 0;
    }

    char buffer[PATH_MAX];
    size_t dir_len = strlen(directory);
    int needs_sep = dir_len > 0 && directory[dir_len - 1] != '/' && directory[dir_len - 1] != '\\';
    int written;
    if (needs_sep) {
        written = snprintf(buffer, sizeof(buffer), "%s/%s", directory, file_name);
    } else {
        written = snprintf(buffer, sizeof(buffer), "%s%s", directory, file_name);
    }
    if (written <= 0 || (size_t)written >= sizeof(buffer)) {
        return 0;
    }

    return sirio_nn_model_load(model, buffer);
}

static int get_executable_directory(char* buffer, size_t size) {
    if (!buffer || size == 0) {
        return 0;
    }
#ifdef _WIN32
    DWORD length = GetModuleFileNameA(NULL, buffer, (DWORD)size);
    if (length == 0 || length >= size) {
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
        return 0;
    }
    buffer[path_size] = '\0';
    char* slash = strrchr(buffer, '/');
    if (!slash) {
        return 0;
    }
    *slash = '\0';
    return 1;
#else
    ssize_t length = readlink("/proc/self/exe", buffer, size - 1);
    if (length <= 0 || (size_t)length >= size) {
        return 0;
    }
    buffer[length] = '\0';
    char* slash = strrchr(buffer, '/');
    if (!slash) {
        return 0;
    }
    *slash = '\0';
    return 1;
#endif
}

static int try_load_default_locations(sirio_nn_model* model, const char* file_name) {
    if (!file_name || !*file_name) {
        return 0;
    }

    if (sirio_nn_model_load(model, file_name)) {
        return 1;
    }

    const char* env_dirs[] = {
        getenv("SIRIOC_RESOURCE_DIR"),
        getenv("SIRIO_RESOURCE_DIR"),
    };

    for (size_t i = 0; i < sizeof(env_dirs) / sizeof(env_dirs[0]); ++i) {
        if (load_from_directory(model, env_dirs[i], file_name)) {
            return 1;
        }

        if (env_dirs[i] && *env_dirs[i]) {
            char buffer[PATH_MAX];
            int written = snprintf(buffer, sizeof(buffer), "%s/resources", env_dirs[i]);
            if (written > 0 && (size_t)written < sizeof(buffer)) {
                if (load_from_directory(model, buffer, file_name)) {
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
        if (load_from_directory(model, prefixes[i], file_name)) {
            return 1;
        }
    }

    char exe_dir[PATH_MAX];
    if (get_executable_directory(exe_dir, sizeof(exe_dir))) {
        if (load_from_directory(model, exe_dir, file_name)) {
            return 1;
        }

        const char* suffixes[] = { "resources", "..", "../resources", "../../resources" };
        for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); ++i) {
            char buffer[PATH_MAX];
            if (suffixes[i][0] == '\0') {
                continue;
            }
            int written = snprintf(buffer, sizeof(buffer), "%s/%s", exe_dir, suffixes[i]);
            if (written > 0 && (size_t)written < sizeof(buffer)) {
                if (load_from_directory(model, buffer, file_name)) {
                    return 1;
                }
            }
        }
    }

    return 0;
}

static void eval_ensure_initialized(void) {
    if (g_eval_initialized) {
        return;
    }

    sirio_nn_model_init(&g_eval_model);
    sirio_nn_model_init(&g_small_model);

    int loaded_eval = try_load_network(&g_eval_model, SIRIO_DEFAULT_NETWORK, SIRIO_DEFAULT_NETWORK_ALT);
    if (!loaded_eval) {
        loaded_eval = try_load_default_locations(&g_eval_model, SIRIO_DEFAULT_PRIMARY_NAME);
    }
    if (!loaded_eval) {
        loaded_eval = load_legacy_material_weights(&g_eval_model, "resources/network.dat");
    }
    if (!loaded_eval) {
        loaded_eval = load_legacy_material_weights(&g_eval_model, "../resources/network.dat");
    }
#ifdef SIRIOC_EMBED_NNUE
    if (!loaded_eval && g_sirio_nnue_default_size > 0 &&
        sirio_nn_model_load_buffer(&g_eval_model, g_sirio_nnue_default, g_sirio_nnue_default_size)) {
        loaded_eval = 1;
        printf("info string Loaded embedded default NNUE weights\n");
    }
#endif
    if (!loaded_eval) {
        printf(
            "info string No NNUE weights loaded from %s; falling back to material until EvalFile is set\n",
            SIRIO_DEFAULT_NETWORK);
    }

    if (!try_load_network(&g_small_model, SIRIO_DEFAULT_SMALL_NETWORK, SIRIO_DEFAULT_SMALL_NETWORK_ALT) &&
        !try_load_default_locations(&g_small_model, SIRIO_DEFAULT_SMALL_NAME)) {
        printf(
            "info string No secondary network loaded from %s; EvalFileSmall can be used to supply one\n",
            SIRIO_DEFAULT_SMALL_NETWORK);
        sirio_nn_model_free(&g_small_model);
        sirio_nn_model_init(&g_small_model);
    }

    g_eval_initialized = true;
}

void eval_init(void) {
    eval_ensure_initialized();
}

void eval_shutdown(void) {
    if (!g_eval_initialized) {
        return;
    }
    sirio_nn_model_free(&g_eval_model);
    sirio_nn_model_free(&g_small_model);
    g_eval_initialized = false;
}

int eval_load_network(const char* path) {
    eval_ensure_initialized();
    if (!path || !*path) {
        return 0;
    }
    int ok = sirio_nn_model_load(&g_eval_model, path);
    if (ok) {
        printf("info string Primary NNUE network loaded from %s\n", path);
    }
    return ok;
}

int eval_load_network_from_buffer(const void* data, size_t size) {
    eval_ensure_initialized();
    if (!data || size == 0) {
        return 0;
    }
    int ok = sirio_nn_model_load_buffer(&g_eval_model, data, size);
    if (ok) {
        printf("info string Primary NNUE network loaded from buffer (%zu bytes)\n", size);
    }
    return ok;
}

int eval_load_small_network(const char* path) {
    eval_ensure_initialized();
    if (!path || !*path) {
        sirio_nn_model_free(&g_small_model);
        sirio_nn_model_init(&g_small_model);
        printf("info string Secondary NNUE network cleared\n");
        return 1;
    }
    int ok = sirio_nn_model_load(&g_small_model, path);
    if (ok) {
        printf("info string Secondary NNUE network loaded from %s\n", path);
    }
    return ok;
}

void eval_set_use_nnue(bool use_nnue) {
    g_use_nnue = use_nnue;
}

bool eval_use_nnue(void) {
    return g_use_nnue;
}

bool eval_has_small_network(void) {
    eval_ensure_initialized();
    return sirio_nn_model_ready(&g_small_model);
}

static int board_piece_count(const Board* board) {
    int total = 0;
    for (int color = COLOR_WHITE; color < COLOR_NB; ++color) {
        for (int pt = PIECE_PAWN; pt < PIECE_TYPE_NB; ++pt) {
            const Bitboard bb = board->bitboards[pt + PIECE_TYPE_NB * color];
            total += bits_popcount(bb);
        }
    }
    return total;
}

static const sirio_nn_model* select_model(const Board* board) {
    if (!g_use_nnue) {
        return NULL;
    }
    if (!sirio_nn_model_ready(&g_eval_model)) {
        return NULL;
    }
    if (sirio_nn_model_ready(&g_small_model)) {
        int pieces = board_piece_count(board);
        if (pieces <= SIRIO_SMALL_NETWORK_THRESHOLD) {
            return &g_small_model;
        }
    }
    return &g_eval_model;
}

Value eval_position(const Board* board) {
    if (board == NULL) {
        return VALUE_DRAW;
    }

    eval_ensure_initialized();

    const sirio_nn_model* model = select_model(board);
    int score;
    if (model) {
        score = sirio_nn_evaluate_board(model, board);
    } else {
        score = sirio_nn_evaluate_board(&g_eval_model, board);
    }

    if (board->side_to_move == COLOR_BLACK) {
        score = -score;
    }
    return score;
}

