#include "engine/eval/nnue/evaluator.hpp"

#include "engine/core/board.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cstddef>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace engine::nnue {

namespace {

constexpr std::string_view kMagic{"NNUE"};

bool read_bytes(std::ifstream& in, void* buffer, std::size_t size) {
    return static_cast<bool>(in.read(reinterpret_cast<char*>(buffer), static_cast<std::streamsize>(size)));
}

struct Section {
    std::string id;
    std::vector<std::uint8_t> payload;
};

bool read_section(std::ifstream& in, Section& section) {
    char raw_id[4];
    uint32_t size = 0;
    if (!read_bytes(in, raw_id, sizeof(raw_id))) {
        return false;
    }
    if (!read_bytes(in, &size, sizeof(size))) {
        return false;
    }
    section.id.assign(raw_id, raw_id + 4);
    section.payload.resize(size);
    if (size == 0) {
        return true;
    }
    return static_cast<bool>(in.read(reinterpret_cast<char*>(section.payload.data()), static_cast<std::streamsize>(size)));
}

int32_t read_i32(const std::uint8_t* data) {
    int32_t value = 0;
    std::memcpy(&value, data, sizeof(value));
    return value;
}

uint32_t read_u32(const std::uint8_t* data) {
    uint32_t value = 0;
    std::memcpy(&value, data, sizeof(value));
    return value;
}

} // namespace

bool Evaluator::load_network(const std::string& path) {
    last_error_.clear();
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        last_error_ = "unable to open file";
        loaded_ = false;
        accumulator_.reset();
        return false;
    }

    char magic[4];
    if (!read_bytes(in, magic, sizeof(magic))) {
        last_error_ = "failed to read header";
        loaded_ = false;
        accumulator_.reset();
        return false;
    }
    if (std::string_view(magic, sizeof(magic)) != kMagic) {
        last_error_ = "invalid magic";
        loaded_ = false;
        accumulator_.reset();
        return false;
    }

    uint32_t version = 0;
    if (!read_bytes(in, &version, sizeof(version))) {
        last_error_ = "missing version";
        loaded_ = false;
        accumulator_.reset();
        return false;
    }

    char arch_raw[16];
    if (!read_bytes(in, arch_raw, sizeof(arch_raw))) {
        last_error_ = "missing architecture";
        loaded_ = false;
        accumulator_.reset();
        return false;
    }
    std::string arch(arch_raw, arch_raw + sizeof(arch_raw));
    const auto null_pos = arch.find('\0');
    if (null_pos != std::string::npos) {
        arch.resize(null_pos);
    }

    Network candidate;
    candidate.version = version;
    candidate.architecture = arch;

    bool have_desc = false;
    bool have_weights = false;
    bool have_bias = false;
    bool have_out_weights = false;
    bool have_out_bias = false;

    while (in.peek() != EOF) {
        Section section;
        if (!read_section(in, section)) {
            last_error_ = "truncated section";
            loaded_ = false;
            accumulator_.reset();
            return false;
        }

        if (section.id == "DESC") {
            if (section.payload.size() != sizeof(uint32_t) * 2 + sizeof(int32_t)) {
                last_error_ = "invalid DESC size";
                loaded_ = false;
                accumulator_.reset();
                return false;
            }
            candidate.input_dim = read_u32(section.payload.data());
            candidate.hidden_dim = read_u32(section.payload.data() + sizeof(uint32_t));
            candidate.output_scale = read_i32(section.payload.data() + sizeof(uint32_t) * 2);
            have_desc = true;
        } else if (section.id == "FTWT") {
            if (!have_desc) {
                last_error_ = "FTWT before DESC";
                loaded_ = false;
                accumulator_.reset();
                return false;
            }
            const size_t expected = static_cast<size_t>(candidate.input_dim) * static_cast<size_t>(candidate.hidden_dim);
            if (section.payload.size() != expected * sizeof(int16_t)) {
                last_error_ = "invalid FTWT size";
                loaded_ = false;
                accumulator_.reset();
                return false;
            }
            candidate.feature_weights.resize(expected);
            std::memcpy(candidate.feature_weights.data(), section.payload.data(), section.payload.size());
            have_weights = true;
        } else if (section.id == "FTBS") {
            if (!have_desc) {
                last_error_ = "FTBS before DESC";
                loaded_ = false;
                accumulator_.reset();
                return false;
            }
            if (section.payload.size() != static_cast<size_t>(candidate.hidden_dim) * sizeof(int32_t)) {
                last_error_ = "invalid FTBS size";
                loaded_ = false;
                accumulator_.reset();
                return false;
            }
            candidate.feature_bias.resize(candidate.hidden_dim);
            std::memcpy(candidate.feature_bias.data(), section.payload.data(), section.payload.size());
            have_bias = true;
        } else if (section.id == "OUTW") {
            if (!have_desc) {
                last_error_ = "OUTW before DESC";
                loaded_ = false;
                accumulator_.reset();
                return false;
            }
            if (section.payload.size() != static_cast<size_t>(candidate.hidden_dim) * sizeof(int16_t)) {
                last_error_ = "invalid OUTW size";
                loaded_ = false;
                accumulator_.reset();
                return false;
            }
            candidate.output_weights.resize(candidate.hidden_dim);
            std::memcpy(candidate.output_weights.data(), section.payload.data(), section.payload.size());
            have_out_weights = true;
        } else if (section.id == "OUTB") {
            if (section.payload.size() != sizeof(int32_t)) {
                last_error_ = "invalid OUTB size";
                loaded_ = false;
                accumulator_.reset();
                return false;
            }
            candidate.output_bias = read_i32(section.payload.data());
            have_out_bias = true;
        }
    }

    if (!have_desc || !have_weights || !have_bias || !have_out_weights || !have_out_bias) {
        last_error_ = "missing sections";
        loaded_ = false;
        accumulator_.reset();
        return false;
    }

    if (candidate.output_scale == 0) {
        last_error_ = "invalid output scale";
        loaded_ = false;
        accumulator_.reset();
        return false;
    }

    network_ = std::move(candidate);
    loaded_ = true;
    accumulator_.reset();
    return true;
}

int Evaluator::eval_cp(const engine::Board& board) const {
    if (!loaded_) {
        return 0;
    }

    accumulator_.update(board,
                        network_.input_dim,
                        network_.hidden_dim,
                        network_.feature_weights,
                        network_.feature_bias);

    const auto& hidden = accumulator_.hidden();
    if (hidden.size() != network_.hidden_dim) {
        return 0;
    }

    int64_t sum = network_.output_bias;
    for (uint32_t i = 0; i < network_.hidden_dim; ++i) {
        const int32_t activation = std::max<int32_t>(0, hidden[i]);
        sum += static_cast<int64_t>(network_.output_weights[i]) * static_cast<int64_t>(activation);
    }
    return static_cast<int>(sum / network_.output_scale);
}

} // namespace engine::nnue
