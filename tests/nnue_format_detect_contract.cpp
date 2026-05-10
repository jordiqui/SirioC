#include <iostream>
#include <string>

#include "sirio/nnue/api.hpp"

namespace {

std::string format_name(sirio::nnue::NnueNetworkFormat format) {
    switch (format) {
    case sirio::nnue::NnueNetworkFormat::Unknown:
        return "Unknown";
    case sirio::nnue::NnueNetworkFormat::SirioNNUE1Legacy:
        return "SirioNNUE1Legacy";
    case sirio::nnue::NnueNetworkFormat::SirioNNUE2MinimalV1:
        return "SirioNNUE2MinimalV1";
    case sirio::nnue::NnueNetworkFormat::Malformed:
        return "Malformed";
    case sirio::nnue::NnueNetworkFormat::Unsupported:
        return "Unsupported";
    }
    return "Unknown";
}

std::string json_escape(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
        case '\\':
            out += "\\\\";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: sirio_nnue_format_detect_contract <network_path>\n";
        return 2;
    }

    const auto info = sirio::nnue::detect_nnue_network_format(argv[1]);
    std::cout << "{"
              << "\"path\":\"" << json_escape(info.path) << "\","
              << "\"format\":\"" << json_escape(format_name(info.format)) << "\","
              << "\"diagnostic\":\"" << json_escape(info.diagnostic) << "\""
              << "}\n";
    return 0;
}
