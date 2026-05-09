#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "sirio/nnue/api.hpp"

namespace {

std::filesystem::path repo_root() {
    return std::filesystem::path(__FILE__).parent_path().parent_path();
}

std::filesystem::path build_fixture_network() {
    const auto root = repo_root();
    const auto out = root / "build" / "test_nnue2_format_detection_v2.bin";
    const auto script = root / "training" / "nnue" / "scripts" / "export_to_engine_v2.py";
    const std::string cmd = "python " + script.string() + " --output " + out.string();
    assert(std::system(cmd.c_str()) == 0);
    return out;
}

void test_detects_valid_sirio_nnue2_minimal_v1() {
    const auto network_path = build_fixture_network();
    const auto info = sirio::nnue::detect_nnue_network_format(network_path.string());
    assert(info.format == sirio::nnue::NnueNetworkFormat::SirioNNUE2MinimalV1);
    assert(info.header_magic_valid);
    assert(info.layout_name == "SirioNNUE2-MinimalV1");
    assert(info.layout_version == "1");
    assert(info.feature_set == "SirioHalfKAv1");
}

void test_missing_file_is_safe_unknown() {
    const auto info = sirio::nnue::detect_nnue_network_format("/tmp/sirio_missing_file.nnue");
    assert(info.format == sirio::nnue::NnueNetworkFormat::Unknown);
}

void test_malformed_binary_is_rejected() {
    const auto path = repo_root() / "build" / "test_nnue2_format_detection_malformed.nnue2";
    std::ofstream out(path, std::ios::binary);
    const char bytes[] = {'S', 'i', 'r', 'i', 'o', 'N', 'N', 'U', 'E', '2'};
    out.write(bytes, sizeof(bytes));
    out.close();

    const auto info = sirio::nnue::detect_nnue_network_format(path.string());
    assert(info.format == sirio::nnue::NnueNetworkFormat::Malformed);
}

void test_fake_stockfish_filename_and_content_is_not_compatible() {
    const auto path = repo_root() / "build" / "nn-62ef826d1a6d.nnue";
    std::ofstream out(path, std::ios::binary);
    out << "stockfish_nnue_payload";
    out.close();

    const auto info = sirio::nnue::detect_nnue_network_format(path.string());
    assert(info.format == sirio::nnue::NnueNetworkFormat::Unknown ||
           info.format == sirio::nnue::NnueNetworkFormat::Malformed ||
           info.format == sirio::nnue::NnueNetworkFormat::Unsupported);
    assert(info.format != sirio::nnue::NnueNetworkFormat::SirioNNUE2MinimalV1);
    assert(info.format != sirio::nnue::NnueNetworkFormat::SirioNNUE1Legacy);
}

void test_detects_sirio_nnue1_legacy_fixture() {
    const auto path = repo_root() / "tests" / "data" / "minimal.nnue";
    const auto info = sirio::nnue::detect_nnue_network_format(path.string());
    assert(info.format == sirio::nnue::NnueNetworkFormat::SirioNNUE1Legacy);
}

void test_detection_does_not_mutate_global_nnue_state() {
    sirio::nnue::unload();
    assert(!sirio::nnue::is_loaded());

    const auto path = repo_root() / "tests" / "data" / "minimal.nnue";
    const auto info = sirio::nnue::detect_nnue_network_format(path.string());
    assert(info.format == sirio::nnue::NnueNetworkFormat::SirioNNUE1Legacy);
    assert(!sirio::nnue::is_loaded());
}

}  // namespace

void run_nnue_format_detection_v2_tests() {
    test_detects_valid_sirio_nnue2_minimal_v1();
    test_missing_file_is_safe_unknown();
    test_malformed_binary_is_rejected();
    test_fake_stockfish_filename_and_content_is_not_compatible();
    test_detects_sirio_nnue1_legacy_fixture();
    test_detection_does_not_mutate_global_nnue_state();
}
