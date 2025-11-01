# Transposition table replacement policy

El módulo de hash adopta ahora una heurística de reemplazo más elaborada, en la que cada entrada
puntúa la profundidad efectiva, la frescura de la generación y el tipo de nodo antes de competir por
una ranura del clúster. `replacement_score` otorga un peso significativo a las entradas exactas y a
las que incluyen movimiento recomendado, con penalización proporcional a la edad, replicando el
comportamiento observado en motores como Stockfish y Berserk que favorecen información reciente pero
profunda.【F:src/tt.cpp†L111-L140】 Esto reduce la sustitución prematura de nodos críticos frente al
esquema `depth-age` previo.

Para mantener un estilo agresivo similar al de Obsidian, la prioridad también premia nodos con
evaluación estática almacenada y penaliza generaciones muy antiguas. De esta forma la tabla conserva
líneas tácticas actuales aun cuando la memoria configurada sea reducida, prolongando la vida útil de
las entradas más informativas sin perder reactividad ante nuevas ramas exploradas.【F:src/tt.cpp†L111-L140】

