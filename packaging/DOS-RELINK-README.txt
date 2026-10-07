Nuked-OPL3 relink materials
===========================

This directory contains the non-Nuked object code used to link the adjacent
DOS release, plus Nuked-OPL3 as a separate Open Watcom library. This permits a
recipient to replace or modify NUKEDOPL.LIB and relink the application as
required by LGPL-2.1-or-later.

Install the pinned Open Watcom v2 toolchain documented by wolf3d-lib, replace
NUKEDOPL.LIB if desired, change to this directory, and run:

    wlink @RELINK.LNK

The resulting WOLF3D.EXE uses the DOS/4G executable format and runs with the
DOS/32A loader from the parent distribution directory. WOLF3D.LIB contains
the GPL engine and the other selected OPL adapters; NUKEDOPL.LIB contains the
independently replaceable Nuked-OPL3 implementation.
