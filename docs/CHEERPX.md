# XO original con CheerpX y dispositivos SDL del navegador

## Qué ejecuta

El ELF x86 original de `Vascolet1_MaquinadelTiempo.xo`. El SHA-256 del ELF y del
paquete fuente están registrados en `native/cheerpx/sources.json`. El código
del juego, sus mapas, sus reglas y sus recursos no se reescriben.

CheerpX ejecuta el código de Linux mediante su JIT x86 → WebAssembly. Un adaptador
SDL 1.2 reemplaza los dispositivos de vídeo, teclado, ratón y audio de Linux:
los sprites se dibujan con Canvas y el audio se reproduce con Web Audio. Las
operaciones gráficas se agrupan por cuadro para evitar miles de cruces de E/S
y mantener los controles HTML de la página responsivos.

No se inicia Wine, Direct3D ni un servidor X. El SDL nativo restante trabaja
con superficies de memoria y un dispositivo de vídeo dummy.

## Resultado verificado

Prueba local en Chromium y el mismo equipo con AMD Radeon RX 6500 XT:

| Medición | Resultado |
|---|---:|
| Arranque local, entorno nuevo | 3,8–6,0 s |
| Arranque local, entorno conservado | 2,2–2,3 s |
| Media dentro de La Prehistoria, con movimiento y salto | 58,0 FPS |
| Tiempo mediano entre cuadros | 17,1 ms |
| Percentil 95 de tiempo entre cuadros | 23,8 ms |
| Audio | AudioContext en ejecución, señal RMS distinta de cero |
| Guardado | Escritura, importación, reinicio y exportación verificados |

El benchmark comprueba primero la aparición del HUD del nivel. No utiliza una
pantalla de menú o de historia como sustituto de una partida. La medición de
movimiento duró unos 20 segundos. La primera descarga desde Internet puede
tardar más que la prueba local. No se ha certificado cada nivel ni cada equipo.

![Prueba real dentro de La Prehistoria](cheerpx-gameplay.png)

El reporte completo está en [cheerpx-benchmark.json](cheerpx-benchmark.json).

## Interfaz y partidas

Se usa la misma página y el mismo menú. `Automático` selecciona CheerpX en este
repositorio cuando el navegador tiene aislamiento de origen. También se puede
elegir `XO · CheerpX (experimental)` explícitamente. Los modos Wine se conservan
para comparar o recuperar una partida anterior de Windows.

Los datos XO viven en IndexedDB, en un espacio distinto del de Wine. El ejecutable
original guarda bajo `SUGAR_ACTIVITY_ROOT`. La copia JSON lleva un campo `engine`
para impedir que se importe accidentalmente en el motor de otra plataforma.

La integración también está habilitada en Vascolet 2–4. Cada repositorio
documenta sus propias mediciones; no se extrapolan estos resultados.

## Correcciones de noviembre de 2026

- Eventos SDL de 20 bytes, según la ABI i386, sin sobrescribir variables del juego.
- Invalidación de texturas al cambiar transparencia, modificar superficies o
  liberar memoria; identificadores de texturas tratados como enteros sin signo.
- Color key comparado sobre los canales RGB; el canal alfa sólo se usa cuando
  SDL_SRCALPHA está activo. Se respeta la precedencia del alfa por píxel.
- Recorte de blits y tratamiento de escrituras parciales del transporte.
- 104 comprobaciones de transparencias y reutilización de texturas aprobadas.
- Nueva prueba dentro de La Prehistoria: 58,03 FPS, mediana 16,92 ms, p95 24,14 ms;
  audio, reinicio e importación/exportación de archivos comprobados.

Estas comprobaciones cubren los errores corregidos y la ruta probada; no
certifican todavía todas las pantallas o un teléfono concreto.

### Menos memoria y trabajo de audio/transporte

Los sonidos ahora se descargan y decodifican al usarse, con una caché compartida
por URL y un objetivo de 32 MiB de buffers decodificados inactivos. Los buffers
en reproducción no se expulsan. Un sonido individual activo puede superar
ese objetivo; no es un límite absoluto de memoria del navegador.

En una comparación controlada al entrar a La Prehistoria, la versión anterior
decodificó dos buffers para el mismo recurso (28.800.000 bytes); la nueva
decodificó uno (14.400.000 bytes). La caché reutiliza ese buffer para efectos
que se repiten. Si una descarga o decodificación falla, se libera el canal del
juego en vez de dejarlo marcado como ocupado. Los nodos terminados se desconectan.

El transporte gráfico usa un buffer reutilizable que crece geométricamente,
en lugar de asignar y copiar todos los bytes pendientes en cada fragmento.
La prueba de un paquete fragmentado de 2 MiB sólo requirió cinco ampliaciones.
También se eliminó una asignación temporal por cada blit de sprites.

Pruebas posteriores: movimiento y salto a 58,01 FPS en PC, audio real sin errores,
importación/exportación y controles multitáctiles. **Todavía no se comprobó la
mejora de tirones o cortes de audio en el teléfono del usuario.**

## Archivos y reproducción

- `site/cheerpx/bundle.zip`: bibliotecas, ELF original, assets y adaptadores.
- `native/cheerpx/sdl_bridge.c`: dispositivos SDL del navegador.
- `native/cheerpx/native_init.c`: preparación del sistema de archivos y copias.
- `native/cheerpx/mixer_anchor.c`: resolución de los símbolos de SDL_mixer.
- `site/cheerpx/bridge.js`: vídeo, entrada y Web Audio en el navegador.
- `site/cheerpx/launch.js`: CheerpX, persistencia e integración con el menú.
- `native/cheerpx/rebuild.py`: recompilación con Zig 0.13.0.

`python build.py` prepara el sitio estático sin necesitar un compilador. Para
reconstruir los adaptadores, configurar `ZIG` y ejecutar
`python native/cheerpx/rebuild.py`. Las opciones de compilación y hashes están en
`native/cheerpx/sources.json`. Los avisos y enlaces al código fuente de las
bibliotecas de terceros están incluidos dentro del bundle, en `licenses/`.

CheerpX 1.2.8 se carga desde su CDN oficial; no se redistribuye su runtime.
Documentación: https://cheerpx.io/docs/overview
