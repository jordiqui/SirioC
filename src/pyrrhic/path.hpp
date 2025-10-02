#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace pathutil {

std::filesystem::path first_existing(const std::vector<std::filesystem::path>& cands);

std::filesystem::path exe_dir();

}  // namespace pathutil

