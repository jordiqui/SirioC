# Prácticas recomendadas para análisis extensos

Los análisis que se prolongan durante horas o días requieren vigilancia activa del motor para evitar bloqueos y para entender cómo evoluciona la búsqueda. Las siguientes pautas condensan la experiencia operativa con SirioC tras la introducción de los nuevos mecanismos de monitorización.

## Supervisión de la cola de trabajo

* El motor expone un vigilante interno que monitoriza la cola de tareas de búsqueda y relanza cualquier hilo que deje de enviar pulsos durante más de tres segundos mientras existan tareas pendientes. Esta supervisión está habilitada por defecto: no es necesario configurar opciones adicionales.
* Para aprovecharla al máximo, mantén actualizados los valores de `Threads` en la interfaz UCI. El vigilante dimensiona la piscina según el número recomendado y la redimensiona automáticamente cuando se modifican las opciones de hilo.
* Si se detecta una saturación persistente (cola en crecimiento continuo), ajusta manualmente los límites de nodos o de tiempo para escalonar los análisis. El reinicio automático de hilos evita deadlocks, pero no resuelve una sobrecarga crónica del sistema anfitrión.

## Seguimiento de KPIs en tiempo real

* El script `tools/kpi_logger.py` lanza el binario de SirioC, establece la posición y transmite la búsqueda en modo UCI, registrando métricas clave en vivo: profundidad (`d`), profundidad selectiva (`sd`), nodos, NPS y estabilidad de la mejor jugada.
* La estabilidad aumenta cuando la jugada principal y la evaluación (en centipeones o mate) se mantienen casi constantes en iteraciones consecutivas. Un valor elevado es una buena señal de convergencia; una caída abrupta indica que la línea crítica ha cambiado y merece ser revisada.
* Ejemplo de uso para un análisis infinito desde la posición inicial:
  ```bash
  ./tools/kpi_logger.py --engine "./build/sirio" --fen startpos --stability-threshold 25
  ```
* Para estudios con límites concretos añade `--movetime` (milisegundos) o `--depth`. El script captura `Ctrl+C`, envía `stop` al motor y continúa procesando la última información antes de imprimir `bestmove`.

## Ajustes para sesiones prolongadas

* Conserva el tablero limpio de módulos externos (tablebases o libros) a menos que aporten valor directo al análisis. Cada dependencia adicional puede introducir I/O no determinista que retrasa la búsqueda y genera falsos positivos en la monitorización.
* Activa registros del sistema (por ejemplo `dmesg -w` o herramientas de observabilidad del SO) para cruzar eventos de CPU o memoria con los NPS reportados por el script. Un descenso sostenido en NPS acompañado de conmutación frecuente puede indicar termal throttling o swapping.
* Planifica puntos de control: guarda periódicamente FENs y líneas PV relevantes. Aunque el vigilante repondrá hilos bloqueados, no puede recuperar la información si el proceso se termina abruptamente por factores externos.

## Checklist operativo

1. Compila la versión más reciente de SirioC (`cmake --build build`) para incluir mejoras del vigilante.
2. Lanza el `kpi_logger.py` con la configuración deseada y verifica que los mensajes `info` aparecen con la cadencia esperada.
3. Ajusta límites de tiempo/nodos según la disponibilidad del hardware y observa la métrica de estabilidad para decidir cuándo detener el análisis o ramificar líneas alternativas.

Con estos pasos el motor se mantiene resiliente frente a bloqueos internos y, a la vez, ofrece una telemetría rica que facilita la toma de decisiones durante análisis extensos.

