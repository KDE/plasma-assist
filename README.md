# Plasma Assist

Assistant which helps to diagnose and fix common problems on KDE Plasma desktop
(no sound, external display, network, sleep, crashing applications).

## Components

- `plasma-assist-troubleshooter`: MCP server (stdio) which provides diagnostic tools
 (read only) and reversible actions.
- `plasma-assistd`: daemon (D-Bus) which hosts conversations, language model and
 MCP client (planned).
- Plasma applet: user interface (planned).

Diagnostic tools are read only. Every action which changes the system needs an explicit
confirmation and can be undone.

## Platform

Linux only for the moment.

## Dependencies

- Qt >= 6.10
- KDE Frameworks >= 6.30 (CoreAddons, I18n, DocTools optional)
- ktextaddons >= 2.2.52 (`KF6TextAutoGenerateTextMcpProtocolCore`)

## Build

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

## Test the MCP server

Add `plasma-assist-troubleshooter` as a stdio server in kmcpclientinspector.

## License

GPL-2.0-or-later, see `LICENSES/`.
