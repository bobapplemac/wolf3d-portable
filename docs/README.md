# Documentation

Start with the [project README](../README.md). This index covers maintained guides,
reference evidence and preserved historical material.

## Build and use

- [building](building.md) — Build requirements, commands, IDEs and toolchain options.
- [source-updates](source-updates.md) — Confirmed source updates, dependency selection and offline builds.
- [running](running.md) — Game data, launch options, configuration precedence, input and audio.
- [support-matrix](support-matrix.md) — Supported build/runtime matrix and validation limits.
- [build-identity](build-identity.md) — Portable/engine version identity and reproducibility.

## Develop and package

- [maintenance](maintenance.md) — Generated/mirrored file ownership and reference-audit findings.

- [repository-layout](repository-layout.md) — Source directory ownership and file-placement rules.
- [BUILD-NAMING](BUILD-NAMING.md) — Shared package/build identity vocabulary.
- [DISTRIBUTION-CONTENTS](DISTRIBUTION-CONTENTS.md) — Shared binary-package file layout and extension rules.

## History and evidence

- [history](history.md) — Project and repository lineage.

## Nearby documentation

- [Build-script architecture](../scripts/README.md)
- [Linux dispatcher](../scripts/linux/README.md)
- [Visual Studio projects](../ide/visual-studio/README.md)
- [Open Watcom workspace](../ide/open-watcom/README.md)
- [Third-party notices](../THIRD_PARTY.md)
- [Changelog](../CHANGELOG.md)

## Documentation ownership

The root README is an introduction and quick start. Building owns commands;
source-updates owns Git behavior; support-matrix owns current platform status.
Repository-layout describes tracked source; BUILD-NAMING and
DISTRIBUTION-CONTENTS describe generated output and are mirrored in both repos.
Detailed developer evidence may retain historical checkpoints, explicitly labelled.
Update the owning guide and link it instead of copying its option tables elsewhere.

Run `python tools/WG_DOCS_AUDIT.py` for local Markdown link, anchor, table and
index coverage checks. Python is a maintainer tool, not a build dependency.
