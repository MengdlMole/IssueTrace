# ADR 0006: Portable archives and offline upgrades

- Status: accepted
- Date: 2026-09-10
- Supersedes: ADR 0002, ADR 0003 and ADR 0004

## Context

IssueTrace must be ready to run after extraction on Windows, macOS and Linux. It does not need an installer, administrator access, online update feed or platform-specific self-extracting package. A user receives a complete newer archive through an existing channel and uses that archive to upgrade.

Application replacement must never remove the SQLite workspace, attachments, templates or exports. Extracting directly over a directory can leave obsolete runtime files, so blind archive overlay is not a sufficient update algorithm.

## Decision

Publish one complete portable archive per platform and architecture:

- Windows: `IssueTrace-<version>-windows-x86_64.zip`;
- macOS: `IssueTrace-<version>-macos-<arch>.zip` containing `IssueTrace.app`;
- Linux: `IssueTrace-<version>-linux-x86_64.tar.gz` containing a portable directory.

The same archive supports first use and offline upgrade. Do not publish an installer, NSIS self-extractor, AppImage delta, appcast or online update service.

Every archive contains:

- the complete application payload and runtime dependencies;
- `release-manifest.json` with version, platform, architecture and hashes of package-owned files;
- `LICENSE`, complete third-party notices and dependency/SBOM information;
- a short portable usage and upgrade guide.

User workspaces are never included in release archives. The application refuses to create a workspace inside an application bundle or package-owned runtime directory. A sibling or completely separate directory is allowed.

## First use

1. Verify the published archive SHA-256 when available.
2. Fully extract the archive to any writable directory.
3. Run `IssueTrace.exe`, `IssueTrace.app` or the Linux `IssueTrace` launcher.
4. Select or create a separate workspace directory.

No registry installation, administrator elevation, system runtime or package manager is required.

## Offline upgrade

Two workflows use the same full archive.

### Always-available manual workflow

1. Close IssueTrace completely.
2. Extract the new archive into a new sibling directory.
3. Start the new version and open the existing external workspace.
4. Confirm the version and workspace health, then remove the old application directory when desired.

This is the recovery path even when in-place upgrade is unavailable.

### In-application archive upgrade

The application exposes “从新版压缩包升级”. A separate updater helper performs the replacement because Windows cannot replace a running executable.

1. User selects a full release archive.
2. The application validates archive type, platform, architecture, version and every manifest hash.
3. The helper extracts to a temporary sibling directory on the same filesystem.
4. IssueTrace closes after confirming there are no pending writes.
5. The helper atomically renames the active application payload to a backup and the staged payload into place.
6. It starts the new version and waits for a startup health marker.
7. On success it retains at most one previous version until the user or retention policy removes it; on failure it restores the previous version.

The helper may only modify files declared package-owned by the installed and incoming manifests. It must reject an archive containing absolute paths, `..` traversal, links escaping the staging directory or duplicate normalized paths.

## Integrity and authenticity

- Internal per-file hashes detect incomplete extraction and tampering after a manifest was obtained.
- Releases publish an external SHA-256 file; signing can be added without changing the archive workflow.
- A manifest inside its own archive is not by itself proof of publisher identity, so the UI must not claim cryptographic publisher verification until signed metadata is implemented.
- IssueTrace rejects same-version packages and downgrades. The main application verifies and backs up the workspace before starting any accepted upgrade.

## Data migrations

Application payload replacement and workspace schema migration are separate transactions. On first open with a newer version, IssueTrace backs up the database, performs ordered migrations and records the new schema version. A failed migration restores the backup and does not trigger deletion of the previous application payload.

## Consequences

- Distribution and upgrades use the same artifact and no network service.
- Users can always recover by starting a newly extracted version against the same workspace.
- Release engineering maintains one archive path per platform instead of installers and multiple updater frameworks.
- An updater helper, manifest validator, archive extraction protection and rollback tests are required before advertising one-click in-place upgrade.
