# Handling FEN-strings

SirioC utiliza cadenas FEN (Forsyth-Edwards Notation) para cargar y serializar posiciones de ajedrez. El motor puede inicializar un tablero a partir de una cadena FEN arbitraria, o producir una cadena FEN que describa el estado actual del tablero. Toda la lógica central reside en `sirio::Board`, concretamente en el método `set_from_fen`, que valida cada parte de la cadena antes de actualizar la representación interna basada en bitboards.

## FEN-string definitions

Una cadena FEN completa contiene seis campos separados por espacios:

1. **Piece placement**: ocho filas separadas por `/`, donde los dígitos indican casillas vacías y las letras (`PNBRQK` en mayúsculas para blancas, minúsculas para negras) indican piezas.
2. **Side to move**: `w` si juegan las blancas o `b` si juegan las negras.
3. **Castling rights**: combinación de `KQkq` para los enroques disponibles; `-` si no hay derechos.
4. **En passant**: casilla disponible para captura al paso (`-` si ninguna).
5. **Half-move clock**: número de jugadas desde la última captura o movimiento de peón, usado para la regla de las cincuenta jugadas.
6. **Full-move number**: número de jugada empezando en 1 e incrementado tras cada jugada de las negras.

## The FEN-parser

El método `set_from_fen` divide el texto FEN en sus seis partes y aplica validaciones específicas para cada una. Si falta alguno de los campos, se lanza una excepción `std::invalid_argument`, lo que evita que el tablero entre en un estado inconsistente. Tras procesar la cadena, el motor actualiza la historia (`Board::history_`) para que las operaciones posteriores, como deshacer movimientos, tengan un punto de partida coherente.

## FEN definitions

Cada validación se apoya en funciones auxiliares:

- `Board::piece_type_from_char` traduce letras a tipos de pieza, y falla si el símbolo no es reconocido.
- `Board::square_from_string` convierte casillas algebraicas en índices de 0 a 63.
- Las funciones `piece_hash`, `castling_hash`, `en_passant_hash` y `side_to_move_hash` actualizan la clave Zobrist para el hash incremental del tablero.

Estos componentes garantizan que los datos provenientes de la cadena FEN se ajusten al modelo interno de SirioC.

## FEN setup

Antes de analizar el texto, el tablero llama a `Board::clear()` para reiniciar bitboards, listas de piezas, máscara de ocupación y estado incremental. Posteriormente rellena cada estructura a medida que procesa la cadena. Si la cadena describe la posición inicial estándar, también puede reconstruirse invocando el constructor por defecto de `Board`, que utiliza `kStartPositionFEN`.

## Split the FEN-string

`set_from_fen` utiliza un `std::istringstream` para extraer los seis campos en orden (`placement`, `active_color`, `castling_rights_text`, `en_passant_text`, `halfmove_text`, `fullmove_text`). La ausencia de cualquiera de ellos provoca una excepción inmediata, lo que facilita detectar FENs incompletas.

## Create the FEN part parsers

Cada subsección del método maneja una parte concreta:

- **Piece placement** recorre los caracteres de `placement`, administra la cuenta de casillas vacías, valida divisores de filas (`/`) y actualiza bitboards y listas de piezas. Cualquier símbolo inesperado o número fuera de rango dispara un error.
- **Side to move** verifica que el campo sea `w` o `b` y ajusta el hash Zobrist cuando juegan las negras.
- **Castling rights** acepta `KQkq` en cualquier orden o `-`. Cada derecho habilitado actualiza tanto el estado como el hash.
- **En passant** permite `-` o una casilla válida, e incorpora su archivo en el hash Zobrist.
- **Move counters** convierten los campos `halfmove_text` y `fullmove_text` a enteros y validan que sean no negativos (half-move) y positivos (full-move).

## Part 1: Piece setup

El análisis del primer campo recorre de la fila 8 a la 1, manteniendo punteros de fila y columna. Cada pieza encontrada activa el bit correspondiente en el bitboard del color y tipo adecuado, actualiza la lista de piezas y combina el hash Zobrist del par (color, pieza, casilla). Las filas deben sumar exactamente ocho columnas; de lo contrario, se informa un error.

## Part 2: Side to move

Tras la colocación de piezas, el motor determina a qué color le toca mover. El campo debe ser `w` o `b`. Cuando se establece que mueven las negras, se alterna el bit de hash `side_to_move` para reflejarlo.

## Part 3: Castling rights

Los derechos de enroque se almacenan en `Board::CastlingRights`. El analizador procesa cada carácter, habilita los flags correspondientes y actualiza el hash asociado. Repeticiones del mismo símbolo simplemente se ignoran porque los flags ya estaban activados.

## Part 4: En Passant

El campo en passant puede ser `-` o una casilla. Cuando es una casilla válida, se almacena su índice y se incorpora al hash mediante `en_passant_hash(file)`. Esto permite distinguir posiciones idénticas salvo por la posibilidad de captura al paso.

## Part 5: Half-Move clock

El reloj de medias jugadas se parsea con `std::stoi`. Valores negativos son rechazados para mantener la coherencia con las reglas de la FIDE. El valor queda disponible a través de `Board::halfmove_clock()` y se usa posteriormente para detectar tablas por repetición de 50 jugadas.

## Part 6: Full-Move number

El contador de jugadas completas también se obtiene con `std::stoi` y debe ser mayor que cero. La interfaz pública `Board::fullmove_number()` expone este valor, que se incrementa automáticamente cuando se aplican movimientos.

Esta arquitectura permite que cualquier posición válida en notación FEN se cargue de forma segura, preservando tanto la información necesaria para el juego como los metadatos auxiliares imprescindibles para funciones de búsqueda y repetición.
