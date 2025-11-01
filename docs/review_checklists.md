# Listas de verificación para revisiones

Estas listas ayudan a alinear las contribuciones con la filosofía de SirioC.
Utilízalas en cada revisión de código y compártelas con cualquier nueva persona
colaboradora durante su incorporación.

## Checklist de preparación (autoría)

- [ ] El cambio se alinea con los rasgos distintivos descritos en
      `docs/engine_comparison.md` o explica claramente por qué se aparta.
- [ ] Se ha documentado cualquier nueva opción UCI, parámetro de búsqueda o ajuste
      relevante en la sección correspondiente de `docs/`.
- [ ] Las pruebas automatizadas relacionadas (`tests/`) se han actualizado o
      ampliado según corresponda.
- [ ] Se incluye un resumen claro en el mensaje de commit y en la descripción de la
      PR para facilitar la revisión.
- [ ] La contribución respeta la licencia MIT y reconoce el trabajo previo cuando
      corresponde.

## Checklist de revisión (personas revisoras)

- [ ] Comprendo el objetivo del cambio y su relación con la hoja de ruta de SirioC.
- [ ] Confirmo que el diseño mantiene la modularidad y legibilidad de las
      secciones afectadas.
- [ ] Verifico que la documentación se haya actualizado (o no sea necesaria) y que
      sea consistente con el comportamiento implementado.
- [ ] Reviso los resultados de pruebas o benchmarks adjuntos y solicito datos
      adicionales si la cobertura es insuficiente.
- [ ] Evalúo si la contribución introduce dependencias externas o configuraciones
      complejas y, de ser así, pido mitigaciones o documentación adicional.
- [ ] Registro cualquier decisión tomada en la conversación de la PR para que quede
      constancia de las razones detrás de la aprobación o cambios solicitados.

## Difusión y aplicación continua

- Anuncia estas listas en los canales habituales del proyecto (por ejemplo,
  README, repositorio o foros internos) y enlázalas en las plantillas de pull
  requests.
- Incluye el enlace a este documento y a `docs/engine_comparison.md` en los
  mensajes de bienvenida a nuevos colaboradores.
- Durante las revisiones futuras, referencia explícitamente los puntos de la
  checklist cuando apruebes o solicites cambios para reforzar el hábito.
