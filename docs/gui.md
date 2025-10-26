# Integración con GUIs UCI

Este documento describe cómo registrar SirioC en las GUIs más comunes que soportan el protocolo
UCI. Los pasos asumen que ya compilaste el motor (por ejemplo, `build/bin/sirio` en Linux o
`sirio.exe` en Windows) y que dispones de las redes NNUE que quieras usar.

## Recomendaciones generales

1. Copia el ejecutable y, si lo deseas, las redes NNUE a una carpeta estable (por ejemplo,
   `~/motores/SirioC` en Linux o `C:\\ChessEngines\\SirioC` en Windows).
2. Comprueba desde una terminal que el binario responde a `uci` y `isready`. Esto ayuda a detectar
   problemas de permisos o librerías faltantes antes de integrarlo con una GUI.
3. Ten a mano las rutas de las redes NNUE para rellenar las opciones `EvalFile` y `EvalFileSmall`
   cuando la GUI te lo solicite. El motor permite rutas absolutas o relativas, por lo que no es
   obligatorio que estén en el mismo directorio del ejecutable.

SirioC anuncia sus opciones estándar en cuanto la GUI envía `uci`, incluyendo la lista completa de
parámetros configurables (`Threads`, `Hash`, `EvalFile`, `EvalFileSmall`, etc.). Consulta
`src/main.cpp` para revisar la implementación del comando `setoption` y la inicialización de la
interfaz UCI.

## Fritz 20

1. Abre Fritz 20 y ve a **Archivo → Opciones → Motores → Crear UCI**.
2. Selecciona el ejecutable del motor (`sirio.exe`). Fritz copiará el archivo a su carpeta de
   motores; si prefieres mantener una copia externa desactiva la casilla *Copiar al directorio de
   programa*.
3. Tras añadirlo, pulsa **Opciones** para abrir el diálogo de configuración UCI.
4. En el campo `EvalFile` elige la ruta a la red principal (por ejemplo,
   `C:\\ChessEngines\\SirioC\\nn-1c0000000000.nnue`). Para registrar la red alternativa pequeña,
   usa `EvalFileSmall` y apunta a `nn-37f18f62d772.nnue` en el mismo directorio.
5. Ajusta `Threads` y `Hash` según tu hardware y confirma con **Aceptar**.

A partir de ese momento el motor aparecerá en la lista de motores UCI y podrás usarlo en partidas,
partidas relámpago o análisis infinitos como cualquier otro motor compatible.

## Cute Chess

1. Abre Cute Chess y selecciona **Engines → Manage… → Add…**.
2. Pulsa **Browse…** y elige el ejecutable del motor (`sirio` en Linux/macOS, `sirio.exe` en
   Windows).
3. Opcional: añade argumentos adicionales o cambia el nombre que mostrará la GUI.
4. Tras crear el registro, selecciona el motor en la lista y pulsa **Configure…** para establecer las
   opciones UCI. Localiza `EvalFile`/`EvalFileSmall` y apunta a las redes deseadas.
5. Guarda los cambios con **OK**. El motor podrá usarse en torneos, partidas de prueba o análisis.

Cute Chess refresca los parámetros cada vez que lanza el motor y almacena las rutas de las redes en
su archivo de configuración, por lo que no necesitas reintroducirlas a menos que muevas los
archivos.

## Arena Chess GUI

1. Ve a **Motores → Instalar nuevo motor…** y selecciona el binario (`sirio`/`sirio.exe`).
2. Arena preguntará si se trata de un motor UCI; responde **Sí**.
3. El diálogo de opciones aparecerá automáticamente. En la pestaña **UCI** encontrarás los campos
   `EvalFile` y `EvalFileSmall`; establece las rutas correspondientes. Ajusta también `Threads`,
   `Hash` y cualquier otro parámetro que necesites.
4. Guarda la configuración. Arena creará un archivo `.uci` con las opciones elegidas para futuras
   sesiones.

Si Arena no muestra el diálogo, puedes acceder más tarde a través de **Motores → Administrar →
Configuración** y editar las opciones manualmente.

## Resolución de problemas

- **La GUI no muestra `EvalFile`:** asegúrate de lanzar el motor con el protocolo UCI (no WinBoard).
  SirioC anuncia las opciones inmediatamente después de recibir `uci` y termina con `uciok`. Las
  GUIs que soportan UCI deberían rellenar automáticamente la lista.
- **El motor no carga la red especificada:** verifica la ruta y los permisos del archivo. El motor
  devolverá `info string Failed to load NNUE` cuando no pueda abrirlo. Puedes revisar los mensajes
  en la consola o en el registro de depuración si activas la opción `Debug Log File`.
- **Fritz copia el ejecutable a otra carpeta:** en tal caso, copia también las redes a la ubicación
  que Fritz utilice o configura rutas absolutas para evitar errores de carga.

Siguiendo estos pasos, SirioC se comportará como cualquier otro motor UCI moderno dentro de las
GUIs mencionadas.
