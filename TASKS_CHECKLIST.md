# Resumen de tareas pendientes

Este documento recopila, en formato de lista de comprobación, las tareas principales pendientes en el proyecto. Se agrupan por área funcional para facilitar su seguimiento.

## NNUE
- [ ] Crear `src/nn/nnue_loader.{h,cpp}` con soporte de carga desde archivo/memoria y mensajes `info string`.
- [ ] Añadir soporte para `EvalFileSmall` y selección automática según el número de piezas.
- [ ] Proporcionar un script de descarga de redes (CC0) y/o posibilidad de incrustación (embed) opcional.

## Syzygy
- [ ] Incluir Fathom como dependencia (vendor/submódulo MIT) e inicializar la ruta con `SyzygyPath`.
- [ ] Añadir las opciones `SyzygyProbeLimit`, `SyzygyProbeDepth` y `Syzygy50MoveRule`.
- [ ] Utilizar `tb_probe_wdl` en nodos interiores y `tb_probe_root` en la raíz cuando el número de piezas no supere el límite configurado.

## Zobrist y tabla de transposición
- [ ] Implementar `zobrist.{h,cpp}` utilizando SplitMix64 con semillas fijas.
- [ ] Mantener la clave Zobrist de forma incremental en `make_move`/`undo_move`.
- [ ] Implementar una tabla de transposición básica utilizando la clave Zobrist.

## Protocolo UCI
- [ ] Implementar `go infinite` con hilo de búsqueda dedicado y parada cooperativa (`stop`).
- [ ] Emitir periódicamente mensajes `info` con `depth`, `time`, `nodes`, `nps` y `pv`.

## Bench
- [ ] Implementar el comando `bench [depth D | movetime ms]` leyendo las posiciones desde `resources/bench.fens`.
- [ ] Mostrar un resumen con el tiempo total, nodos e `nps` alcanzados.

> _Última actualización: 2025-09-30._
