#include "nnue_loader.h"

#include "../eval.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

namespace nnue {
namespace {

struct FileHeader {
    char magic[8];
    std::uint32_t version;
    std::uint32_t feature_count;
    std::uint32_t hidden_size;
    std::uint32_t output_scale;
};

constexpr char kExpectedMagic[] = {'S', 'I', 'R', 'I', 'O', 'N', 'N', 'U'};
constexpr std::uint32_t kExpectedVersion = 1U;

bool validate_header(const FileHeader& header) {
    if (std::memcmp(header.magic, kExpectedMagic, sizeof(kExpectedMagic)) != 0) {
        return false;
    }
    if (header.version != kExpectedVersion) {
        return false;
    }
    if (header.feature_count == 0 || header.hidden_size == 0 || header.output_scale == 0) {
        return false;
    }
    return true;
}

std::size_t payload_size(const FileHeader& header) {
    const std::size_t bias_size = static_cast<std::size_t>(header.hidden_size) * sizeof(std::int16_t);
    const std::size_t weight_size = static_cast<std::size_t>(header.feature_count) *
                                    static_cast<std::size_t>(header.hidden_size) * sizeof(std::int8_t);
    const std::size_t output_weights = static_cast<std::size_t>(header.hidden_size) * sizeof(std::int16_t);
    return bias_size + weight_size + sizeof(std::int32_t) + output_weights;
}

bool parse_metadata(const std::uint8_t* data, std::size_t size, const std::string& source, Metadata& meta) {
    if (!data || size < sizeof(FileHeader)) {
        return false;
    }

    FileHeader header{};
    std::memcpy(&header, data, sizeof(FileHeader));
    if (!validate_header(header)) {
        return false;
    }

    const std::size_t required = sizeof(FileHeader) + payload_size(header);
    if (size < required) {
        return false;
    }

    Metadata result;
    result.architecture = "HalfKP";
    result.feature_count = header.feature_count;
    result.hidden_size = header.hidden_size;
    result.output_scale = header.output_scale;
    result.size_bytes = size;
    result.source = source;

    std::ostringstream dims;
    dims << '(' << header.feature_count << ", " << header.hidden_size << ", 1, " << header.output_scale << ')';
    result.dimensions = dims.str();

    meta = std::move(result);
    return true;
}

std::string canonical_source_path(const std::string& path) {
    std::error_code ec;
    auto absolute = std::filesystem::absolute(path, ec);
    if (!ec) {
        return absolute.generic_string();
    }
    return path;
}

#ifdef SIRIOC_EMBED_NNUE
extern "C" {
extern const unsigned char g_sirio_nnue_default[];
extern const std::size_t g_sirio_nnue_default_size;
}
#endif

}  // namespace

namespace loader {

bool load_from_memory(const MemoryResource& resource, Metadata& meta) {
    if (!resource.data || resource.size == 0) {
        return false;
    }

    Metadata parsed;
    if (!parse_metadata(resource.data, resource.size, resource.source, parsed)) {
        return false;
    }

    if (!eval_load_network_from_buffer(resource.data, resource.size)) {
        return false;
    }

    meta = std::move(parsed);
    return true;
}

bool load_from_file(const std::string& path, Metadata& meta) {
    if (path.empty()) {
        return false;
    }

    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return false;
    }

    std::vector<std::uint8_t> buffer((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    stream.close();
    if (buffer.empty()) {
        return false;
    }

    Metadata parsed;
    if (!parse_metadata(buffer.data(), buffer.size(), canonical_source_path(path), parsed)) {
        return false;
    }

    if (!eval_load_network_from_buffer(buffer.data(), buffer.size())) {
        return false;
    }

    meta = std::move(parsed);
    return true;
}

bool has_embedded_default() {
#ifdef SIRIOC_EMBED_NNUE
    return g_sirio_nnue_default_size > 0;
#else
    return false;
#endif
}

bool load_default(Metadata& meta) {
#ifdef SIRIOC_EMBED_NNUE
    MemoryResource resource{g_sirio_nnue_default, g_sirio_nnue_default_size, "embedded (default)"};
    return load_from_memory(resource, meta);
#else
    (void)meta;
    return false;
#endif
}

}  // namespace loader

}  // namespace nnue

