# Build System

Este repositorio usa el flujo homologado de AoC para proyectos de solo-extensión.

## Flujo

- Linux:
  1. `make deps-exts-linux`
  2. `make build-exts-linux`
  3. `make package-exts-linux`
  4. `make release-linux`

- Windows:
  1. `make deps-exts-windows`
  2. `make build-exts-windows`
  3. `make package-exts-windows`
  4. `make release-windows`

## Targets

- `deps-exts-*`: descargan SDKs y toolchains nativos.
- `build-exts-*`: construyen la extensión con AMBuild.
- `package-exts-*`: arman el árbol intermedio del paquete.
- `release-*`: generan el artifact final y el ZIP en `dist/release/`.

## Manifiesto

El archivo [plugin-package-map.json](../plugin-package-map.json) define:

- qué extensión se construye
- qué archivos runtime adicionales se publican

En este repo eso cubre:

- `autocomplete.ext.2.l4d2.so` o `.dll`
- `addons/sourcemod/extensions/autocomplete.autoload`
- `addons/sourcemod/gamedata/autocomplete.games.txt`
