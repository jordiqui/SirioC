# SirioC Benchmark Report

## Metodología

Se añadieron suites de referencia para Stockfish, Berserk y Obsidian en `bench/`, y se automatizó su ejecución mediante `run_benchmarks.py`. El script inicia el motor en modo UCI, aplica los límites publicados por cada suite y almacena la telemetría expuesta por la nueva instrumentación del buscador.【F:bench/run_benchmarks.py†L6-L189】【F:bench/latest_bench.json†L1-L120】

Para mantener un tiempo de ejecución homogéneo en esta corrida de control, se limitaron las evaluaciones a las tres primeras posiciones de cada suite y se forzó un tope de 20k nodos (`--limit-type nodes --limit-value 20000 --positions 3`).【bd814b†L1-L11】 La instrumentación por nodo y eventos permite correlacionar las pausas con los instantes en los que se alcanzan límites de recursos.【F:bench/latest_bench.json†L17-L120】

## Resultados

| Suite | Límite base | Nodos acumulados | Tiempo (s) | Nodos/s |
|-------|-------------|------------------|-----------:|--------:|
| Stockfish Reference Bench | depth 13 | 60 000 | 0.27 | 222 927 |
| Berserk Gauntlet Bench | depth 14 | 60 000 | 0.21 | 286 683 |
| Obsidian Stress Bench | nodes 400 000 | 60 000 | 0.20 | 304 898 |

Los resultados provienen directamente de `bench/latest_bench.json`, que preserva tanto los totales agregados como la cronología de eventos instrumentados para cada posición.【F:bench/latest_bench.json†L1-L254】 Un extracto típico muestra cómo, tras alcanzar el límite de nodos, se registra un evento `limit/node` seguido de la cancelación de la iteración en curso, lo que facilita detectar pausas inesperadas.【F:bench/latest_bench.json†L71-L120】

## Visualización

La tabla anterior resume el throughput agregado (nodos/s) alcanzado por SirioC en cada suite bajo el tope homogéneo utilizado en esta medición. El script `bench/run_benchmarks.py` emite los datos necesarios en `bench/latest_bench.json`, de modo que cada equipo pueda transformar los resultados en gráficos con sus propias herramientas sin necesidad de incorporar archivos binarios al repositorio.【F:bench/run_benchmarks.py†L190-L292】【F:bench/latest_bench.json†L1-L120】
