# SirioC

<p align="center">
  <img src="docs/logo.svg" alt="Logo minimalista de SirioC" width="180" />
</p>

SirioC es un proyecto de motor de ajedrez en C++ inspirado en la guía de Rustic Chess. El objetivo es construir una base clara y modular que cubra la representación del tablero, la carga de posiciones en notación FEN y utilidades básicas para futuros módulos como la generación de movimientos y la evaluación.

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

## Próximos pasos sugeridos

- Implementar un modo de análisis persistente que conserve la tabla de transposición entre sesiones y exponga controles UCI específicos.
- Construir un libro de aperturas en formato Polyglot a partir de colecciones PGN y añadir utilidades CLI para mantenerlo.
- Automatizar matches de regresión (incluidos tests `perft`) dentro del CI para vigilar la fuerza táctica del motor.

