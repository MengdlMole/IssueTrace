# ADR 0005: Local workspace and export contract

- Status: accepted
- Date: 2026-09-10

## Context

The original prototype converted copied XLSX rows into fixed Obsidian issue directories. That flow proved useful as an experiment, but it made a team-specific spreadsheet schema and one note layout the center of the product. The actual user need is to capture incomplete problems quickly, append investigation notes and evidence over time, and produce a final summary.

The current product deliberately avoids user-defined schemas and an in-app summary editor. Program updates must also remain independent from user data.

## Decision

IssueTrace becomes a local-first issue journal with three primary domain concepts:

- issue;
- timeline entry and attachment;
- fixed Markdown/XLSX export projections.

Use SQLite with FTS5 as the authoritative store. Store attachment bytes in a user-selected workspace and keep relative paths in SQLite. Markdown and XLSX are exports rather than the live data store.

Issue fields have stable machine IDs. The built-in detail form, XLSX columns and summary variables reference those IDs. The bundled Markdown template supplies necessary context and leaves clearly marked sections for later editing in Obsidian; IssueTrace does not expose a template editor.

The program directory never contains the active database, user templates or attachments. Portable package replacement only changes application files.

## Consequences

- The main UI can create a problem with only a title and collect details later.
- Progress is derived from timeline entries instead of being manually duplicated.
- Search, attention reminders and export share one data source.
- Obsidian remains supported through Markdown export without constraining storage.
- SQLite migrations, backup and attachment consistency become required infrastructure.
- TSV/XLSX mapping and fixed-directory generation are removed from the product rather than retained as a legacy adapter.

## Alternatives rejected

### One Markdown directory per issue as the database

This is easy to inspect but makes atomic edits, dynamic form fields, cross-issue search, soft deletion and schema migration harder. Markdown remains an essential export and escape format.

### JSON files as the primary store

JSON reduces dependencies but requires the project to implement transactions, indexes, partial updates and recovery. SQLite provides these capabilities in-process with a small open-source footprint.

### Keep XLSX as the authoritative database

XLSX is valuable for exchange and team reporting but is unsuitable for frequent timeline writes, attachments, search and transactional local editing.
