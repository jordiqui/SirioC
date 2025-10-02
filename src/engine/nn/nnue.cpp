#include "nnue.h"

#include "nnue_loader.h"
#include "../eval/eval.h"

#include <cstdint>
#include <utility>

namespace nnue {

State state;

namespace {

void assign_state(Metadata&& meta) {
    state.loaded = true;
    state.meta = std::move(meta);
    eval_set_use_nnue(true);
}

}  // namespace

void reset() {
    state.loaded = false;
    state.meta = Metadata{};
    eval_set_use_nnue(false);
}

bool load(const std::string& filePath) {
    Metadata meta;
    if (!loader::load_from_file(filePath, meta)) {
        return false;
    }
    assign_state(std::move(meta));
    return true;
}

bool load_from_memory(const void* data, std::size_t size, const std::string& source) {
    if (!data || size == 0) {
        return false;
    }
    MemoryResource resource{
        static_cast<const std::uint8_t*>(data),
        size,
        source,
        Metadata::SourceType::Memory};
    Metadata meta;
    if (!loader::load_from_memory(resource, meta)) {
        return false;
    }
    assign_state(std::move(meta));
    return true;
}

bool load_default() {
    Metadata meta;
    if (!loader::load_default(meta)) {
        return false;
    }
    assign_state(std::move(meta));
    return true;
}

bool has_embedded_default() {
    return loader::has_embedded_default();
}

}  // namespace nnue

