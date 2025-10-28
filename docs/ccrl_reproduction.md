# Reproducción local de pruebas estilo CCRL

Este documento describe cómo preparar y ejecutar partidas automáticas que
emulan los entornos utilizados por la Computer Chess Rating Lists (CCRL) al
evaluar motores UCI. El objetivo es que cualquier contribución a SirioC se
valide en condiciones similares antes de enviar binarios oficiales.

## Requisitos previos

1. **Compilar SirioC**: asegúrate de tener el binario en `build/bin/sirio`.
2. **Instalar `cutechess-cli`**: la herramienta recomendada por CCRL para
ejecutar matches automatizados. En Debian/Ubuntu puedes instalarla con:
   ```bash
   sudo apt-get install cutechess
   ```
3. **(Opcional) Instalar `psutil`**: el script de benchmarks usa esta
   biblioteca para registrar métricas de CPU y memoria. Instálala con:
   ```bash
   pip install psutil
   ```
4. **(Opcional) Libro de aperturas**: el repositorio incluye
   `bench/ccrl_openings.pgn` con aperturas simétricas de 3 jugadas. Puedes
   sustituirlo por el libro oficial de CCRL si lo tienes disponible.

## Benchmarks disponibles

El script `bench/ccrl_suite.py` agrupa tres configuraciones habituales:

- `ccrl_blitz`: 40/4 + 0.1 (equivalente a partidas de 2+0.1) durante 20
  partidas, el formato empleado por CCRL Blitz.
- `ccrl_rapid`: 40/15 + 0.1 con 8 partidas, equivalente al listado Rapid.
- `ccrl_classic`: 40/120 + 0.2 con 4 partidas, útil para detectar problemas
  de estabilidad en controles prolongados.

Cada benchmark ejecuta un gauntlet de `cutechess-cli` con un solo hilo por
motor, tablas adjudicadas tras 6 medias jugadas sin progreso y renuncias
configuradas según las reglas públicas de CCRL.

## Ejecución básica

Compila el proyecto y lanza la suite:

```bash
cmake -S . -B build
cmake --build build
python bench/ccrl_suite.py --cutechess cutechess-cli --openings bench/ccrl_openings.pgn --save-pgns
```

El comando anterior realiza auto-matches (SirioC contra sí mismo) usando las
aperturas proporcionadas y guarda los registros en `bench/results/`. Para
especificar un oponente distinto añade `--opponent` apuntando al binario
correspondiente:

```bash
python bench/ccrl_suite.py --engine build/bin/sirio \
       --opponent /ruta/a/stockfish \
       --cutechess /usr/bin/cutechess-cli \
       --openings bench/ccrl_openings.pgn \
       --save-pgns
```

Usa `--only` para limitar la ejecución a un subconjunto de benchmarks:

```bash
python bench/ccrl_suite.py --only ccrl_blitz ccrl_rapid
```

## Comparar redes internas con referencias públicas

El script permite lanzar auto-matches donde cada instancia de SirioC carga
redes NNUE distintas. Esto facilita confrontar una red en desarrollo con la
referencia pública sin editar manualmente los comandos de `cutechess-cli`.

- `--internal-evalfile` / `--internal-evalfile-small`: rutas a las redes
  principal y alternativa que quieres probar.
- `--reference-evalfile` / `--reference-evalfile-small`: rutas a las redes
  oficiales (por ejemplo, `nn-1c0000000000.nnue` y `nn-37f18f62d772.nnue`).
- `--internal-label` y `--reference-label`: nombres descriptivos para los
  perfiles de cutechess-cli.
- `--threads`: número de hilos UCI asignados a cada motor durante el match.

Ejemplo de comparación directa entre una red interna y la referencia pública:

```bash
python bench/ccrl_suite.py \
    --internal-evalfile training/nnue/weights/sirio_experimental.nnue \
    --reference-evalfile /ruta/a/nn-1c0000000000.nnue \
    --reference-evalfile-small /ruta/a/nn-37f18f62d772.nnue \
    --internal-label sirio-dev \
    --reference-label sirio-ref \
    --threads 2
```

Al iniciar cada benchmark, la herramienta imprime un resumen de los perfiles
cargados (`EvalFile` y `EvalFileSmall`) para que confirmes que las rutas son
correctas.

## Métricas de CPU y memoria

Si `psutil` está instalado, cada match generará un fichero CSV y un resumen
JSON en `bench/results/<benchmark>_resources.*` con las siguientes métricas:

- `avg_cpu_percent`: uso medio de CPU (sumado en todos los núcleos) durante
  la ejecución del match.
- `max_cpu_percent`: pico de CPU alcanzado.
- `max_rss_bytes`: máximo de memoria residente (RSS) observado.
- `samples`: número de muestras registradas al intervalo indicado.

Puedes ajustar la frecuencia de muestreo con `--monitor-interval` (valor por
defecto: 1 segundo). Para desactivar la captura, establece el argumento en 0:

```bash
python bench/ccrl_suite.py --monitor-interval 0
```

Cuando `psutil` no esté disponible el script añadirá una nota explicativa en
el fichero `<benchmark>_resources.csv` sin detener la ejecución del match.

## Interpretación de resultados

- **PGN**: si usas `--save-pgns`, cada benchmark generará un archivo PGN con
  todas las partidas jugadas.
- **Logs**: el volcado estándar de `cutechess-cli` se almacena en
  `bench/results/<benchmark>.log`.
- **Recursos**: revisa los archivos JSON mencionados para validar que el
  consumo de CPU y memoria está dentro de los límites aceptados por CCRL.

## Envío a CCRL

1. Ejecuta los tres benchmarks y confirma que no hay cuelgues ni abandonos
   inesperados.
2. Revisa los PGN y logs para asegurarte de que el motor responde a los
   comandos UCI esperados (`isready`, `ucinewgame`, etc.).
3. Adjunta los CSV/JSON de recursos junto con la build enviada a CCRL como
   evidencia de compatibilidad.
4. Proporciona la información del entorno (hardware, sistema operativo,
   versión de cutechess-cli) en el correo de solicitud.

Siguiendo estos pasos, los mantenedores pueden verificar el estado del
motor antes de cualquier publicación en listas de clasificación externas.
