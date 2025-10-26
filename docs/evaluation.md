# 6. Evaluation

La función de evaluación de SirioC se encarga de proporcionar una estimación estática de la
posición para guiar la búsqueda negamax. Actualmente se apoya en dos ingredientes clásicos:
conteo de material, tablas pieza-casilla precomputadas y una bonificación por pareja de
alfiles.【F:src/evaluation.cpp†L80-L110】

## 6.1. Understanding evaluation

`evaluate` recorre cada color y tipo de pieza, extrae las casillas ocupadas del bitboard y suma
su contribución al marcador. El resultado se expresa desde la perspectiva de las blancas, de modo
que los valores positivos favorecen a ese bando.【F:src/evaluation.cpp†L80-L110】

## 6.2. Material counting

Los valores básicos de cada tipo de pieza están codificados en `piece_values` siguiendo la
escala tradicional (peón=100, caballo=320, etc.). Durante la iteración se añaden a la puntuación
cada vez que aparece una pieza del color correspondiente.【F:src/evaluation.cpp†L11-L95】

## 6.3. Piece-Square Tables

Cada pieza dispone de una tabla de 64 entradas que refleja preferencias posicionales: peones
avanzados, caballos centralizados o reyes resguardados, entre otros patrones. Para las negras se
emplea `mirror_square`, que refleja la casilla sobre el eje horizontal y permite reutilizar las
mismas tablas definidas para las blancas.【F:src/evaluation.cpp†L13-L77】【F:src/evaluation.cpp†L89-L94】

## 6.4. The evaluation function

El bucle principal combina el valor material y la bonificación posicional (`ps_value`) para cada
pieza encontrada en la posición. Si un bando conserva ambos alfiles, se suma también
`bishop_pair_bonus`. Las contribuciones de las piezas negras se restan, generando un único
resultado con signo desde la perspectiva de las blancas.【F:src/evaluation.cpp†L80-L110】

## 6.5. Tapering the evaluation

La versión actual no aplica *tapering* ni pesos según la fase de la partida: tanto el material como
las tablas pieza-casilla tienen la misma influencia en aperturas y finales. Incorporar escalados
basados en el material total sería una mejora futura para adaptar la evaluación a cada fase.

## 6.6. Redes NNUE híbridas

Además del evaluador clásico, SirioC puede cargar redes NNUE entrenadas externamente. El backend
acepta una configuración primaria y una secundaria; cuando ambas están presentes, el motor decide
qué red utilizar según un umbral de fase basado en material total (`material:NN`) o en la profundidad
de búsqueda (`depth:NN`).【F:src/main.cpp†L19-L116】【F:src/nnue/backend.cpp†L18-L187】

Cada red mantiene su propio estado incremental (`push`/`pop`), por lo que la transición resulta
transparente para la búsqueda. Si no se proporciona red secundaria o el umbral es cero, la red
principal se emplea en todos los nodos.【F:src/nnue/backend.cpp†L118-L187】
