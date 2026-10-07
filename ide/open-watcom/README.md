# Open Watcom DOS workspace

Run `open-ide.cmd` from a configured Open Watcom command prompt. The workspace
builds four targets from the same source files as the Docker release path:

1. `WOLF3D.lib`, from the pinned wolf3d-lib submodule;
2. independently replaceable `NUKEDOPL.lib`;
3. native hardware adapter `WGADLIB.lib`;
4. protected-mode `WOLF3D.exe` using the DOS/4G link format.

Copy DOS/32A's `DOS32A.EXE` beside the result as `DOS4GW.EXE` before running
it. The normal `make dos` package performs that staging automatically and
remains the reproducible release path.

For the supported Pentium baseline, choose **Pentium register-based calling**
for the C compiler in all four targets and select the IDE's release switch
set. Open Watcom otherwise defaults new DOS32 projects to Pentium Pro.

The project descriptors are generated state, not hand-edited text. Regenerate
them after changing target composition with:

```text
python tools\W3P_GENERATE_OPENWATCOM_IDE.py
```
