# 7. Communication

SirioC expone una interfaz UCI mínima para integrarse con GUIs de ajedrez. El ejecutable lee
comandos línea a línea desde `stdin` y responde en `stdout`, lo que facilita la integración con
herramientas como Cute Chess o Arena.【F:src/main.cpp†L17-L122】

## 7.1. Introduction

El bucle principal comienza con un `Board` vacío y procesa cada línea recibida. Los comandos
reconocidos siguen el estándar UCI: `uci`, `isready`, `ucinewgame`, `position`, `go`, `stop` y
`quit`, además del atajo `d` para imprimir la posición actual en FEN. Cada caso se gestiona en un
bloque `if` que delega la lógica en funciones auxiliares cuando es necesario.【F:src/main.cpp†L71-L116】

## 7.2. Comparison

A diferencia de motores más avanzados que mantienen colas de mensajes o hilos dedicados, SirioC
utiliza un bucle monohilo y bloqueante. Esta elección simplifica el flujo y reduce el riesgo de
condiciones de carrera, a costa de no poder pensar en segundo plano mientras espera nuevas órdenes.
El comportamiento sigue la guía de Rustic Chess, priorizando claridad sobre rendimiento máximo.

## 7.3. How it works

- `uci`: envía la identificación del motor, publica las opciones (incluidas `EvalFile` y
  `EvalFileSmall`) y confirma con `uciok` mediante `send_uci_id`.
- `isready`: garantiza que cualquier ruta `EvalFile` pendiente se haya intentado cargar antes de
  contestar `readyok` desde `send_ready`.
- `ucinewgame`: restablece el `Board` a la posición inicial.
- `position`: interpreta `startpos` o un bloque `fen`, aplica los movimientos listados y actualiza el
  tablero con `set_position`.
- `go`: acepta parámetros sencillos (`depth`) y delega en `handle_go` para lanzar la búsqueda y
  devolver `bestmove`.
- `stop`/`quit`: abandonan el bucle.
- `d`: imprime el FEN actual para depuración.【F:src/main.cpp†L15-L110】【F:src/main.cpp†L118-L166】

## 7.4. Design

La lógica se concentra en tres funciones auxiliares:

1. `send_uci_id` encapsula la presentación del motor (nombre y autor), declara las opciones UCI
   soportadas y emite la señal `uciok`.
2. `set_position` restaura la posición inicial o configura una FEN personalizada, procesando el
   historial de movimientos para mantener el estado coherente.
3. `handle_go` abstrae la búsqueda (`search_best_move`) y emite tanto la información intermedia como
   el `bestmove`.

Esta separación permite evolucionar cada pieza de la interfaz sin afectar el bucle principal.【F:src/main.cpp†L15-L110】

## 7.5. Implementing IComm

Aunque la interacción actual se gestiona directamente con `std::cin`/`std::cout`, es sencillo
envolverla en una interfaz `IComm`. Esta podría definir métodos como `read_command()`,
`send_response(std::string_view)` y `send_error(std::string_view)`. Implementar un adaptador para
flujos estándar mantendría el comportamiento actual, mientras que adaptadores alternativos podrían
conectar sockets, tuberías o entornos de pruebas automatizadas.

## 7.6. Implementing commands

Para añadir nuevos comandos, conviene seguir el patrón existente: analizar tokens con
`std::istringstream`, delegar en funciones específicas y mantener las respuestas en el formato UCI.
Las funciones auxiliares deben recibir solo los datos necesarios (por ejemplo, argumentos del
comando y una referencia const o mutable al `Board`). Este enfoque minimiza el acoplamiento y hace
que probar cada comando de forma aislada sea más sencillo.

## 7.7. Recommended UCI flow

Para integrarlo con GUIs como Fritz o Cute Chess se recomienda el siguiente intercambio:

1. Enviar `uci` y esperar a `uciok`.
2. Enviar `isready` antes de comenzar cada partida para asegurarse de que el motor está libre y que
   el backend NNUE (si se configuró `EvalFile`) se haya inicializado correctamente.
3. Reiniciar con `ucinewgame` y establecer la posición mediante `position startpos moves ...` o un
   bloque `position fen ...` seguido de la lista de jugadas.
4. Lanzar la búsqueda con `go`, proporcionando parámetros de tiempo (`wtime`, `btime`, `winc`,
   `binc`, `movestogo`, `movetime`) o límites explícitos (`depth`, `nodes`). El motor calcula los
   márgenes de tiempo a partir de esos valores y mantiene un contador de nodos para las búsquedas
   recortadas.【F:src/main.cpp†L79-L139】【F:src/search.cpp†L31-L230】【F:src/search.cpp†L400-L470】
5. Cuando la GUI envía `stop` o el límite expira, SirioC devuelve siempre un `bestmove` legal: si la
   búsqueda no produjo PV válido, se recurre a la primera jugada generada en el estado actual, por lo
   que la GUI nunca recibe `0000` salvo que no existan movimientos legales.【F:src/main.cpp†L131-L144】

Limitaciones actuales: el motor mantiene un único hilo principal de comunicación y la gestión del
tiempo sigue siendo aproximada. Aun así respeta los márgenes duros/soft de tiempo y los topes de
nodos establecidos por la GUI, y ahora ofrece opciones UCI básicas (`Threads`, `SyzygyPath` y las
dos rutas NNUE) para integrarse mejor con entornos modernos.【F:src/main.cpp†L17-L207】【F:src/search.cpp†L31-L230】【F:src/search.cpp†L400-L470】
