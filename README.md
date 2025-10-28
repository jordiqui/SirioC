# SirioC

<p align="center">
  <img src="docs/logo.svg" alt="Logo minimalista de SirioC" width="180" />
</p>

SirioC es un proyecto de motor de ajedrez en C++ inspirado en la guía de Rustic Chess. El objetivo es construir una base clara y modular que cubra la representación del tablero, la carga de posiciones en notación FEN y utilidades básicas para futuros módulos como la generación de movimientos y la evaluación.

Este es un motor original creado por **Jorge Ruiz Centelles**. El núcleo del motor, su interfaz UCI y las herramientas de prueba están desarrolladas íntegramente en **C++20**, con archivos de configuración en CMake y Makefile para facilitar la compilación en distintas plataformas. El proyecto se distribuye bajo licencia MIT, por lo que cualquier redistribución debe conservar este aviso de autoría y los términos de la licencia.

## Características actuales

- Representación del tablero basada en bitboards.
- Carga y serialización de posiciones FEN, incluida la gestión de derechos de enroque y casillas de captura al paso.
- Generación de movimientos pseudo-legales y legales usando ataques por bitboards, incluida la detección de jaques, enroques y capturas al paso.
- Evaluación estática enriquecida con estructura de peones, seguridad del rey, movilidad y actividad de piezas menores, además de heurísticas especializadas para finales conocidos.
- Búsqueda `negamax` con poda alfa-beta, quiescence search, tabla de transposición ampliada, reducciones de movimientos tardíos, poda de movimiento nulo y *aspiration windows*.
- Búsqueda iterativa con control de tiempo adaptativo, cálculo de nodos visitados y soporte para opciones UCI como `SyzygyPath`.
- Integración opcional con tablebases Syzygy (3-7 piezas) mediante el motor Fathom.
- Interfaz UCI multihilo con búsqueda *lazy SMP* configurable.
- Suite de pruebas unitarias que cubre inicialización del tablero, compatibilidad FEN, reglas de tablas, heurísticas de evaluación y nuevas utilidades como el movimiento nulo.
- Benchmarks reproducibles para medir nodos por segundo, precisión táctica y verificación opcional de tablebases.
- Evaluación NNUE configurable con uno o dos ficheros, permitiendo redes especializadas para medio juego y finales.
- Modo de análisis persistente que guarda automáticamente la tabla de transposición entre sesiones y ofrece controles UCI para activar, guardar, cargar o limpiar el fichero asociado.

## Documentación

- [Manejo de cadenas FEN](docs/fen.md)
- [Búsqueda y ordenación de movimientos](docs/search.md)
- [Comunicación UCI](docs/communication.md)
- [Integración con GUIs UCI](docs/gui.md)

## Settings

The Arena/Fritz configuration dialog exposes the following UCI options. The
descriptions below explain what each control does and highlight the recommended
values for common time controls:

- **Bullet** (≤ 2+1)
- **Blitz** (3+2 to 5+3)
- **Rapid** (10+0 to 25+10)
- **Classical** (≥ 45+15)

### Core performance

#### Threads
Determines how many CPU threads the search will use. SirioC scales best with the
number of physical cores available; hyper-threads provide diminishing returns.
By default the engine auto-detects the host CPU and selects as many threads as
`std::thread::hardware_concurrency()` reports. You can override this before
launching the GUI by setting the environment variable `SIRIOC_THREADS` to a
positive integer, which is useful on shared workstations. **Recommended
values:** Bullet—use 75% of your physical cores to keep the GUI
responsive, Blitz/Rapid/Classical—use all physical cores or all but one if the
machine is shared.

#### Hash
Sets the size of the transposition table in megabytes. Larger tables allow the
engine to reuse more positions between searches but consume more RAM.
**Recommended values:** Bullet—64 to 128 MB, Blitz—256 MB, Rapid—512 MB, and
Classical—1024 MB or the largest size that keeps free memory for the GUI.

#### Clear Hash (button)
Immediately empties the transposition table. Use it before starting new test
matches or when switching between very different positions. No time-control
dependency: press as needed.

#### HashFile / HashPersist
`HashFile` points to a file used to persist the transposition table across
sessions when `HashPersist` is enabled. Persistent hash only helps in long-term
analysis; leave `HashPersist` disabled for all competitive time controls and
enable it with a writable path only for offline studies.

#### NumaPolicy
Controls how SirioC pins memory on NUMA systems. `auto` lets the engine choose.
Only adjust this on workstations with multiple CPU sockets. All time controls
should keep the default unless profiling shows imbalance.

### Search and analysis output

#### Analysis Lines
Specifies how many principal variations the engine reports. Extra lines slow the
search because SirioC must keep multiple best lines.
**Recommended values:** Bullet/Blitz—1 line to focus on the best move,
Rapid—2 lines when analysing, Classical—2 to 3 lines if you need deeper
explanations; keep 1 when playing a tournament game.

#### MultiPV
Determines how many best moves the engine tracks internally. Increasing it has
the same cost considerations as `Analysis Lines`.
**Recommended values:** Bullet/Blitz—1, Rapid—2 when analysing, Classical—2 to 4
for study sessions; return to 1 for competitive play.

#### UCI_AnalyseMode
Fritz y otras GUIs activan esta bandera cuando entran en modo de análisis
permanente. SirioC acepta el ajuste para mantener la compatibilidad con estas
interfaces (actualmente no modifica el algoritmo de búsqueda, pero evita
advertencias y restablece el valor anunciado por la GUI).

#### Skill Level
Artificially limits engine strength by capping search depth and randomness.
Leave it at 20 (maximum strength) for all time controls unless you deliberately
need a weaker sparring partner.

#### nodestime
Caps the number of nodes searched per millisecond. Leave it at 0 so SirioC uses
its adaptive time management at every time control.

#### Debug Log File
If set, SirioC writes an execution log to the given path. Logging incurs slight
overhead, so keep it empty for Bullet/Blitz. For Rapid/Classical analysis
sessions you may provide a path when you need diagnostics.

### Time management

#### Ponder
Allows the engine to think on the opponent's time. **Recommended values:**
Bullet—off (it can interfere with fast move input), Blitz—off unless you run on
a dedicated machine, Rapid/Classical—on for maximum strength when background
thinking is acceptable.

#### Move Overhead
Reserves extra milliseconds per move to account for GUI latency. **Recommended
values:** Bullet—250 to 300 ms, Blitz—150 ms, Rapid—100 ms, Classical—50 ms.

#### Minimum Thinking Time
Defines a floor on how little time the engine uses even when ahead on the clock.
**Recommended values:** Bullet—20 ms, Blitz—50 ms, Rapid—100 ms,
Classical—150 ms.

#### Slow Mover
Scales how aggressively the search spends time. Lower numbers make the engine
play faster. **Recommended values:** Bullet—80, Blitz—90, Rapid—100,
Classical—110.

### Opening book and NNUE

#### UseBook / BookFile
`UseBook` enables the external opening book located at `BookFile`. Books provide
instant moves and conserve time. **Recommended values:** enable the book for
Bullet/Blitz/Rapid to avoid early clock usage, and enable it for Classical when
you trust the book contents; disable if you prefer to test pure engine play.

#### UseNNUE / EvalFile / EvalFileSmall / NNUEFile
`UseNNUE` toggles the neural evaluation backend. `EvalFile` and `NNUEFile`
should point to the main network (`nn-1c0000000000.nnue` by default) and
`EvalFileSmall` to a fallback network for low-memory systems. NNUE evaluation
produces stronger play across all time controls, so keep `UseNNUE` enabled with
valid paths for Bullet through Classical. Only disable NNUE when benchmarking
the handcrafted evaluation.

### Syzygy tablebases

#### SyzygyPath
Directory containing Syzygy tablebase files. Configure it whenever you have the
bases locally; they instantly solve many endings and save time. Applies equally
to all time controls.

#### SyzygyProbeDepth / SyzygyProbeLimit
`SyzygyProbeDepth` determines how deep the search waits before probing the
bases, while `SyzygyProbeLimit` limits the number of pieces probed. **Recommended
values:** Bullet—depth 2 and limit 5 to avoid latency, Blitz—depth 4 limit 6,
Rapid—depth 6 limit 7, Classical—depth 8 limit 7 for maximum precision.

#### Syzygy50MoveRule
When true, SirioC honours the 50-move rule while probing tablebases. Keep it
enabled for all official games regardless of time control.

### Persistent analysis tools

#### PersistentAnalysis
Stores and reuses the transposition table between searches while analysing a
single position deeply. Disable it for competitive play at every time control;
enable it only for long manual studies.

#### PersistentAnalysisFile
File path used by the persistent analysis feature. Provide a writable location
when `PersistentAnalysis` is enabled; leave empty otherwise.

#### PersistentAnalysisLoad / Save / Clear (buttons)
`Load` restores a saved analysis table from `PersistentAnalysisFile`, `Save`
writes the current table, and `Clear` deletes the stored data. Invoke these
manually during deep analysis sessions; they are not part of routine timed play.

### Strength limiting and variants

#### UCI_LimitStrength / UCI_Elo
These controls emulate a weaker playing strength by targeting a specific ELO
rating. Keep `UCI_LimitStrength` off and `UCI_Elo` at its default (3190) for all
serious games.

#### UCI_Chess960
Enables Chess960 castling rules. Activate it only when the event is in Chess960
format; leave it off otherwise.

#### UCI_ShowWDL
Requests win/draw/loss statistics in analysis output. The extra formatting has
minimal impact, so feel free to keep it on for Rapid/Classical analysis. Disable
it for Bullet/Blitz if you want the leanest info stream.

## Requisitos

- CMake 3.16 o superior.
- Un compilador con soporte completo de C++20 (por ejemplo, GCC 11+, Clang 12+, MSVC 2019 16.10+).

## Compilación

```bash
cmake -S . -B build
cmake --build build
```

## Compilación con Makefile

```bash
make
```

El ejecutable quedará disponible en `build/bin/sirio`.

Para compilar y ejecutar las pruebas con el Makefile:

```bash
make test
```

Esto genera el binario de pruebas en `build/bin/sirio_tests` y lo ejecuta.

El ejecutable `sirio` imprime el tablero de la posición inicial, o bien de la posición indicada como argumentos en FEN:

```bash
./build/sirio "8/8/8/3k4/4R3/8/8/4K3 w - - 0 1"
```

Para ejecutar las pruebas:

```bash
ctest --test-dir build
```

## Benchmarks

Los benchmarks se pueden compilar con CMake (`sirio_bench`) o mediante el Makefile:

```bash
make bench
```

El ejecutable imprime tres métricas:

- Nodos por segundo sobre un conjunto fijo de posiciones de velocidad.
- Aciertos en una pequeña suite táctica de mates inmediatos.
- Resultado de una sonda Syzygy (si las tablebases están disponibles); en caso contrario muestra instrucciones para configurarlas.

## Tablebases Syzygy

El motor configura automáticamente la ruta a las tablebases buscando un
directorio `tablebases/` junto al ejecutable (o en su directorio padre) y
respeta la variable de entorno `SIRIO_SYZYGY_PATH` si está definida. Este
comportamiento proporciona una configuración lista para usar: basta con copiar
los archivos Syzygy dentro de `tablebases/` para que estén disponibles desde el
primer arranque.

También es posible ajustar la ruta manualmente mediante UCI:

```
setoption name SyzygyPath value /ruta/a/tablebases
```

El directorio debe contener los archivos Syzygy de 3 a 7 piezas (`*.rtbw`, `*.rtbz`). Para disponer del conjunto completo de 7 piezas se requieren aproximadamente 150 GB de almacenamiento. Si no se establece la ruta o los archivos no están disponibles el motor continúa funcionando con su evaluación interna.

La integración utiliza la biblioteca Fathom (licencia MIT) incluida en `third_party/fathom`. Consulte `third_party/fathom/LICENSE` para los términos completos.

## Evaluación NNUE

SirioC incluye un backend NNUE opcional. Por defecto se usa la evaluación clásica, pero es
posible cargar una red neuronal mediante las opciones estándar `EvalFile` y `EvalFileSmall`.

- `EvalFile` apunta a la red principal que se utilizará siempre que esté disponible.
- `EvalFileSmall` permite registrar una red alternativa (por ejemplo, versiones "small" para
  hardware con menos memoria). Si `EvalFile` no se puede cargar, la GUI intentará esta ruta al
  enviar `isready`.

Se recomienda utilizar las redes oficiales más recientes (`nn-1c0000000000.nnue`, ~45 MiB) como
principal y `nn-37f18f62d772.nnue` como alternativa "small". Ambas se publican por el proyecto
Stockfish y son compatibles con SirioC:

```
setoption name EvalFile value /ruta/a/nn-1c0000000000.nnue
setoption name EvalFileSmall value /ruta/a/nn-37f18f62d772.nnue
```

Tras una carga correcta el motor informará con `info string NNUE evaluation using ...` que incluye
la ruta y el tamaño aproximado del archivo. Para desactivar el backend NNUE basta con limpiar la
opción:

```
setoption name EvalFile value <empty>
```

Ambas redes se distribuyen bajo la licencia GNU GPLv3 del proyecto Stockfish. SirioC no incluye
copias de dichos archivos, pero al utilizarlas debes respetar sus términos (conservar avisos de
copyright, ofrecer el código fuente del generador de la red si redistribuyes binarios, etc.). Para
quienes necesiten una alternativa completamente propia, se puede seguir utilizando la evaluación
clásica integrada.

### Descarga de redes y configuración en GUIs

Las redes NNUE oficiales están publicadas en el repositorio de Stockfish. Puedes obtener la red
principal y la variante "small" directamente desde los servidores de pruebas del proyecto:

```bash
curl -L -o nn-1c0000000000.nnue \
  https://tests.stockfishchess.org/api/nn/nn-1c0000000000.nnue
curl -L -o nn-37f18f62d772.nnue \
  https://tests.stockfishchess.org/api/nn/nn-37f18f62d772.nnue
```

o, si lo prefieres, con `wget`:

```bash
wget https://tests.stockfishchess.org/api/nn/nn-1c0000000000.nnue
wget https://tests.stockfishchess.org/api/nn/nn-37f18f62d772.nnue
```

No es obligatorio colocar las redes en el mismo directorio que el binario del motor, pero resulta
práctico hacerlo para que la GUI construya rutas relativas de forma automática. En Windows, por
ejemplo, puedes crear `C:\ChessEngines\SirioC` y copiar allí `sirio.exe` junto con los archivos
`nn-1c0000000000.nnue` y `nn-37f18f62d772.nnue`. Después, desde Fritz 20:

1. Ve a **Archivo → Opciones → Motores → Crear UCI** y selecciona `sirio.exe`.
2. Abre el diálogo de opciones del motor recién agregado y localiza el campo `EvalFile`.
3. Pulsa **Examinar…** y apunta a `nn-1c0000000000.nnue`. Si deseas registrar la red alternativa
   pequeña, repite el proceso con `EvalFileSmall` y selecciona `nn-37f18f62d772.nnue`.

SirioC anuncia estas dos rutas como valores por defecto (`EvalFile` = `nn-1c0000000000.nnue` y
`EvalFileSmall` = `nn-37f18f62d772.nnue`). Si los archivos están en el mismo directorio que el
ejecutable, muchas GUIs (incluida Fritz 20) pre rellenarán los campos con esos nombres, por lo que
solo tendrás que confirmar la selección.
4. Confirma con **Aceptar** y utiliza el motor con normalidad.

El resto de GUIs (Cute Chess, Arena, etc.) también mostrarán `EvalFile` y `EvalFileSmall` dentro de
las opciones UCI; basta con indicar la ruta absoluta o relativa a las redes descargadas. Si no se
configura ninguna ruta el motor seguirá funcionando con su evaluación clásica.

### Uso de redes NNUE entrenadas internamente

Si cuentas con una suite propia para entrenar redes NNUE, puedes integrarlas en SirioC siguiendo
los mismos mecanismos que emplea el proyecto para redes oficiales:

1. **Exporta la red al formato `.nnue`.** Herramientas como `nnue-pytorch` o `nnue-training` de
   Stockfish generan archivos compatibles con la implementación incluida en `src/nnue`.
2. **Ubica los modelos en tu suite de pruebas.** Mantenerlos junto al binario (`build/bin/sirio`)
   o en un directorio versionado facilita automatizar entrenamientos y validaciones.
3. **Configura las rutas desde UCI.** Lanza el motor y registra la red principal con
   `setoption name EvalFile value /ruta/a/tu_red.nnue`. Si tu flujo separa redes de medio juego y
   finales, aprovecha `EvalFileSmall` para proporcionar el modelo alternativo.
4. **Verifica la carga.** Ejecuta `make bench` o `./build/bin/sirio_bench` para confirmar que el
   motor informa `info string NNUE evaluation using ...` con tu archivo y que el rendimiento se
   alinea con los datos de la suite.
5. **Integra la red en tus matches.** Una vez validada, reutiliza los mismos archivos en tus
   enfrentamientos automáticos (`cutechess-cli`, `banksia`, etc.) o en las GUIs que utilices.

Este flujo mantiene separada la fase de entrenamiento de la de integración, permitiendo iterar
rápidamente sobre arquitecturas o datasets propios antes de promover una red al motor principal.

## Próximos pasos sugeridos

- Consolidar una **pipeline de entrenamiento NNUE propia**, definiendo datasets, scripts de entrenamiento y métricas de validación reproducibles.
- La suite de benchmarks incluye comparativas entre redes internas y referencias públicas gracias a la carga automatizada de `EvalFile` y `EvalFileSmall` en `bench/ccrl_suite.py`.
- Incorporar un orquestador de matches (`cutechess-cli`, `fastchess`) que ejecute regresiones periódicas con las redes validadas y actualice automáticamente su despliegue en el motor.

