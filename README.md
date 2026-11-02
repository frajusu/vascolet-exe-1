# Vascolet 1 — ejecutable original en el navegador

[Abrir juego](https://frajusu.github.io/vascolet-exe-1/)

La opción recomendada ejecuta **el programa original de XO con CheerpX**, con
dispositivos SDL conectados al navegador. También se conserva el EXE de Windows
con Boxedwine + Wine 11 como alternativa.
La pantalla está dedicada al juego, como los ports `-web`. Las opciones están
en el botón ☰. Los botones 1–4 cambian de Vascolet.

## Motores y carga

- **XO · CheerpX (experimental):** seleccionado por Automático cuando el navegador
  permite aislamiento. Prueba local dentro de La Prehistoria con movimiento:
  aproximadamente **58 FPS**, audio activo y arranque de 4–6 s / unos 2,3 s en local.
  Ver [método, evidencia y alcance](docs/CHEERPX.md).
- JIT multihilo de Wine con SharedArrayBuffer y OffscreenCanvas.
  Un service worker agrega COOP/COEP
  en GitHub Pages; requiere una recarga automática en la primera visita.
- JIT de un hilo como alternativa y fallback si falla la inicialización del
  multihilo. El menú permite seleccionar cualquiera.
- Arranque directo del EXE, sin consola `cmd` ni procesos `reg` en cada inicio.
- Wine y juego se descargan en paralelo y quedan en Cache Storage. La misma
  descarga Wine se comparte entre los cuatro sitios en este origen.
- Se conserva el entorno de Wine; no se destruye en cada reinicio.

La emulación sigue teniendo coste. No se promete arranque instantáneo ni 60 FPS.
Ver [mediciones e investigación](docs/PERFORMANCE.md).

## Controles en teléfonos

En pantallas táctiles aparecen flechas, **Saltar (Espacio)**, **Enter** y **Esc**.
Se pueden mantener varios botones a la vez y deslizar por la cruceta para
cambiar de dirección o usar diagonales. Tocá directamente el juego para elegir
opciones en sus menús. El menú ☰ permite activar u ocultar los controles.
En computadoras con ratón permanecen ocultos por defecto.

## Partidas

Los archivos escritos por el juego se conservan en IndexedDB,
con una clave distinta para cada Vascolet. `localStorage` guarda solamente las
preferencias: motor y sonido. IndexedDB evita el límite pequeño de localStorage
y permite guardar los archivos binarios originales.

Se sincroniza al cambiar de juego, al reiniciar, al ocultar la pestaña y
periódicamente. El menú permite exportar e importar una copia JSON de los
archivos modificados de este entorno, no un estado instantáneo de la RAM.
XO y Wine mantienen partidas separadas. El juego debe guardar primero su propio progreso. Borrar los datos del sitio
o usar otra sesión de navegador no conserva esas partidas.

## Desarrollo y publicación

```sh
python build.py
python -m http.server 8000 --directory dist
```

Python 3.11+, Internet para el primer build. Chrome/Edge o Firefox actualizado
en PC. La opción XO carga su paquete y CheerpX; no descarga Wine para jugar.
La alternativa Windows carga aproximadamente 130 MB la primera vez. Las
siguientes visitas usan la caché local. Se ha validado CheerpX en Chromium.

Actions genera `dist/` y publica GitHub Pages. Las descargas se verifican con
los hashes de `runtime.lock.json`. El pin Wine upstream contiene `latest`:
si cambia, el build falla por checksum y requiere revisar el pin.

## Originales y licencias

`game-manifest.json` registra los SHA-256 originales y dependencias adicionales.
No se modificaron los EXE. Se excluyeron desinstaladores y archivos ajenos;
Vascolet 3 incluía una carpeta con un instalador VPN.
Vascolet 1–3 requerían la OpenAL32.dll que se incluyó desde Vascolet 4.
El paquete del juego 4 está dividido en `.001` / `.002` y se concatena al
generar el sitio, sin alterar su contenido.

Boxedwine build 121, fuente `26bf258ab0dc00aa1ec6da81d1ca263f922f8947`,
GPL-2.0-or-later; ver `vendor/Boxedwine-COPYING.txt` y el
[código fuente correspondiente](https://github.com/danoon2/Boxedwine/tree/26bf258ab0dc00aa1ec6da81d1ca263f922f8947).
La capa de integración está en `site/runner.js`; se mantienen avisos de Wine y
Tiny Core dentro de la imagen. No se distribuye Microsoft Windows.
Vascolet y sus recursos pertenecen a sus autores originales.
