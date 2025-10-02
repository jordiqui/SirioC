#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace nnue {

struct Metadata {
    enum class SourceType { Unknown, File, Embedded, Memory };

    std::string architecture;
    std::string dimensions;
    std::string source;
    std::size_t size_bytes = 0;
    std::uint32_t feature_count = 0;
    std::uint32_t hidden_size = 0;
    std::uint32_t output_scale = 0;
    SourceType source_type = SourceType::Unknown;
};

struct MemoryResource {
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
    std::string source;
    Metadata::SourceType source_type = Metadata::SourceType::Memory;
};

namespace loader {

bool load_from_file(const std::string& path, Metadata& meta);
bool load_from_memory(const MemoryResource& resource, Metadata& meta);
bool load_default(Metadata& meta);
bool has_embedded_default();

}  // namespace loader

}  // namespace nnue

