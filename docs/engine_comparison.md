# Comparativa de motores: SirioC vs. Stockfish, Berserk y Obsidian

Este documento resume las similitudes y diferencias clave entre SirioC y tres motores
reconocidos de la escena open source. El objetivo es identificar los rasgos propios
que guían la evolución del proyecto y los aspectos donde conviene inspirarse o
marcar distancias.

## Visión general

| Característica | SirioC | Stockfish | Berserk | Obsidian |
| --- | --- | --- | --- | --- |
| Lenguaje principal | C++20 modular inspirado en Rustic Chess | C++17 optimizado | C++20 con componentes Rust | C++17 con capas específicas para NNUE |
| Filosofía | Legibilidad, modularidad y trazabilidad | Rendimiento absoluto y experimentación continua | Mezcla de ideas modernas con foco en jugabilidad y matches rápidos | Innovación en heurísticas tácticas y tablas personalizadas |
| Estado de madurez | Proyecto emergente con base estable | Referencia mundial, altamente probado | En crecimiento rápido con matches frecuentes en servidores | Consolidado en torneos comunitarios |
| Arquitectura de búsqueda | Negamax con poda alfa-beta, LMR, null move y aspiration windows | Búsqueda con mejoras avanzadas (probcut, verification search, pruning dinámico extenso) | Búsqueda híbrida con double extensions y pruning agresivo | Búsqueda alfa-beta con extensiones seleccionadas y pruning personalizado |
| Evaluación | Estática con NNUE opcional dual (medio juego/finales) | NNUE principal integrada en rama oficial | NNUE única enfocada a blitz | NNUE propia especializada en táctica |
| Persistencia | Tabla de transposición persistente y libro de aperturas con carga en caliente | Ficheros `evalsave` para NNUE y herramientas externas para libros | Libro Polyglot externo, sin persistencia integrada | Persistencia centrada en redes y sets de pruebas |
| Integraciones | Tablebases Syzygy (3-7), modo análisis persistente, opciones UCI centradas en claridad | Amplio set de opciones UCI, soporte Syzygy y herramientas de testing | Integración con servidores de matches y book learning automatizado | Integración con frameworks de entrenamiento propios |

## Rasgos distintivos de SirioC

- **Base modular y documentada**: cada subsistema (tablero, FEN, búsqueda, evaluación,
  comunicación) dispone de documentación dedicada que prioriza la comprensión del
  flujo antes que la microoptimización.
- **Persistencia integrada**: el modo de análisis persistente guarda y recupera la
  tabla de transposición de forma automática, lo que facilita estudios largos sin
  tooling adicional.
- **Libros de aperturas gestionados internamente**: el motor incluye un libro con
  ponderación estocástica y recarga en caliente, ideal para experimentar con
  repertorios sin herramientas externas.
- **NNUE opcional y dual**: se mantiene la evaluación clásica como opción por defecto,
  pero se ofrece compatibilidad con dos redes NNUE para adaptar el comportamiento a
  distintos escenarios (medio juego frente a finales).
- **Enfoque en claridad UCI**: las opciones expuestas se mantienen concisas para que
  cualquier colaborador comprenda su impacto durante revisiones y pruebas.

## Inspiraciones de otros motores

- **Stockfish**: sus mecanismos de pruebas continuas, el pipeline de NNUE y las
  optimizaciones de poda sirven como referencia para futuras mejoras en búsqueda y
  evaluación. SirioC adopta selectivamente técnicas probadas (LMR, null move) sin
  sacrificar legibilidad.
- **Berserk**: destaca por la velocidad de iteración y la automatización de matches.
  SirioC puede beneficiarse de adoptar flujos de testing reproducibles y perfiles de
  tiempo específicos para blitz.
- **Obsidian**: su énfasis en heurísticas tácticas y tablas especializadas inspira a
  SirioC a mantener módulos experimentales separados para evitar que el núcleo se
  vuelva opaco.

## Guía para colaboradores

1. Consulta esta comparativa antes de proponer cambios profundos en búsqueda o
   evaluación. Ayuda a alinear expectativas respecto al equilibrio entre claridad y
   fuerza de juego.
2. Usa los rasgos distintivos como lista de control: cualquier contribución debería
   conservarlos o justificar claramente por qué se aleja de ellos.
3. Documenta decisiones que introduzcan nuevas técnicas inspiradas en otros motores
   para que puedan revisarse con el contexto adecuado.

Estas pautas complementan la lista de verificación de revisiones descrita en
`docs/review_checklists.md`.
