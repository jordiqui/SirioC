# Evaluación de módulos externos

Esta nota resume la viabilidad de portar archivos de Stockfish —`timeman.*`, `tt.*`, `tune.*`, `types.h`, `uci.*` y `ucioption.*`— al árbol de SirioC. Se contrasta su funcionalidad esperada con el código existente.

## Resumen

| Módulo | Funcionalidad esperada | Cobertura actual en SirioC | Viabilidad | Comentarios clave |
| --- | --- | --- | --- | --- |
| `timeman.cpp/h` | Cálculo de límites de tiempo, *overhead*, *slow mover*, nodos/tiempo | Gestión dispersa entre `SearchLimits`, utilidades en `search.cpp` y opciones UCI en `main.cpp` | Media | Requiere extraer lógica de tiempo de `search.cpp` y reescribir el *plumbing* de opciones. |
| `tt.cpp/h` + "Secondary TT Aging" | Tabla de transposición compartida, generaciones y políticas de reemplazo | Implementación propia `GlobalTranspositionTable` embebida en `search.cpp` con serialización | Media | Hay equivalencia funcional, pero habría que trocear el fichero, mapear tipos y adaptar la persistencia actual. |
| `tune.cpp/h` | Infraestructura de *tuning* y guardas en macros `TUNE()` | No existe; solo hay *benchmarks* ad hoc | Baja | Introducirlo exige definir macros y rutas de build nuevas; útil solo si se añade tooling externo. |
| `types.h` | Definiciones básicas (Color, Piece, etc.) con `enum` compactos | Definidas en `board.hpp` y otros encabezados específicos | Media | Se puede factorizar, pero implica revisar include-guards y dependencias ciclicas. |
| `uci.cpp/h` | Bucle principal y manejo de comandos UCI | Toda la lógica vive en `main.cpp` (impresión de opciones, `setoption`, `go`, etc.) | Media-baja | Portar requiere reestructurar `main.cpp`, separar el hilo de búsqueda y ajustar puntos de entrada. |
| `ucioption.cpp/h` | Gestión tipada de opciones UCI (spin, check, combo) | Existe `include/sirio/uci_options.hpp` sin uso en binario principal | Media | Se puede reaprovechar concepto, pero habría que reconciliar nomenclaturas y callbacks con el flujo actual en `main.cpp`. |

## Detalles

### `timeman.cpp` y `timeman.h`
- SirioC ya expone `set_move_overhead`, `set_minimum_thinking_time`, `set_slow_mover` y `set_nodestime` como *atomics* globales dentro de `search.cpp`, más las consultas públicas en `search.hpp`. 【F:src/search.cpp†L363-L387】【F:include/sirio/search.hpp†L51-L58】
- La aplicación de estas opciones se hace desde `main.cpp` al parsear `setoption`, mezclando UCI con política de tiempo. 【F:src/main.cpp†L75-L113】【F:src/main.cpp†L424-L528】
- Stockfish encapsula heurísticas (p. ej. fracciones de tiempo según fase), algo que aquí está integrado en la búsqueda. Portar `timeman.*` implicaría:
  - Definir una interfaz limpia (probablemente en `include/sirio`) para calcular límites de tiempo a partir de `SearchLimits` y del estado del nodo.
  - Migrar la lógica de comprobación de tiempo que hoy se reparte entre `SearchSharedState` y el lazo de iteraciones en `search.cpp`. 【F:src/search.cpp†L295-L314】
- Conclusión: viable, pero requiere refactorizar cómo `search_best_move` calcula `soft_time_limit` y `hard_time_limit`.

### `tt.cpp`, `tt.h` y "Introduce Secondary TT Aging"
- Actualmente la tabla de transposición está implementada como `GlobalTranspositionTable` dentro de `search.cpp`, con empaquetado manual de `Move`, guardado/carga y *shards* de mutex. 【F:src/search.cpp†L58-L288】
- Hay API pública para redimensionar, limpiar y persistir la tabla expuesta vía `search.hpp`. 【F:src/search.cpp†L342-L360】【F:include/sirio/search.hpp†L45-L49】
- El parche de "Secondary TT Aging" de Stockfish añade contadores extra para caducar entradas; SirioC tendría que adaptar `PackedTTEntry` y la política de reemplazo (hoy reemplaza por profundidad/generación) y asegurarse de preservar la serialización binaria existente.
- Se recomienda, antes de portar, extraer la clase a `src/tt.cpp` + `include/sirio/tt.hpp`, lo que facilita mapear los cambios y minimizar diffs.

### `tune.cpp` y `tune.h`
- SirioC no tiene macros `TUNE()` ni build flags asociados. La única infraestructura relacionada son los *benchmarks* en `bench/bench.cpp`. 【F:bench/bench.cpp†L698-L759】
- Portar `tune.*` sin un pipeline de ajuste automatizado aportaría poco; además habría que introducir macros condicionados (p. ej. `#ifdef TUNE`) y evitar su uso accidental, como sugiere el commit "Prevent accidental misuse of TUNE()".
- Viabilidad baja salvo que el proyecto adopte flujo de tuning similar al de Stockfish.

### `types.h`
- Las definiciones de `Color`, `PieceType` y estructuras auxiliares residen en `board.hpp` junto con la lógica de tablero. 【F:include/sirio/board.hpp†L17-L141】
- Trasladarlas a un `types.h` separado permitiría reducir dependencias cruzadas (por ejemplo, `move.hpp` y `movegen.hpp` podrían incluir solo tipos). Sin embargo, exige revisar todos los `#include` y posibles dependencias cíclicas.
- Viable si se planifica una refactorización por fases: crear `include/sirio/types.hpp`, mover enums y actualizar includes, asegurándose de que no se rompan forward declarations.

### `uci.cpp` y `uci.h`
- El binario principal implementa todo el protocolo UCI en `main.cpp`: impresión de `option` en `send_uci_id`, parseo manual de `setoption`, control de `go` y *threading*. 【F:src/main.cpp†L277-L614】
- Portar la versión modular de Stockfish requeriría:
  - Extraer la lógica de opciones y del hilo de búsqueda a un nuevo módulo (`src/uci.cpp`).
  - Exponer una función `uci_loop()` que reemplace el `main` actual o reorganizar `main.cpp` para delegar.
  - Revisar cómo se gestionan dependencias con NNUE, Syzygy y apertura (hoy se inicializan directamente desde `main`).
- Viabilidad media-baja: posible pero con refactorización significativa del punto de entrada.

### `ucioption.cpp` y `ucioption.h`
- Existe `include/sirio/uci_options.hpp` que define un mini-framework de opciones (tipos `Spin`, `Check`, callbacks). 【F:include/sirio/uci_options.hpp†L1-L142】
- Sin embargo, `main.cpp` no lo usa; imprime y procesa cada opción manualmente. 【F:src/main.cpp†L277-L594】
- Portar los archivos de Stockfish permitiría reemplazar el código ad-hoc por una tabla declarativa, pero habría que alinear nombres (`Move Overhead` vs `MoveOverhead`, etc.) y conectar callbacks para actualizar `EngineOptions`.
- Como paso intermedio, podría integrarse el header existente y validar qué gaps cubre respecto a la versión de Stockfish.

## Recomendaciones

1. **Plan de refactorización incremental**: comenzar extrayendo componentes existentes (`GlobalTranspositionTable`, gestión de tiempo) a archivos dedicados dentro de `src/` y `include/sirio/`. Esto aproxima la estructura a la de Stockfish y facilita portar *patches* específicos.
2. **Normalizar opciones UCI**: adoptar `uci_options.hpp` (o el futuro `ucioption.*`) para centralizar el registro de opciones antes de mover el bucle UCI a un archivo separado.
3. **Evaluar coste/beneficio de `tune.*`**: solo merece la pena si se añade una herramienta de ajuste automática; de lo contrario, puede posponerse.
4. **Coordinar con cambios de tipos**: si se introduce `types.h`, hacerlo antes de portar código que lo requiera para evitar ediciones repetidas.

