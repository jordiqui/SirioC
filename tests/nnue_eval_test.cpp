#include "engine/core/board.hpp"
#include "engine/eval/nnue/evaluator.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <system_error>

namespace {

struct ScopedNetworkFile {
    std::filesystem::path path;
    [[nodiscard]] bool valid() const noexcept { return !path.empty(); }
    ~ScopedNetworkFile() {
        if (!path.empty()) {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    }
};

ScopedNetworkFile create_test_network() {
    namespace fs = std::filesystem;
    ScopedNetworkFile file;

    std::error_code ec;
    fs::path temp_dir = fs::temp_directory_path(ec);
    if (ec) {
        return file;
    }

    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path path = temp_dir / ("sirio_nnue_test-" + std::to_string(now) + ".nnue");

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return file;
    }

    const std::array<char, 4> magic{'N', 'N', 'U', 'E'};
    out.write(magic.data(), static_cast<std::streamsize>(magic.size()));

    const std::uint32_t version = 1;
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));

    std::array<char, 16> arch{};
    const char arch_name[] = "TestNetwork";
    std::copy(std::begin(arch_name), std::end(arch_name) - 1, arch.begin());
    out.write(arch.data(), static_cast<std::streamsize>(arch.size()));

    const std::uint32_t input_dim = 14;
    const std::uint32_t hidden_dim = 1;
    const std::int32_t output_scale = 1;

    const auto write_section = [&out](const std::array<char, 4>& id, std::uint32_t size) {
        out.write(id.data(), static_cast<std::streamsize>(id.size()));
        out.write(reinterpret_cast<const char*>(&size), sizeof(size));
    };

    write_section({'D', 'E', 'S', 'C'}, sizeof(input_dim) * 2 + sizeof(output_scale));
    out.write(reinterpret_cast<const char*>(&input_dim), sizeof(input_dim));
    out.write(reinterpret_cast<const char*>(&hidden_dim), sizeof(hidden_dim));
    out.write(reinterpret_cast<const char*>(&output_scale), sizeof(output_scale));

    std::array<std::int16_t, input_dim> feature_weights{};
    feature_weights[0] = 100;  // white pawn count
    feature_weights[6] = -100; // black pawn count
    write_section({'F', 'T', 'W', 'T'}, static_cast<std::uint32_t>(feature_weights.size() * sizeof(std::int16_t)));
    out.write(reinterpret_cast<const char*>(feature_weights.data()),
              static_cast<std::streamsize>(feature_weights.size() * sizeof(std::int16_t)));

    const std::array<std::int32_t, hidden_dim> feature_bias{};
    write_section({'F', 'T', 'B', 'S'}, static_cast<std::uint32_t>(feature_bias.size() * sizeof(std::int32_t)));
    out.write(reinterpret_cast<const char*>(feature_bias.data()),
              static_cast<std::streamsize>(feature_bias.size() * sizeof(std::int32_t)));

    const std::array<std::int16_t, hidden_dim> output_weights{2};
    write_section({'O', 'U', 'T', 'W'}, static_cast<std::uint32_t>(output_weights.size() * sizeof(std::int16_t)));
    out.write(reinterpret_cast<const char*>(output_weights.data()),
              static_cast<std::streamsize>(output_weights.size() * sizeof(std::int16_t)));

    const std::int32_t output_bias = -100;
    write_section({'O', 'U', 'T', 'B'}, sizeof(output_bias));
    out.write(reinterpret_cast<const char*>(&output_bias), sizeof(output_bias));

    if (!out) {
        return file;
    }

    out.close();
    file.path = std::move(path);
    return file;
}

ScopedNetworkFile create_invalid_network() {
    namespace fs = std::filesystem;
    ScopedNetworkFile file;

    std::error_code ec;
    fs::path temp_dir = fs::temp_directory_path(ec);
    if (ec) {
        return file;
    }

    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path path = temp_dir / ("sirio_nnue_invalid-" + std::to_string(now) + ".nnue");

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return file;
    }

    const std::array<char, 4> bad_magic{'B', 'A', 'D', '!'};
    out.write(bad_magic.data(), static_cast<std::streamsize>(bad_magic.size()));
    out.close();

    file.path = std::move(path);
    return file;
}

} // namespace

int main() {
    ScopedNetworkFile network = create_test_network();
    if (!network.valid()) {
        std::cerr << "Failed to create NNUE test network file\n";
        return 1;
    }

    engine::nnue::Evaluator eval;
    if (!eval.load_network(network.path.string())) {
        std::cerr << "Failed to load network: " << eval.last_error() << "\n";
        return 2;
    }

    engine::Board board;
    if (!board.set_fen("8/8/8/8/8/8/8/P6k w - - 0 1")) {
        std::cerr << "Failed to set FEN\n";
        return 3;
    }
    int cp = eval.eval_cp(board);
    if (cp != 100) {
        std::cerr << "Expected 100 cp, got " << cp << "\n";
        return 4;
    }

    if (!board.set_fen("8/8/8/8/8/8/8/6pk w - - 0 1")) {
        std::cerr << "Failed to set FEN\n";
        return 5;
    }
    cp = eval.eval_cp(board);
    if (cp != -100) {
        std::cerr << "Expected -100 cp, got " << cp << "\n";
        return 6;
    }

    ScopedNetworkFile invalid = create_invalid_network();
    if (!invalid.valid()) {
        std::cerr << "Failed to create invalid NNUE network file\n";
        return 7;
    }

    if (eval.load_network(invalid.path.string())) {
        std::cerr << "Invalid network was unexpectedly accepted\n";
        return 8;
    }

    if (!eval.is_loaded()) {
        std::cerr << "Evaluator lost previously loaded network after failure\n";
        return 9;
    }

    if (!board.set_fen("8/8/8/8/8/8/8/P6k w - - 0 1")) {
        std::cerr << "Failed to reset FEN\n";
        return 10;
    }
    cp = eval.eval_cp(board);
    if (cp != 100) {
        std::cerr << "Expected cached network to remain usable, got " << cp << "\n";
        return 11;
    }

    return 0;
}
