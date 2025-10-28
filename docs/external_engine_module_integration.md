# Integración de módulos externos para fortalecer SirioC

Este documento sugiere módulos específicos de motores de ajedrez reconocidos y resume cómo podrían integrarse en SirioC para mejorar su fuerza de juego. Se destacan dependencias, beneficios y pasos preliminares.

## Berserk

### Heurísticas de poda adaptativa
- **Qué aporta**: Berserk destaca por su combinación de *null-move pruning* agresiva y *aspiration windows* adaptativos que se recalibran en base a la estabilidad de la evaluación.
- **Integración sugerida**: Revisar `src/search.cpp` para aislar la lógica de `search_pv` y permitir inyectar heurísticas configurables. Crear puntos de extensión para ajustar los márgenes de *null-move* y las reglas de *late move pruning*.
- **Preparación requerida**:
  1. Extraer parámetros de poda a una estructura en `include/sirio/search_params.hpp` (nuevo archivo).
  2. Añadir métricas de estabilidad de evaluación en `SearchSharedState` para soportar ventanas de aspiración dinámicas.

### Gestión de tiempo inspirada en Berserk
- **Qué aporta**: Berserk distribuye el tiempo en función de la variación de la evaluación y el número estimado de jugadas restantes.
- **Integración sugerida**: Complementar el plan descrito en `docs/upstream_module_viability.md` para separar `timeman.*`. Añadir ponderación por volatilidad de evaluación al cálculo del límite blando de tiempo.

## Obsidian

### Pipelines NNUE
- **Qué aporta**: Obsidian encapsula las redes neuronales en capas desacopladas del motor, permitiendo intercambiar modelos con *feature transformers* especializados.
- **Integración sugerida**:
  - Introducir un submódulo `src/nnue/` con interfaces definidas en `include/sirio/nnue/`. Comenzar con adaptadores que consuman la estructura de características de SirioC (`include/sirio/nnue_features.hpp`).
  - Añadir pruebas en `tests/` que validen la equivalencia de evaluación entre el backend actual y el adaptador.
- **Preparación requerida**: Documentar en `docs/training/` un flujo para convertir redes Obsidian a la representación esperada por SirioC.

### Base de finales híbrida
- **Qué aporta**: Obsidian combina *Syzygy* con tablas propietarias en un gestor unificado.
- **Integración sugerida**: Extender `tablebases/` para soportar múltiples proveedores mediante una fábrica (`TablebaseProvider`), permitiendo registrar Syzygy y futuros módulos.

## Stockfish

### Tabla de transposición avanzada
- **Qué aporta**: Stockfish implementa técnicas como *secondary aging* y particiones por hilo para mejorar la reutilización de nodos.
- **Integración sugerida**: Seguir la recomendación existente de extraer `GlobalTranspositionTable` a `src/tt.cpp`. Una vez modularizado, importar la lógica de reemplazo secundaria de Stockfish y adaptar las estructuras de empaquetado.
- **Preparación requerida**: Añadir pruebas de colisiones en `tests/tt_tests.cpp` que verifiquen la preservación del comportamiento actual antes de incorporar los cambios.

### Gestión de opciones UCI
- **Qué aporta**: Stockfish expone opciones tipadas con callbacks y validaciones centralizadas.
- **Integración sugerida**: Terminar de implementar `include/sirio/uci_options.hpp` para registrar opciones y delegar en un nuevo `src/uci.cpp`. Esto abre la puerta a portar `ucioption.cpp` y reducir la lógica manual en `src/main.cpp`.

### Infraestructura de *tuning*
- **Qué aporta**: El módulo `tune.cpp` automatiza experimentos de parámetros mediante macros `TUNE()` y scripts de soporte.
- **Integración sugerida**: Solo perseguir esta integración cuando exista un pipeline de entrenamiento automatizado en `training/`. Mientras tanto, documentar parámetros sensibles en `docs/evaluation.md` para facilitar ajustes manuales.

## Recomendaciones generales
1. Priorizar integraciones que requieran refactorizaciones ya identificadas (tabla de transposición, gestor de tiempo), evitando desviarse hacia módulos con baja preparación (p. ej. `tune.*`).
2. Introducir pruebas unitarias específicas por módulo antes de portar código externo para asegurar compatibilidad y detectar regresiones.
3. Mantener documentación viva en `docs/` que registre el estado de cada integración, responsables y métricas comparativas en partidas de referencia.

