# SirioC

SirioC es un proyecto de motor de ajedrez en C++ inspirado en la guía de Rustic Chess. El objetivo es construir una base clara y modular que cubra la representación del tablero, la carga de posiciones en notación FEN y utilidades básicas para futuros módulos como la generación de movimientos y la evaluación.

## Características actuales

- Representación del tablero basada en bitboards.
- Carga y serialización de posiciones FEN, incluida la gestión de derechos de enroque y casillas de captura al paso.
- Generación de movimientos pseudo-legales y legales usando ataques por bitboards, incluida la detección de jaques, enroques y capturas al paso.
- Evaluación estática enriquecida con estructura de peones, seguridad del rey, movilidad y actividad de piezas menores, además de heurísticas especializadas para finales conocidos.
- Búsqueda `negamax` con poda alfa-beta, quiescence search, tabla de transposición ampliada, reducciones de movimientos tardíos, poda de movimiento nulo y *aspiration windows*.
- Búsqueda iterativa con control de tiempo adaptativo, cálculo de nodos visitados y soporte para opciones UCI como `SyzygyPath`.
- Integración opcional con tablebases Syzygy (3-7 piezas) mediante el motor Fathom.
- Interfaz UCI monohilo lista para conectarse con GUIs de ajedrez.
- Suite de pruebas unitarias que cubre inicialización del tablero, compatibilidad FEN, reglas de tablas, heurísticas de evaluación y nuevas utilidades como el movimiento nulo.
- Benchmarks reproducibles para medir nodos por segundo, precisión táctica y verificación opcional de tablebases.

## Documentación

- [Manejo de cadenas FEN](docs/fen.md)
- [Búsqueda y ordenación de movimientos](docs/search.md)
- [Comunicación UCI](docs/communication.md)

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

El motor permite configurar la ruta a las tablebases mediante UCI:

```
setoption name SyzygyPath value /ruta/a/tablebases
```

El directorio debe contener los archivos Syzygy de 3 a 7 piezas (`*.rtbw`, `*.rtbz`). Para disponer del conjunto completo de 7 piezas se requieren aproximadamente 150 GB de almacenamiento. Si no se establece la ruta o los archivos no están disponibles el motor continúa funcionando con su evaluación interna.

La integración utiliza la biblioteca Fathom (licencia MIT) incluida en `third_party/fathom`. Consulte `third_party/fathom/LICENSE` para los términos completos.

## Próximos pasos sugeridos

- Paralelizar la búsqueda (por ejemplo, mediante *lazy SMP* o trabajo en *split points*).
- Persistir la tabla de transposición para sesiones largas y añadir libro de aperturas.
- Integrar suites tácticas más extensas (LCT-II, WAC) y benchmarks automáticos en CI.

