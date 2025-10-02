#pragma once

#include "nnue_loader.h"

#include <cstddef>
#include <string>

namespace nnue {

struct State {
    bool loaded = false;
    Metadata meta;
};

extern State state;

bool load(const std::string& filePath);
bool load_from_memory(const void* data, std::size_t size, const std::string& source);
bool load_default();
bool has_embedded_default();
void reset();

}  // namespace nnue

