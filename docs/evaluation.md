# 6. Evaluation

La función de evaluación de SirioC se encarga de proporcionar una estimación estática de la
posición para guiar la búsqueda negamax. Combina los ingredientes clásicos (material, tablas
pieza-casilla y pareja de alfiles) con un esquema de *tapering* que pondera cada término según la
fase de la partida.【F:src/evaluation.cpp†L470-L575】

## 6.1. Understanding evaluation

`evaluate` recorre cada color y tipo de pieza, acumula por separado la contribución de medio juego
(`mg_score`) y final (`eg_score`), y calcula un “game phase” sumando `piece_phase_values` para las
fuerzas restantes. El resultado final se interpola entre ambas puntuaciones y se expresa desde la
perspectiva de las blancas.【F:src/evaluation.cpp†L470-L566】

## 6.2. Material counting

Los valores básicos de cada tipo de pieza están codificados en `piece_values_mg` y
`piece_values_eg`, lo que permite matizar la valoración según la fase (por ejemplo, las torres
ganan peso relativo en finales). Cada vez que aparece una pieza del color correspondiente se suma
su valor base junto con la entrada PST específica del medio juego o del final.【F:src/evaluation.cpp†L29-L118】【F:src/evaluation.cpp†L488-L514】

## 6.3. Piece-Square Tables

Cada pieza dispone de una tabla de 64 entradas que refleja preferencias posicionales. El motor
mantiene dos familias: `piece_square_tables_mg` reutiliza las tablas clásicas y
`piece_square_tables_eg` introduce un mapa específico para el rey en finales, favoreciendo su
centralización. Para las negras se aplica `mirror_square`, lo que permite aprovechar las mismas
tablas definidas para las blancas.【F:src/evaluation.cpp†L37-L126】【F:src/evaluation.cpp†L488-L502】

## 6.4. The evaluation function

El bucle principal suma el valor material y las entradas PST en las dos fases, añade los bonus por
pareja de alfiles (`bishop_pair_bonus_mg`/`_eg`) y agrega las evaluaciones adicionales (estructura
de peones, seguridad del rey, movilidad, piezas menores). Cada término se reescala mediante
`scale_term`, permitiendo que la misma heurística pese distinto en apertura y final antes de
combinarse en la interpolación final.【F:src/evaluation.cpp†L470-L566】

## 6.5. Tapering the evaluation

Las heurísticas secundarias (estructura de peones, movilidad, seguridad del rey o valoración de
caballos y alfiles) utilizan factores de ponderación distintos para medio juego y final mediante
`scale_term`. Además, cuando el material restante cae por debajo de `endgame_material_threshold`,
el módulo de finales refuerza la proximidad de los reyes, la búsqueda de esquinas y la oposición
con puntuaciones exclusivamente asociadas al final de partida.【F:src/evaluation.cpp†L512-L566】

## 6.6. Redes NNUE híbridas

Además del evaluador clásico, SirioC puede cargar redes NNUE entrenadas externamente. El backend
acepta una configuración primaria y una secundaria; cuando ambas están presentes, el motor decide
qué red utilizar según un umbral de fase basado en material total (`material:NN`) o en la profundidad
de búsqueda (`depth:NN`).【F:src/main.cpp†L19-L116】【F:src/nnue/backend.cpp†L18-L187】

Cada red mantiene su propio estado incremental (`push`/`pop`), por lo que la transición resulta
transparente para la búsqueda. Si no se proporciona red secundaria o el umbral es cero, la red
principal se emplea en todos los nodos.【F:src/nnue/backend.cpp†L118-L187】
