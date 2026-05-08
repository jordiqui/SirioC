#include <array>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include "sirio/nnue/backend.hpp"
#include "sirio/nnue/features.hpp"

namespace {

std::filesystem::path repo_root() {
    return std::filesystem::path(__FILE__).parent_path().parent_path();
}

void test_export_and_load_roundtrip() {
    const auto root = repo_root();
    const auto out1 = root / "build" / "test_nnue2_a.bin";
    const auto out2 = root / "build" / "test_nnue2_b.bin";
    const auto script = root / "training" / "nnue" / "scripts" / "export_to_engine_v2.py";

    const std::string cmd1 = "python " + script.string() + " --output " + out1.string();
    const std::string cmd2 = "python " + script.string() + " --output " + out2.string();
    assert(std::system(cmd1.c_str()) == 0);
    assert(std::system(cmd2.c_str()) == 0);

    std::string error;
    sirio::nnue::Nnue2NetworkParameters net;
    assert(sirio::nnue::load_nnue2_network_file(out1.string(), net, error));
    assert(net.header.version == 2);
    assert(net.header.feature_set_id == 1);
    assert(net.header.features_per_perspective == sirio::nnue::kSirioHalfKAv1FeaturesPerPerspective);
    assert(net.header.accumulator_size == sirio::nnue::kNnue2AccumulatorSize);
    assert(net.header.hidden_dimensions == sirio::nnue::kNnue2AccumulatorSize);
    assert(net.header.payload_bytes ==
           net.header.input_weights_bytes + net.header.hidden_bias_bytes +
               net.header.output_weights_bytes + net.header.output_bias_bytes);

    std::ifstream a(out1, std::ios::binary);
    std::ifstream b(out2, std::ios::binary);
    const std::string bytes_a((std::istreambuf_iterator<char>(a)), std::istreambuf_iterator<char>());
    const std::string bytes_b((std::istreambuf_iterator<char>(b)), std::istreambuf_iterator<char>());
    assert(bytes_a == bytes_b);
}

void test_reject_wrong_magic() {
    auto header = sirio::nnue::make_default_nnue2_header();
    header.magic = {'N', 'o', 't', 'S', 'i', 'r', 'i', 'o', '2', '\0', '\0', '\0'};
    assert(!sirio::nnue::is_valid_nnue2_header(header));
}

void test_reject_wrong_feature_count() {
    auto header = sirio::nnue::make_default_nnue2_header();
    header.features_per_perspective = 12;
    assert(!sirio::nnue::is_valid_nnue2_header(header));
}

void test_reject_wrong_version() {
    auto header = sirio::nnue::make_default_nnue2_header();
    header.version = 7;
    assert(!sirio::nnue::is_valid_nnue2_header(header));
}

void test_reject_truncated_file() {
    const auto root = repo_root();
    const auto path = root / "build" / "truncated_nnue2.bin";
    {
        std::ofstream out(path, std::ios::binary);
        auto header = sirio::nnue::make_default_nnue2_header();
        out.write(reinterpret_cast<const char *>(&header), sizeof(header));
    }
    std::string error;
    sirio::nnue::Nnue2NetworkParameters net;
    assert(!sirio::nnue::load_nnue2_network_file(path.string(), net, error));
}

}  // namespace

void run_nnue_roundtrip_tests() {
    test_export_and_load_roundtrip();
    test_reject_wrong_magic();
    test_reject_wrong_feature_count();
    test_reject_wrong_version();
    test_reject_truncated_file();
}
