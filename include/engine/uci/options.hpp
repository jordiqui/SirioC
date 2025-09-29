#pragma once

#include <string>

namespace engine {

struct EngineOptions {
    int         Hash         = 16;
    int         Threads      = 1;
    bool        Ponder       = false;
    bool        UCI_ShowWDL  = true;
    bool        UCI_Chess960 = false;
    int         MoveOverhead = 50;
    int         Contempt     = 0;
    int         MultiPV      = 1;
    std::string SyzygyPath;

    // === NNUE ===
    bool        UseNNUE       = true;
    std::string EvalFile;
    std::string EvalFileSmall;
};

extern EngineOptions Options;

} // namespace engine

