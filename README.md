# SirioC

SirioC es un proyecto de motor de ajedrez en C++ inspirado en la guía de Rustic Chess. El objetivo es construir una base clara y modular que cubra la representación del tablero, la carga de posiciones en notación FEN y utilidades básicas para futuros módulos como la generación de movimientos y la evaluación.

## Características actuales

- Representación del tablero basada en bitboards.
- Carga y serialización de posiciones FEN, incluida la gestión de derechos de enroque y casillas de captura al paso.
- Generación de movimientos pseudo-legales y legales usando ataques por bitboards, incluida la detección de jaques, enroques y capturas al paso.
- Evaluación estática con conteo de material, tablas pieza-casilla y bonificación por pareja de alfiles.
- Búsqueda `negamax` con poda alfa-beta, quiescence search, heurísticas de ordenación y tabla de transposición.
- Interfaz UCI monohilo lista para conectarse con GUIs de ajedrez.
- Conjunto de pruebas unitarias sencillas para validar la inicialización, la compatibilidad FEN y la detección de ataques.

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

## Próximos pasos sugeridos

- Añadir gestión de tiempo y búsqueda iterativa para soportar partidas competitivas.
- Incorporar mejoras de evaluación específicas para finales (por ejemplo, tablas de distancias rey-rey).
- Implementar búsqueda con extensiones y reducciones selectivas siguiendo el plan de Rustic Chess.

