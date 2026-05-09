#include "sirio/nnue/api.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
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
    Nnue2NetworkParameters network{};
    std::string error;
    if (load_nnue2_network_file(path, network, error)) {
        Nnue2MinimalDecodedLayout layout{};
        if (decode_nnue2_minimal_layout(network, layout, error)) {
            std::ostringstream report;
            report << "support_present=true"
                   << ";model_layout_name=" << layout.model_layout_name
                   << ";model_layout_version=" << layout.model_layout_version
                   << ";feature_set=" << layout.feature_set
                   << ";features_per_perspective=" << layout.features_per_perspective
                   << ";accumulator_size=" << layout.accumulator_size
                   << ";hidden1_size=" << layout.hidden1_size
                   << ";hidden2_size=" << layout.hidden2_size
                   << ";output_size=" << layout.output_size
                   << ";activation=" << layout.activation
                   << ";binary_magic=SirioNNUE2"
                   << ";binary_version=" << network.header.version
                   << ";binary_section_order=input_weights,hidden_bias,output_weights,output_bias"
                   << ";quantization_status=deterministic_placeholder_test_scaling"
                   << ";quantization_production=deferred"
                   << ";legacy_sirio_nnue1_status=legacy_test_baseline"
                   << ";checksum=0x" << std::hex << network.header.checksum;
            result.format_report = report.str();
            result.dims = "SirioNNUE2-MinimalV1 HalfKAv1[40960]->Hidden[256]->Out[1]";
        } else {
            result.dims = "SirioNNUE2 header detected but layout decode failed";
        }
    } else {
        result.dims = "SirioNNUE1 PieceCounts[2x6]";
        result.format_report =
            "support_present=false;legacy_sirio_nnue1_status=legacy_test_baseline;checksum=unavailable";
    }
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

    if (error_message) {
        error_message->clear();
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
