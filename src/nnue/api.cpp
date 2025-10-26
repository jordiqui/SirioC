#include "sirio/nnue/api.hpp"

#include <filesystem>
#include <mutex>
#include <utility>

#include "sirio/evaluation.hpp"
#include "sirio/nnue/backend.hpp"

namespace sirio::nnue {
namespace {

struct ApiState {
    std::mutex mutex;
    bool loaded = false;
    NetworkInfo info{};
};

ApiState &state() {
    static ApiState instance;
    return instance;
}

NetworkInfo build_info(const std::string &path) {
    NetworkInfo result;
    result.path = path;
    result.dims = "PieceCounts[2x6]";
    std::error_code ec;
    auto size = std::filesystem::file_size(path, ec);
    if (!ec) {
        result.bytes = static_cast<std::size_t>(size);
    }
    return result;
}

}  // namespace

bool init(const std::string &path, std::string *error_message) {
    std::string local_error;
    std::string *error_ptr = error_message ? &local_error : nullptr;
    auto backend = sirio::make_nnue_evaluation(path, error_ptr);
    if (!backend) {
        if (error_message) {
            *error_message = local_error;
        }
        return false;
    }

    sirio::set_evaluation_backend(std::move(backend));

    ApiState &api_state = state();
    std::lock_guard lock(api_state.mutex);
    api_state.loaded = true;
    api_state.info = build_info(path);
    return true;
}

void unload() {
    sirio::use_classical_evaluation();
    ApiState &api_state = state();
    std::lock_guard lock(api_state.mutex);
    api_state.loaded = false;
    api_state.info = NetworkInfo{};
}

bool is_loaded() {
    ApiState &api_state = state();
    std::lock_guard lock(api_state.mutex);
    return api_state.loaded;
}

std::optional<NetworkInfo> info() {
    ApiState &api_state = state();
    std::lock_guard lock(api_state.mutex);
    if (!api_state.loaded) {
        return std::nullopt;
    }
    return api_state.info;
}

}  // namespace sirio::nnue

