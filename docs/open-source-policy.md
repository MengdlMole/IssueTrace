# Open-source software policy

## Rule

IssueTrace source code, runtime dependencies, build tools, packaging tools and
update components must use OSI-approved open-source licenses compatible with
`GPL-3.0-or-later`.

Commercial-only modules, proprietary SDK libraries, telemetry SDKs and
proprietary hosted services that cannot be replaced or self-hosted are not
allowed.

## Platform exception

Building installable software for macOS requires Apple's SDK, code-signing and
notarization tools. Running and signing for Windows may require operating-system
SDK facilities. These target-platform requirements are permitted exceptions;
they must not become runtime product dependencies beyond what the operating
system already provides.

Windows builds use MinGW-w64 rather than MSVC. Windows Authenticode signing uses
an open-source signing client where compatible with the selected executable
format; certificates and the operating-system trust service are external release
credentials, not linked software dependencies.

## Approved baseline

| Component | Purpose | License |
|---|---|---|
| IssueTrace | Application | GPL-3.0-or-later |
| Qt Community | UI and OS integration | LGPL-3.0/GPL-3.0 |
| CMake | Build system | BSD-3-Clause |
| Ninja | Build executor | Apache-2.0 |
| GCC/MinGW-w64/LLVM | Compilers | GPL-compatible/Apache-2.0 |
| SQLite | Local workspace and FTS5 search | Public domain |
| libxlsxwriter | XLSX export | BSD-2-Clause |
| System tar (libarchive/bsdtar or GNU tar) | Offline archive listing and extraction | BSD-2-Clause/GPL-3.0-or-later |

IssueTrace's direct Qt dependencies are Qt Core, Gui, Qml, Quick, Quick Controls
2 and Widgets. Widgets is used only for the system tray menu; all visible main
UI remains QML. Native deployment may copy additional open-source transitive Qt
modules selected by the platform style. Such runtime modules are permitted only
when recorded by the release dependency evidence. Qt Test and Qt Quick Test are
build-only dependencies and must be removed from end-user archives.

The dependencies listed above are release dependencies and are recorded in
the platform dependency locks and generated CycloneDX SBOM.

GPL-only Qt add-on modules and Qt Marketplace components require an explicit ADR
before use. Qt WebEngine is prohibited because IssueTrace does not need a browser
runtime and its dependency/license surface is disproportionate.

## Dependency admission

Before adding or updating a dependency:

1. Record its exact version, upstream URL and SPDX license identifier.
2. Review direct and transitive licenses for GPL compatibility.
3. Pin a release tag or archive checksum; never depend on an unpinned branch.
4. Build it from public source or use a reproducible public binary with checksum.
5. Add its notices and source offer to `THIRD_PARTY_NOTICES.md`.
6. Confirm that the dependency is available on all affected target platforms.

No dependency is approved merely because its repository is publicly visible.
The license must explicitly grant open-source rights.

## Release evidence

Every release must publish or bundle:

- IssueTrace corresponding source for the exact binary release;
- `LICENSE` and `THIRD_PARTY_NOTICES.md`;
- an SPDX or CycloneDX software bill of materials;
- dependency versions and source archive checksums;
- checksums for every user-facing portable artifact and update package.
