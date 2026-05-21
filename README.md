# SRCDS Autocomplete para L4D2

`srcds-autocomplete` es una extensión nativa de SourceMod que agrega autocompletado con `Tab` a la entrada de consola `-console` usada por servidores dedicados de Left 4 Dead 2.

La extensión intercepta la ruta de entrada de la consola dedicada y entrega:

- autocompletado por prefijo para comandos y cvars
- sugerencias de argumentos para comandos que exponen `AutoCompleteSuggest()`
- salida en tabla cuando existen múltiples coincidencias
- providers manuales para ramas comunes de comandos `sm` y `meta`
- sugerencias de archivos reales del servidor para carga/recarga de plugins y extensiones

Este repositorio se mantiene de forma intencional como una versión **solo para L4D2**.

## Qué Resuelve

El uso de la consola vanilla de SRCDS es incómodo para trabajo operativo. Esta extensión mejora:

- el descubrimiento de comandos al trabajar directamente en la terminal del servidor
- el autocompletado de comandos largos del motor o de plugins
- las sugerencias de argumentos para comandos como `changelevel`
- la navegación de ramas comunes de SourceMod y MetaMod

Ejemplos:

```text
statu<Tab>           -> status
changelev<Tab>       -> changelevel
changelevel <Tab>    -> sugerencias de mapas
sm e<Tab>            -> sm exts
sm exts l<Tab>       -> sm exts list
sm plugins lo<Tab>   -> sm plugins load
sm exts lo<Tab>      -> sm exts load
meta ve<Tab>         -> meta version
```

## Alcance

Este repositorio apunta deliberadamente a:

- `Left 4 Dead 2`
- SourceMod `1.12`
- MetaMod:Source `1.12`
- servidores dedicados Linux y Windows

Los caminos legacy multi-engine del proyecto original fueron removidos para reducir costo de mantenimiento y dejar la extensión alineada con el target real de despliegue.

## Instalación

Debes desplegar estos archivos en el servidor:

- `addons/sourcemod/extensions/autocomplete.ext.so` o `autocomplete.ext.dll`
- `addons/sourcemod/extensions/autocomplete.autoload`
- `addons/sourcemod/gamedata/autocomplete.games.txt`

Notas:

- la extensión está pensada para uso en consola del servidor dedicado
- `autocomplete.autoload` debe permanecer junto a la extensión para que SourceMod la cargue automáticamente

## Depuración

La extensión expone las siguientes `ConVar`:

```text
sm_autocomplete_debug
sm_autocomplete_max_results
```

`sm_autocomplete_debug`

- `0` = desactivado
- `1` = escribe información de depuración en los logs de SourceMod

Ejemplo:

```text
sm_cvar sm_autocomplete_debug 1
```

Cuando está activo, la extensión registra:

- la línea de entrada actual
- la cantidad de coincidencias encontradas
- los primeros resultados encontrados
- si tomó la ruta de match único o múltiples matches

`sm_autocomplete_max_results`

- define el máximo de sugerencias que se mostrarán en una tabla
- valor por defecto: `32`
- rango: `1` a `256`

Ejemplo:

```text
sm_cvar sm_autocomplete_max_results 16
```

## Providers Manuales

Además del autocompletado normal del motor, la extensión agrega soporte manual para ramas comunes de comandos.

### SourceMod

Primer nivel:

- `sm credits`
- `sm cvars`
- `sm dump_handles`
- `sm exts`
- `sm info`
- `sm plugins`
- `sm profiler`
- `sm reload_translations`
- `sm version`

Segundo nivel:

- `sm exts info`
- `sm exts list`
- `sm exts load`
- `sm exts reload`
- `sm exts unload`
- `sm plugins info`
- `sm plugins list`
- `sm plugins load`
- `sm plugins reload`
- `sm plugins unload`

### MetaMod

- `meta clear`
- `meta credits`
- `meta game`
- `meta list`
- `meta load`
- `meta pause`
- `meta refresh`
- `meta retry`
- `meta unload`
- `meta unpause`
- `meta version`

## Sugerencias de Archivos

La extensión también puede sugerir archivos reales del servidor en estos contextos:

- `sm plugins load`
- `sm plugins reload`
- `sm plugins unload`
- `sm exts load`
- `sm exts reload`
- `sm exts unload`

Resolución actual:

- plugins:
  - `addons/sourcemod/plugins`
  - extensión `.smx`
- extensiones:
  - `addons/sourcemod/extensions`
  - `.so` en Linux
  - `.dll` en Windows

## Estructura del Repositorio

- `extension/src/` implementación de la extensión nativa
- `extension/include/` headers locales y shims del motor
- `extension/config/` header de configuración de la extensión SourceMod
- `gamedata/` firmas y símbolos del juego
- `extension/` también contiene `autocomplete.autoload`, ya que pertenece canónicamente a la extensión
- `scripts/` helpers de bootstrap de dependencias y build
- `docs/` notas del repositorio y documentación del alcance L4D2

## Dependencias

Este repositorio usa scripts locales de bootstrap en `.deps/` para:

- `hl2sdk-l4d2`
- `sourcemod-1.12`
- `mmsource-1.12`
- `AMBuild`

Descarga las dependencias con:

- Linux / WSL:
  - `make deps-linux`
- Windows:
  - `make deps-windows`

## Compilación

Comandos de build:

- Linux / WSL:
  - `make build-linux`
- Windows:
  - `make build-windows`

La salida se genera en:

- `.build/linux-l4d2/`
- `.build/windows-l4d2/`

## Packaging y releases

Comandos de empaquetado:

- Linux / WSL:
  - `make package-exts-linux`
  - `make release-linux`
- Windows:
  - `make package-exts-windows`
  - `make release-windows`

Artifacts finales:

- `dist/release/srcds-autocomplete-local-linux.zip`
- `dist/release/srcds-autocomplete-local-windows.zip`

El contenido publicado se describe en [plugin-package-map.json](C:\GitHub\srcds-autocomplete\plugin-package-map.json) y el flujo completo está resumido en [docs/BUILD_SYSTEM.md](C:\GitHub\srcds-autocomplete\docs\BUILD_SYSTEM.md).

## Notas Técnicas

La extensión depende de:

- interfaces públicas del motor como `ICvar`, `ConCommand` e `ICommandLine`
- un shim local mínimo de `CTextConsole` para L4D2
- un detour sobre `CTextConsole::ReceiveTab`

El SDK no expone públicamente la implementación de la consola dedicada, por lo que el layout de `CTextConsole` usado aquí es una reconstrucción local y deliberadamente acotada para L4D2.

## Créditos

Este repositorio está basado en el proyecto original de peace-maker:

- [peace-maker/srcds-autocomplete](https://github.com/peace-maker/srcds-autocomplete)

También se conserva el crédito histórico del proyecto original a:

- Didrole, por ModuleScanner y el trabajo relacionado con SourceCurses
