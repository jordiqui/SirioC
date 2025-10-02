#include "nnue_loader.h"

#include "../eval/eval.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <system_error>
#include <utility>
#include <vector>

namespace nnue {
namespace {

constexpr std::size_t kMinimumReasonableSize = 1u << 20;  // 1 MiB

bool validate_buffer_size(std::size_t size) {
    return size > kMinimumReasonableSize;
}

bool parse_metadata(const std::uint8_t* data, std::size_t size, const std::string& source, Metadata& meta) {
    if (!data || !validate_buffer_size(size)) {
        return false;
    }

    Metadata result;
    result.architecture = "NNUE";
    result.size_bytes = size;
    result.source = source;

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

void emit_success_info(const Metadata& meta) {
    std::cout << "info string NNUE network loaded: " << meta.architecture;
    if (!meta.dimensions.empty()) {
        std::cout << ' ' << meta.dimensions;
    }
    if (meta.size_bytes > 0) {
        std::cout << " (" << meta.size_bytes << " bytes)";
    }
    std::cout << "\n";
    if (!meta.source.empty()) {
        std::cout << "info string NNUE source: " << meta.source << "\n";
    }
}

void emit_failure_info(const std::string& source) {
    std::cout << "info string NNUE load failed from " << source << "\n";
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
        emit_failure_info(resource.source.empty() ? "memory resource" : resource.source);
        return false;
    }

    parsed.source_type = resource.source_type;
    if (!eval_load_network_from_buffer(resource.data, resource.size)) {
        emit_failure_info(resource.source.empty() ? "memory resource" : resource.source);
        return false;
    }

    meta = std::move(parsed);
    emit_success_info(meta);
    return true;
}

bool load_from_file(const std::string& path, Metadata& meta) {
    if (path.empty()) {
        return false;
    }

    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        emit_failure_info(path);
        return false;
    }

    std::vector<std::uint8_t> buffer((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    stream.close();
    if (buffer.empty()) {
        emit_failure_info(path);
        return false;
    }

    Metadata parsed;
    const std::string canonical = canonical_source_path(path);
    if (!parse_metadata(buffer.data(), buffer.size(), canonical, parsed)) {
        emit_failure_info(path);
        return false;
    }

    parsed.source_type = Metadata::SourceType::File;
    if (!eval_load_network_from_buffer(buffer.data(), buffer.size())) {
        emit_failure_info(path);
        return false;
    }

    meta = std::move(parsed);
    emit_success_info(meta);
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
    MemoryResource resource{
        g_sirio_nnue_default,
        g_sirio_nnue_default_size,
        "embedded (default)",
        Metadata::SourceType::Embedded};
    return load_from_memory(resource, meta);
#else
    (void)meta;
    return false;
#endif
}

}  // namespace loader

}  // namespace nnue

