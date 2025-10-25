# 7. Communication

SirioC expone una interfaz UCI mínima para integrarse con GUIs de ajedrez. El ejecutable lee
comandos línea a línea desde `stdin` y responde en `stdout`, lo que facilita la integración con
herramientas como Cute Chess o Arena.【F:src/main.cpp†L71-L116】

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

- `uci`: envía la identificación del motor y confirma con `uciok` mediante `send_uci_id`.
- `isready`: responde con `readyok` desde `send_ready`.
- `ucinewgame`: restablece el `Board` a la posición inicial.
- `position`: interpreta `startpos` o un bloque `fen`, aplica los movimientos listados y actualiza el
  tablero con `set_position`.
- `go`: acepta parámetros sencillos (`depth`) y delega en `handle_go` para lanzar la búsqueda y
  devolver `bestmove`.
- `stop`/`quit`: abandonan el bucle.
- `d`: imprime el FEN actual para depuración.【F:src/main.cpp†L15-L110】【F:src/main.cpp†L118-L166】

## 7.4. Design

La lógica se concentra en tres funciones auxiliares:

1. `send_uci_id` encapsula la presentación del motor (nombre y autor) y la señal `uciok`.
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
