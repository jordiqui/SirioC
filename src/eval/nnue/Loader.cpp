#include "Loader.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace {
constexpr uint32_t kMagic = 0x454e554e; // "NNUE"

template <typename T>
bool read_vector(std::ifstream &in, std::vector<T> &out, std::size_t count) {
    if (count == 0) {
        out.clear();
        return true;
    }
    out.resize(count);
    in.read(reinterpret_cast<char *>(out.data()), static_cast<std::streamsize>(count * sizeof(T)));
    return static_cast<bool>(in);
}

} // namespace

namespace nnue {

bool loadFile(const std::string &path, Weights &out, std::string &metaInfo) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }

    Header hdr{};
    in.read(reinterpret_cast<char *>(&hdr), sizeof(Header));
    if (!in) {
        return false;
    }

    if (hdr.magic != kMagic || hdr.layerSize == 0 || hdr.inputSize == 0) {
        return false;
    }

    out = Weights{};
    out.hdr = hdr;

    const std::size_t layerSize = hdr.layerSize;
    const std::size_t inputSize = hdr.inputSize;

    if (!read_vector(in, out.ftBias, layerSize)) {
        return false;
    }
    if (!read_vector(in, out.ftWeights, layerSize * inputSize)) {
        return false;
    }

    // Remaining data in file may or may not include additional layers. We try to
    // read them but tolerate absent data.
    auto read_optional = [&](auto &vec, std::size_t count) {
        auto pos = in.tellg();
        if (!read_vector(in, vec, count)) {
            in.clear();
            in.seekg(pos, std::ios::beg);
            vec.clear();
        }
    };

    read_optional(out.l1Bias, 32);
    read_optional(out.l1Weights, layerSize * 32);
    read_optional(out.outBias, hdr.output == 0 ? 1 : hdr.output);
    read_optional(out.outWeights, layerSize);

    std::ostringstream oss;
    oss << "NNUE evaluation using " << path << " (input=" << hdr.inputSize << ", layer="
        << hdr.layerSize << ", output=" << hdr.output << ")";
    metaInfo = oss.str();

    return true;
}

} // namespace nnue
