# ADR 0001: Cross-platform desktop stack

- Status: accepted
- Date: 2026-08-27

> ADR 0005 replaces the original XLSX-to-Obsidian product boundary. This ADR
> remains accepted for the C++/Qt desktop stack; the current product boundary is
> defined by `docs/product-spec.md`.

## Context

IssueTrace is a desktop utility that must run on macOS, Linux and Windows. It needs
clipboard image access, native file and URI handling, forms and a C++ backend.

## Decision

Use:

- C++20 for domain logic, application services and platform adapters;
- Qt 6 Quick/QML and Qt Quick Controls for all new visible UI;
- CMake, CMake Presets and Ninja;
- open-source Qt Community modules only;
- platform-specific CI jobs and packages rather than cross-compiling all targets
  from one host.

Bootstrap against the public Qt Community 6.11 line. The first verified desktop
build uses 6.11.1 from the open-source Homebrew split formulae. Release builds
pin an exact tested patch and
advance it deliberately; the project must not silently build against whatever
Qt happens to be installed. Commercial Qt and commercial-only LTS patches are
outside the project policy.

## Layering

```text
QML / Qt Quick Controls
        |
QObject view-models and application services
        |
issuetrace_core (standard C++20)
        |
SQLite, filesystem and Markdown/XLSX export

Platform adapters:
  clipboard | tray | open URI | notifications
```

`issuetrace_core` remains Qt-free. This keeps parsing, mapping, validation and file
generation testable with Clang, GCC and MSVC without a GUI runtime.

## Why Qt Quick rather than Qt Widgets

Qt Widgets is mature and remains useful for established desktop applications,
but Qt recommends Qt Quick for new UI development. QML keeps the two-pane inbox,
timeline cards and attachment interactions concise. IssueTrace does not build a
Widgets main UI; it links Qt Widgets only for the system tray menu and a
platform-native Qt Quick Controls style may also use it.

## Alternatives considered

### Tauri 2 plus a web frontend

Tauri has strong packaging, tray and shortcut capabilities and usually produces
smaller bundles than Chromium-based shells. For IssueTrace it adds Rust, a web UI
toolchain and either a C ABI or sidecar/IPC boundary around the C++ core. Rich
clipboard handling would also span more layers. This complexity is not justified
for a small local C++ application.

### Flutter desktop

Flutter provides consistent UI on all three targets, but it adds Dart and an FFI
boundary to the C++ core. Tray, global shortcuts and detailed platform clipboard
behavior rely more heavily on plugins or platform channels. It is a better fit
when a shared mobile UI is a primary goal, which IssueTrace does not currently have.

### Electron

Electron has excellent desktop APIs and a large ecosystem, but includes a
Chromium/Node multi-process runtime and requires a native-addon or IPC boundary
for C++. Its runtime and update surface are disproportionate for this utility.

### Slint

Slint is attractive for modern declarative C++ UI and supports the three desktop
systems. Its desktop integration ecosystem is younger, so IssueTrace would own
more tray, shortcut and rich-clipboard platform work. Reconsider it if avoiding
Qt licensing becomes more important than ecosystem maturity.

## Cross-platform details

### macOS

- Clang/Xcode; build both Apple Silicon and Intel artifacts or a universal app.
- Produce an `.app` inside a complete portable ZIP; signing is a separate release-channel choice.
- Keep screen-capture and notification permissions explicit and user-visible.

### Windows

- MinGW-w64; initially target x86-64.
- Collect Qt runtime dependencies into a complete portable ZIP.
- Enable long-path awareness.

### Linux

- GCC on a conservative glibc baseline.
- Test both X11 and Wayland sessions.
- Provide a self-contained `tar.gz` portable directory and test its executable
  permissions and links after extraction.
- Treat tray availability and global shortcuts as capabilities, not assumptions.

## Licensing constraint

IssueTrace is distributed under `GPL-3.0-or-later` and uses only open-source Qt
Community modules. Every release includes corresponding source, license notices
and a dependency SBOM. Commercial-only Qt modules and services are prohibited.
See `docs/open-source-policy.md`. This ADR records an engineering policy and is
not legal advice.
