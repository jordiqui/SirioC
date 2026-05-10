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

std::string network_format_name(NnueNetworkFormat format) {
    switch (format) {
    case NnueNetworkFormat::Unknown:
        return "Unknown";
    case NnueNetworkFormat::SirioNNUE1Legacy:
        return "SirioNNUE1Legacy";
    case NnueNetworkFormat::SirioNNUE2MinimalV1:
        return "SirioNNUE2MinimalV1";
    case NnueNetworkFormat::Malformed:
        return "Malformed";
    case NnueNetworkFormat::Unsupported:
        return "Unsupported";
    }
    return "Unknown";
}

NetworkInfo build_info(const std::string &path) {
    NetworkInfo result;
    result.path = path;
    const NnueNetworkFormatInfo detected = detect_nnue_network_format(path);
    Nnue2NetworkParameters network{};
    std::string error;
    if (detected.format == NnueNetworkFormat::SirioNNUE2MinimalV1 &&
        load_nnue2_network_file(path, network, error)) {
        Nnue2MinimalDecodedLayout layout{};
        if (decode_nnue2_minimal_layout(network, layout, error)) {
            std::ostringstream report;
            report << "support_present=true"
                   << ";stockfish_nnue_compatibility=not_claimed"
                   << ";sirio_nnue1_nnue_names=legacy_sirio_format"
                   << ";sirio_nnue2_runtime_status=non_default"
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
                   << ";detected_format=" << network_format_name(detected.format)
                   << ";detector_diagnostic=" << detected.diagnostic
                   << ";checksum=0x" << std::hex << network.header.checksum;
            result.format_report = report.str();
            result.dims = "SirioNNUE2-MinimalV1 HalfKAv1[40960]->Hidden[256]->Out[1]";
        } else {
            result.dims = "SirioNNUE2 header detected but layout decode failed";
        }
    } else {
        result.dims = "SirioNNUE1 PieceCounts[2x6]";
        result.format_report =
            "support_present=false;stockfish_nnue_compatibility=not_claimed;"
            "sirio_nnue1_nnue_names=legacy_sirio_format;sirio_nnue2_runtime_status=non_default;"
            "legacy_sirio_nnue1_status=legacy_test_baseline;checksum=unavailable";
    }
    std::error_code ec;
    auto size = std::filesystem::file_size(path, ec);
    if (!ec) {
        result.bytes = static_cast<std::size_t>(size);
    }
    return result;
}

}  // namespace

NnueNetworkFormatInfo detect_nnue_network_format(const std::string &path) {
    NnueNetworkFormatInfo info;
    info.path = path;

    std::ifstream text_input(path);
    if (!text_input) {
        info.format = NnueNetworkFormat::Unknown;
        info.diagnostic = "Unable to open file";
        return info;
    }

    std::string legacy_header;
    if ((text_input >> legacy_header) && legacy_header == "SirioNNUE1") {
        double bias = 0.0;
        double scale = 0.0;
        if (!(text_input >> bias >> scale)) {
            info.format = NnueNetworkFormat::Malformed;
            info.diagnostic = "SirioNNUE1 header present but bias/scale missing";
            return info;
        }
        double weight = 0.0;
        for (std::size_t index = 0; index < kFeatureCount; ++index) {
            if (!(text_input >> weight)) {
                info.format = NnueNetworkFormat::Malformed;
                info.diagnostic = "SirioNNUE1 weight table is incomplete";
                return info;
            }
        }
        info.format = NnueNetworkFormat::SirioNNUE1Legacy;
        info.layout_name = "SirioNNUE1";
        info.layout_version = "legacy";
        info.feature_set = "PieceCounts2x6";
        info.checksum = "unavailable";
        info.diagnostic = "Legacy SirioNNUE1 contract parsed";
        return info;
    }

    Nnue2NetworkParameters network{};
    std::string load_error;
    if (load_nnue2_network_file(path, network, load_error)) {
        Nnue2MinimalDecodedLayout layout{};
        std::string decode_error;
        if (!decode_nnue2_minimal_layout(network, layout, decode_error)) {
            info.format = NnueNetworkFormat::Unsupported;
            info.header_magic_valid = true;
            info.diagnostic = decode_error;
            return info;
        }
        info.format = NnueNetworkFormat::SirioNNUE2MinimalV1;
        info.header_magic_valid = true;
        info.feature_set = layout.feature_set;
        info.layout_name = layout.model_layout_name;
        info.layout_version = std::to_string(layout.model_layout_version);
        std::ostringstream checksum;
        checksum << "0x" << std::hex << network.header.checksum;
        info.checksum = checksum.str();
        info.diagnostic = "SirioNNUE2-MinimalV1 header and layout validated";
        return info;
    }

    if (load_error.find("Truncated") != std::string::npos ||
        load_error.find("Unexpected trailing bytes") != std::string::npos) {
        info.format = NnueNetworkFormat::Malformed;
        info.diagnostic = load_error;
        return info;
    }
    if (load_error.find("Invalid SirioNNUE2 header contract") != std::string::npos) {
        info.format = NnueNetworkFormat::Unknown;
        info.diagnostic = "No SirioNNUE2/SirioNNUE1 format match";
        return info;
    }
    info.format = NnueNetworkFormat::Unsupported;
    info.diagnostic = load_error;
    return info;
}

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
