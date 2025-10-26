# UCI Essential Options (lightweight)

This module provides a minimal, header-only UCI options system for SirioC:
types (check/spin/string/combo/button), defaults, pretty-printers for the
`uci` response, and a parser for `setoption`.

## Integration (2 lines)
1) Include the header in your UCI server file (where you parse "uci" and "setoption"):

```cpp
#include "sirio/uci_options.hpp"
```

2) Create the option set once, register essentials, and wire 2 handlers:

```cpp
static sirio::uci::OptionsMap Options;
static bool options_init = false;

// on "uci"
if (!options_init) { sirio::uci::register_essential_options(Options); options_init = true; }
sirio::uci::print_uci_options(std::cout, Options);
std::cout << "uciok\n";

// on "setoption name <Name> value <Value...>"
sirio::uci::handle_setoption(Options, full_line_after_keyword_setoption);
```

> You can later attach callbacks to Options["..."].after_set(...) to apply
> changes on-the-fly (resize TT, change threads, reload NNUE, etc.).

## Provided options and defaults
- Threads (spin 1..1024, default 1)
- Hash (spin 1..33554432 MB, default 64)
- Clear Hash (button)
- Ponder (check false)
- MultiPV (spin 1..256, default 1)
- UCI_Chess960 (check false)
- UCI_ShowWDL (check false)
- Move Overhead (spin 0..5000, default 250)
- Minimum Thinking Time (spin 0..2000, default 25)
- Slow Mover (spin 10..500, default 98)
- UCI_LimitStrength (check false)
- UCI_Elo (spin 1320..3190, default 3190)
- Debug Log File (string "")
- EvalFile (string "nn-000000000000.nnue")
- SyzygyPath (string "")
- SyzygyProbeDepth (spin 0..128, default 1)
- Syzygy50MoveRule (check true)
- NumaPolicy (combo: auto, interleave, compact, numa0, numa1; default auto)

## Reading values in your engine
```cpp
int threads = int(Options["Threads"]);
int hashMB  = int(Options["Hash"]);
bool ponder = bool(Options["Ponder"]);
std::string nn = std::string(Options["EvalFile"]);
```

## Attaching callbacks (examples)
```cpp
Options["Hash"].after_set([](const sirio::uci::Option& o){
    // TT.resize(int(o));   // your code here
});
Options["Threads"].after_set([](const sirio::uci::Option& o){
    // Threads.set(int(o)); // your code here
});
Options["Clear Hash"].after_set([](const sirio::uci::Option&){
    // TT.clear();          // your code here
});
Options["EvalFile"].after_set([](const sirio::uci::Option& o){
    // Eval::load(std::string(o)); // your code here
});
```

## Printing WDL
If you support WDL in `info`, simply respect the boolean from:
```cpp
if (bool(Options["UCI_ShowWDL"])) { /* emit wdl W D L */ }
```

## License
Placed under the same license as SirioC repository (inherit).
