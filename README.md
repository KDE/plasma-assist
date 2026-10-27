# Plasma Assist

Assistant which helps to diagnose and fix common problems on KDE Plasma desktop
(no sound, external display, network, sleep, crashing applications).

## Components

- `plasma-assist-troubleshooter`: MCP server (stdio) which provides diagnostic
 tools (read only), reversible actions with a journal of changes, guided
 scenarios (MCP prompts), a system report and a bug report tool. Personal data
 are masked in results.
- `plasma-assistd`: daemon (D-Bus service `org.kde.plasma.assist`, started on
 demand) which hosts conversations with the language model (ktextaddons) and
 asks user to confirm actions.
- `org.kde.plasma.assist`: Plasma applet, user interface.

Diagnostic tools are read only. Every action which changes the system needs an
explicit confirmation and can be undone. Actions are disabled by default.

## Platform

Linux only for the moment.

## Dependencies

- Qt >= 6.10 (Core, Concurrent, DBus, Gui, Network, Quick, Widgets)
- KDE Frameworks >= 6.30 (Config, CoreAddons, I18n, NetworkManagerQt,
 Notifications, Solid, DocTools optional)
- Plasma >= 6.5 (libplasma, libkscreen), PulseAudioQt >= 1.6
- ktextaddons >= 2.2.57 (`KF6TextAutoGenerateText`,
 `KF6TextAutoGenerateTextMcpProtocolCore`)

## Build

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

## Run

```bash
cmake --install build-dev
plasmoidviewer -a org.kde.plasma.assist
```

The daemon is started by D-Bus activation. A language model must be configured
(button *Configure Language Model…* of the applet). Remote language models are
refused by default: use a local model (Ollama, llama.cpp, LM Studio), or set
`AllowRemoteLanguageModel=true` in group `[General]` of `plasma-assistrc`.

The D-Bus API can be used without the applet:

```bash
SERVICE="org.kde.plasma.assist /Assistant org.kde.plasma.assist.Assistant"
busctl --user call $SERVICE Status
busctl --user call $SERVICE CallTool ss list_audio_devices '{}'
busctl --user call $SERVICE QuickDiagnostic s no_sound
```

## Test the MCP server

Add `plasma-assist-troubleshooter` as a stdio server in kmcpclientinspector.

## License

GPL-2.0-or-later, see `LICENSES/`.
