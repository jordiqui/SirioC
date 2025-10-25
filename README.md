# SirioC

SirioC es un proyecto de motor de ajedrez en C++ inspirado en la guía de Rustic Chess. El objetivo es construir una base clara y modular que cubra la representación del tablero, la carga de posiciones en notación FEN y utilidades básicas para futuros módulos como la generación de movimientos y la evaluación.

## Características actuales

- Representación del tablero basada en bitboards.
- Carga y serialización de posiciones FEN, incluida la gestión de derechos de enroque y casillas de captura al paso.
- Detección de casillas atacadas para piezas deslizantes, caballos, peones y reyes.
- Ejecutable de ejemplo que muestra un tablero ASCII y acepta una posición en FEN desde la línea de comandos.
- Conjunto de pruebas unitarias sencillas para validar la inicialización, la compatibilidad FEN y la detección de ataques.

## Requisitos

- CMake 3.16 o superior.
- Un compilador con soporte completo de C++20 (por ejemplo, GCC 11+, Clang 12+, MSVC 2019 16.10+).

## Compilación

```bash
cmake -S . -B build
cmake --build build
```

El ejecutable `sirio` imprime el tablero de la posición inicial, o bien de la posición indicada como argumentos en FEN:

```bash
./build/sirio "8/8/8/3k4/4R3/8/8/4K3 w - - 0 1"
```

Para ejecutar las pruebas:

```bash
ctest --test-dir build
```

## Próximos pasos sugeridos

- Implementar un generador de movimientos completo utilizando la representación bitboard existente.
- Añadir evaluación estática y búsqueda minimax/alpha-beta.
- Integrar una interfaz UCI para conectarse con GUIs de ajedrez.
- Incorporar tablas de transposición y otras optimizaciones avanzadas conforme al plan de Rustic Chess.

