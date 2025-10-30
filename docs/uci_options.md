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
- Threads (spin 1..1024, default auto-detected)
- Hash (spin 1..33554432 MB, default 16)
- Clear Hash (button)
- Ponder (check false)
- MultiPV (spin 1..256, default 1)
- UCI_Chess960 (check false)
- UCI_ShowWDL (check false)
- Move Overhead (spin 0..5000, default 250)
- Minimum Thinking Time (spin 0..2000, default 25)
- Slow Mover (spin 10..500, default 98)
- UCI_LimitStrength (check false)
- UCI_AnalyseMode (check false)
- UCI_Elo (spin 1320..3190, default 3190)
- Debug Log File (string "")
- EvalFile (string "nn-1c0000000000.nnue")
- SyzygyPath (string "")
- SyzygyProbeDepth (spin 0..128, default 1)
- Syzygy50MoveRule (check true)
- NumaPolicy (combo: auto, interleave, compact, numa0, numa1; default auto)
- UseBook (check true)
- BookFile (string "")

`Threads` se inicializa con el valor devuelto por
`std::thread::hardware_concurrency()` cada vez que se arranca el motor. Define
la variable de entorno `SIRIOC_THREADS` antes de lanzar la GUI para forzar un
recuento distinto sin recompilar.

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

## NNUE options in SirioC

La implementación por defecto de SirioC expone las opciones `EvalFile` y `EvalFileSmall` para cargar
redes NNUE desde disco. La primera ruta es la preferida; la segunda se usa como respaldo durante
`isready` cuando la principal falla (por ejemplo, en máquinas con poca memoria). Ambas aceptan
`<empty>` para volver a la evaluación clásica. Las redes recomendadas actualmente son
`nn-1c0000000000.nnue` como principal y `nn-37f18f62d772.nnue` como alternativa, ambas basadas en
las versiones más recientes publicadas por el proyecto Stockfish. Recuerda cumplir con su licencia
si las redistribuyes junto a SirioC.

## Libro de aperturas

`UseBook` está activada por defecto. Si la opción `BookFile` queda vacía, el motor emitirá un mensaje
informativo al arrancar indicando que necesita la ruta del libro. Para aprovechar la apertura debes
descargar un fichero en formato Polyglot (`.bin` o `.obk`) y apuntar `BookFile` a su ubicación.

### UHO Big 8Mv5

1. Descarga el libro oficial desde el repositorio de UHO Stockfish: <https://github.com/UHO-Stockfish/Book/releases>.
   El archivo que necesitas se llama `UHO_Big_8Mv5.obk` (comprimido en ZIP en la publicación).
2. Crea el directorio `resources/books` en el mismo árbol que el ejecutable de SirioC y descomprime ahí el archivo.
   La ruta final debería quedar como `resources/books/UHO_Big_8Mv5.obk`.
3. En tu GUI UCI ejecuta `setoption name BookFile value resources/books/UHO_Big_8Mv5.obk`.

Si la ruta es correcta, el motor responderá con `info string Opening book loaded from ...`. Si el
archivo no existe o el formato es incorrecto, verás un mensaje explicando el motivo para que puedas
corregirlo.

## License
Placed under the same license as SirioC repository (inherit).
