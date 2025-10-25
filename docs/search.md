# Search

SirioC implementa una búsqueda `negamax` con poda alfa-beta reforzada con heurísticas de
ordenación de movimientos. El algoritmo intenta explorar primero las jugadas más prometedoras
utilizando tablas de transposición con almacenamiento de profundidad y tipo de nodo, heurísticas
de movimientos asesinos y la regla MVV/LVA para capturas. Estas optimizaciones están encapsuladas
en helpers internos y en `SearchContext`, que acompaña a cada llamada recursiva para conservar el
historial de movimientos especiales.【F:src/search.cpp†L18-L134】

## 5.1. Iterative deepening y gestión del tiempo

La función `search_best_move` realiza ahora una búsqueda iterativa desde profundidad 1 hasta la
profundidad objetivo, interrumpiendo en cuanto expira el presupuesto temporal calculado por
`compute_time_limit`. El `SearchContext` mantiene el instante de inicio y un contador de nodos que
se usa para comprobar periódicamente si se debe abortar la rama actual cuando se agota el tiempo
disponible.【F:src/search.cpp†L37-L334】【F:src/search.cpp†L338-L410】

## 5.2. Move Ordering

La ordenación de movimientos se aplica justo antes de iterar sobre los candidatos generados
por `generate_legal_moves`. Cada jugada obtiene una puntuación heurística y la lista se reordena
de forma estable para preservar la prioridad de las capturas más tácticas. De esta manera el
primer corte beta suele aparecer con menos ramificaciones, acelerando la búsqueda.【F:src/search.cpp†L79-L120】

### 5.2.1. The reason

La poda alfa-beta depende de encontrar rápidamente jugadas que refuten el nodo actual; si esas
jugadas aparecen tarde en la lista, la búsqueda debe evaluar muchas alternativas inútiles. Al
calcular una puntuación con `score_move` y ordenar antes del bucle principal, `negamax` explora
primero la respuesta más probable, elevando `alpha` o produciendo un corte temprano cuando es
posible.【F:src/search.cpp†L118-L155】

### 5.2.2. How it works

`order_moves` transforma el vector de movimientos en pares `(puntuación, movimiento)`, ordena
por puntuación descendente y reconstruye la lista. Las puntuaciones se derivan de `score_move`,
que evalúa la coincidencia con la tabla de transposición, la naturaleza de captura o si el
movimiento coincide con una heurística asesina previa. El resultado es una secuencia priorizada
antes de entrar en la recursión.【F:src/search.cpp†L48-L120】

### 5.2.3. MVV_LVA

Las capturas reciben un sesgo adicional mediante la heurística MVV/LVA (Most Valuable Victim /
Least Valuable Aggressor). `mvv_lva_score` otorga más puntos a capturas de piezas valiosas con
atacantes baratos, utilizando valores básicos (peón=100, caballo=320, etc.) y penalizando el
valor del atacante. Así, el orden prioriza sacrificios favorables o tácticas de alto impacto en
primer lugar.【F:src/search.cpp†L28-L56】

### 5.2.4. Killer moves heuristic

Las jugadas silenciosas que provocan un corte beta se almacenan en dos ranuras por plie
(`killer_moves`). Si en un nodo futuro el mismo patrón aparece, `killer_score` añade una bonificación
para que `order_moves` las sitúe antes de otras jugadas tranquilas. Esto permite reutilizar
refutaciones previas dentro de la misma rama de profundidad.【F:src/search.cpp†L30-L56】【F:src/search.cpp†L95-L117】

### 5.2.5. TT-move ordering

`SearchContext` mantiene una tabla de transposición completa (`tt_entries`) que almacena el mejor
movimiento, la profundidad alcanzada, el valor evaluado y el tipo de nodo (exacto, límite inferior
o límite superior). Cuando la búsqueda vuelve a visitar la misma posición, `score_move` concede la
puntuación más alta al movimiento almacenado y, si la entrada posee suficiente profundidad,
`negamax` puede producir un corte inmediato ajustando los límites alfa/beta. Las entradas solo se
reemplazan cuando la nueva búsqueda alcanza igual o mayor profundidad.【F:src/search.cpp†L22-L139】【F:src/search.cpp†L170-L248】

## 5.3. Quiescence search

Al alcanzar profundidad cero, SirioC no se detiene en una evaluación estática inmediata. En su
lugar ejecuta una quiescence search que examina todas las capturas, promociones y capturas al paso
legales. Esta extensión evita el horizonte táctico y estabiliza la valoración al descartar ruidos
producidos por entregas superficiales.【F:src/search.cpp†L254-L289】
