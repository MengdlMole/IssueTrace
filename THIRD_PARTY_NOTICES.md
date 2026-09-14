# Third-party notices

IssueTrace desktop development builds link to Qt Community Edition and the
open-source libraries used by the selected Qt build. Platform dependency
versions, licenses, upstream URLs and checksums are recorded in
`packaging/dependency-lock.*.json`.

The application and packaging toolchain use the following open-source projects:

- Qt Community Edition 6.11.1 — LGPL-3.0-only OR GPL-3.0-only
- libxlsxwriter 1.2.4 — BSD-2-Clause；内含 BSD-3-Clause 的 FreeBSD
  `queue.h`/`tree.h`、Zlib 许可的 minizip，以及 MPL-2.0 的 tmpfileplus；
  IssueTrace 构建关闭可选 MD5 实现
- SQLite 3.53.4 — Public Domain；目标环境没有 SQLite 开发库时从官方
  amalgamation 静态构建，并启用 FTS5
- zlib 1.3.2（系统缺失时静态构建；否则使用系统或目标工具链版本）— Zlib
- CMake — BSD-3-Clause
- Ninja — Apache-2.0

The Windows x86_64 development bundle uses Qt Community Edition 6.11.1 and the
MinGW-w64/GCC runtime. It
deliberately excludes Qt WebEngine, Qt Quick 3D and
the optional proprietary D3D compiler and Mesa packages. Windows operating
system DLLs are platform components and are not redistributed by IssueTrace.

This file is a release gate: a packaging job must fail if the dependency lock and
notices do not agree.

The native macOS archive and cross-built Windows development ZIP include this
notice, the IssueTrace GPL license, a platform dependency lock, a CycloneDX SBOM
and the complete libxlsxwriter license text. Qt is used under its GPL-3.0 option;
the bundled GPL-3.0 text is therefore also the applicable Qt license. Platform
runtime notices and public source locations are recorded in the dependency lock.
