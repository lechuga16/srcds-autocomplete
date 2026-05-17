# Notas Técnicas: Consola Dedicada de Source y Terminal Host

## Resumen

`srcds-autocomplete` mejora la experiencia de uso de la consola dedicada de SRCDS, pero no reemplaza ni controla completamente el terminal donde corre el proceso.

La distinción importante es esta:

- **Source / SRCDS** mantiene un buffer interno de entrada y salida para la consola dedicada.
- **El terminal host** (Linux tty, SSH, tmux, screen, VS Code terminal, consola de Windows, etc.) es quien decide cómo renderizar visualmente esa salida.

Por eso algunas capacidades son fiables dentro del motor y otras no.

## Qué Controla Realmente la Extensión

La extensión trabaja sobre la consola dedicada interna del motor, principalmente mediante:

- un detour sobre `CTextConsole::ReceiveTab`
- acceso a campos del buffer de entrada reconstruidos localmente en `CTextConsole`
- uso de interfaces públicas como:
  - `ICvar`
  - `ConCommand`
  - `ICommandLine`

Eso permite implementar de forma confiable:

- autocompletado por prefijo
- sugerencias de argumentos para comandos con `AutoCompleteSuggest()`
- impresión de tablas de sugerencias
- manipulación del buffer de entrada actual

## Qué No Controla la Extensión

La extensión **no** controla el comportamiento visual del emulador de terminal host.

Eso incluye:

- borrado real de scrollback
- limpieza uniforme de la pantalla
- posicionamiento visual fiable del cursor en todos los terminales
- diferencias entre:
  - consola Linux nativa
  - terminal remota por SSH
  - tmux / screen
  - terminal integrada de VS Code
  - consola de Windows

En otras palabras, SRCDS escribe salida; el terminal decide cómo mostrarla.

## `CTextConsole` y el SDK

El SDK público de L4D2 no expone una API estable para la implementación interna de la consola dedicada.

Sí expone tipos útiles para autocompletado:

- `ICvar`
- `ConCommand`
- `ConCommandBase`
- `AutoCompleteSuggest()`
- `ICommandLine`

Pero no expone públicamente:

- `CTextConsole`
- `CTextConsole::ReceiveTab`
- el layout del buffer de entrada

Por eso esta extensión usa un shim local mínimo en:

- `extension/include/textconsole.h`

Ese header es una reconstrucción deliberadamente acotada al layout de L4D2 necesario para la extensión.

## Implicancias Prácticas

### Features fiables

Estas son buenas candidatas para esta extensión:

- autocompletado por prefijo
- providers manuales (`sm`, `meta`, etc.)
- sugerencias de mapas
- sugerencias de plugins/extensiones por archivos reales
- clasificación visual de resultados (`cmd`, `cvar`, `manual`)

### Features poco fiables

Estas son malas candidatas o solo pueden ofrecerse como *best effort*:

- `clear` de consola
- control de scrollback del terminal
- manipulación visual avanzada del cursor
- limpieza total compatible con todos los terminales

Por esa razón, features como `clear` fueron descartadas del alcance principal de la extensión.

## Sobre ANSI y Limpieza de Consola

Enviar secuencias ANSI como:

```text
\033[2J
\033[H
\033[3J
```

puede funcionar en algunos terminales, pero eso no depende del motor Source sino del terminal host.

Entonces:

- si el terminal interpreta ANSI, puede parecer que “funciona”
- si no lo interpreta igual, la limpieza será parcial o nula

Eso no es un bug del autocompletado; es una limitación natural del entorno.

## Conclusión

La consola dedicada de Source debe entenderse en dos capas:

1. **capa interna del motor**
   - buffer de entrada
   - `ReceiveTab`
   - comandos/cvars
   - sugerencias

2. **capa del terminal host**
   - render visual
   - limpieza de pantalla
   - scrollback
   - comportamiento ANSI

`srcds-autocomplete` debe enfocarse en la primera capa, donde el comportamiento es estable y controlable, y evitar prometer demasiado en la segunda.

