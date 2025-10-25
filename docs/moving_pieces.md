# Moving Pieces

SirioC actualiza el estado del tablero de manera incremental a través de `Board::apply_move`. La función combina operaciones sobre
bitboards, listas de piezas, derechos de enroque, casillas de captura al paso y hash de Zobrist para mantener una visión coherente
del juego después de cada movimiento.

## Incremental updates

`apply_move` parte de una copia del tablero actual y aplica cambios mínimos a cada estructura interna. Siempre que una pieza es
retirada o añadida a una casilla, la clave Zobrist se ajusta con `piece_hash`. La casilla de enroque al paso se limpia y vuelve a
introducirse solo cuando una captura al paso es posible, y el bit correspondiente a la mano que mueve se alterna al final del
proceso. Estos cambios mantienen sincronizados los bitboards, la ocupación agregada, el historial y el hash.【F:src/board.cpp†L463-L566】【F:src/board.cpp†L593-L619】

## Movement helpers

La implementación emplea lambdas locales como primitivas de movimiento. Cada una actualiza tanto los bitboards como el hash de
Zobrist y las listas de piezas, lo que permite mantener consistencia sin volver a generar el estado completo del tablero.

- **Remove piece from square**. `remove_piece_hash` y la actualización de `pieces_ref` eliminan una pieza de su casilla, borrándola
de la lista correspondiente y del hash.【F:src/board.cpp†L470-L491】
- **Put piece onto square**. Tras determinar la pieza colocada (promociones incluidas), el motor marca la casilla de destino en el
bitboard, añade el índice a la lista de piezas y actualiza el hash.【F:src/board.cpp†L588-L592】
- **Move piece from one square to another**. La secuencia de eliminar en la casilla de origen y añadir en la casilla de destino
modela un movimiento normal, mientras que las capturas y promociones actualizan listas y hash del oponente cuando es necesario.【F:src/board.cpp†L488-L592】
- **Set a square to be the en-passant destination**. Cuando un peón avanza dos casillas, se calcula la casilla intermedia y se
almacena en el estado junto con su hash asociado.【F:src/board.cpp†L606-L612】
- **Clear the en-passant square**. Antes de procesar el movimiento, `clear_en_passant_hash` elimina cualquier marca previa para
asegurar que la casilla solo permanezca cuando es legal.【F:src/board.cpp†L478-L486】
- **Swap active side from one to the other**. Al final del movimiento se alterna `state_.side_to_move`, se ajusta el hash y, si las
negras acaban de jugar, se incrementa el número de jugada completa.【F:src/board.cpp†L613-L619】
- **Update castling permissions**. Las lambdas `reset_castling_rights` y `update_rook_rights_on_move` deshabilitan permisos cuando se
mueve el rey, un alfil de torre, o cuando la torre es capturada, manteniendo sincronizado el hash con los derechos vigentes.【F:src/board.cpp†L493-L567】

Estas primitivas permiten que `apply_move` realice cambios mínimos pero completos, cruciales para algoritmos de búsqueda que
aplican y deshacen movimientos con frecuencia.
