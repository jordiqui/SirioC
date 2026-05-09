#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "sirio/nnue/api.hpp"

namespace {

std::filesystem::path repo_root() {
    return std::filesystem::path(__FILE__).parent_path().parent_path();
}

std::filesystem::path build_fixture_network() {
    const auto root = repo_root();
    const auto out = root / "build" / "test_nnue2_api_build_info_v2.bin";
    const auto script = root / "training" / "nnue" / "scripts" / "export_to_engine_v2.py";
    const std::string cmd = "python " + script.string() + " --output " + out.string();
    assert(std::system(cmd.c_str()) == 0);
    return out;
}

void test_nnue2_build_info_format_report_contract() {
    const auto network_path = build_fixture_network();
    std::string error;
    assert(sirio::nnue::init(network_path.string(), &error));

    const auto meta = sirio::nnue::info();
    assert(meta.has_value());
    assert(meta->dims.find("SirioNNUE2-MinimalV1") != std::string::npos);
    assert(meta->format_report.find("support_present=true") != std::string::npos);
    assert(meta->format_report.find("model_layout_name=SirioNNUE2-MinimalV1") != std::string::npos);
    assert(meta->format_report.find("model_layout_version=1") != std::string::npos);
    assert(meta->format_report.find("feature_set=SirioHalfKAv1") != std::string::npos);
    assert(meta->format_report.find("features_per_perspective=40960") != std::string::npos);
    assert(meta->format_report.find("accumulator_size=256") != std::string::npos);
    assert(meta->format_report.find("hidden1_size=256") != std::string::npos);
    assert(meta->format_report.find("hidden2_size=0") != std::string::npos);
    assert(meta->format_report.find("output_size=1") != std::string::npos);
    assert(meta->format_report.find("activation=relu") != std::string::npos);
    assert(meta->format_report.find("binary_magic=SirioNNUE2") != std::string::npos);
    assert(meta->format_report.find("binary_section_order=input_weights,hidden_bias,output_weights,output_bias") !=
           std::string::npos);
    assert(meta->format_report.find("quantization_status=deterministic_placeholder_test_scaling") !=
           std::string::npos);
    assert(meta->format_report.find("quantization_production=deferred") != std::string::npos);
    assert(meta->format_report.find("legacy_sirio_nnue1_status=legacy_test_baseline") != std::string::npos);
    assert(meta->format_report.find("Stockfish") == std::string::npos);
    assert(meta->format_report.find("stockfish") == std::string::npos);

    sirio::nnue::unload();
}

}  // namespace

void run_nnue_api_build_info_v2_tests() { test_nnue2_build_info_format_report_contract(); }
