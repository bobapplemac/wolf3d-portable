# Project history

`wolf3d-portable` contains the operating-system hosts and release packaging for
the preservation-oriented
[`wolf3d-lib`](https://github.com/bobapplemac/wolf3d-lib) engine. The project
began as part of the original combined `wolf3dgeneric` development repository;
its existing chronology records the gradual construction of the engine and
reference hosts before commit `afc1eea` separated the reusable engine into its
own repository.

Unlike the library's curated public history, this repository retains its
original development chronology on `main`. The branch
`archive/gitlab-development-history` names the exact final private checkpoint,
commit `7b444e759696698a6da67234667ce014b384b750`, before the public GitHub
migration.

## Library checkpoint mapping

The private portable checkpoint recorded wolf3d-lib commit
`79cac7556771bb59638ac50183ccb3db3c17371e`. That exact library history remains
available from the library repository's own
`archive/gitlab-development-history` branch.

Public portable development instead pins curated wolf3d-lib commit
`593c5a0590ab810fdc84e0196ba49d6f6b63de84`. The engine, tests, build inputs,
and third-party sources at the two checkpoints are byte-for-byte identical.
The public checkpoint differs only in reviewed history, licensing, versioning,
and repository-link documentation.

Historical portable commits may reference earlier library gitlinks. They are
kept fetchable through the public library archive rather than rewritten, so
the portable repository's original commit identities and chronology remain
intact.

No commercial Wolfenstein 3D or Spear of Destiny game data is stored in either
repository. Users must provide compatible original data files separately.
