# Build identity

wolf3d-portable is a rolling source project on main, without a separate portable
release version. The version in a package folder identifies the selected
wolf3d-lib engine. The full naming vocabulary is maintained in
[BUILD-NAMING.md](BUILD-NAMING.md); package contents are defined in
[DISTRIBUTION-CONTENTS.md](DISTRIBUTION-CONTENTS.md).

DOCS/BUILD.TXT records the actual portable and engine revisions plus every
configured build option. The folder name alone does not identify exact sources.

The committed engine gitlink is a reproducible starting point. Guided build
scripts offer newer compatible engine main revisions with user confirmation;
direct build commands use the selected checkout. Engine bug fixes do not require
a portable gitlink update before users can accept them. See
[source-updates.md](source-updates.md) for update checks, local changes, offline
builds and unattended operation.
