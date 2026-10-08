#!/usr/bin/env python3
"""Regenerate the DOS wrapper's Open Watcom IDE descriptors."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
IDE = ROOT / "ide" / "open-watcom"


def emit_string(lines, object_id, class_name, value):
    lines.extend((str(object_id), class_name, str(len(value)), value))


def write_target(path, output_name, rule_name, target_ident, sources, libraries):
    lines = ["39", "targetIdent", "0", "MProject", "1", "MComponent", "0"]
    emit_string(lines, 2, "WString", rule_name)
    emit_string(lines, 3, "WString", target_ident)
    lines.extend(("1", "0", "1", "4", "MCommand", "0", "5", "MCommand",
                  "0", "6", "MItem", str(len(output_name)), output_name))
    emit_string(lines, 7, "WString", rule_name)
    lines.extend(("8", "WVList", "0", "9", "WVList", "0", "-1", "1",
                  "1", "0", "10", "WPickList",
                  str(1 + len(sources) + (1 + len(libraries)
                                         if libraries else 0))))

    item_id = 11
    c_wildcard = item_id
    items = [("*.c", "COBJ", -1)]
    items.extend((source, "COBJ", c_wildcard) for source in sources)
    if libraries:
        lib_wildcard = item_id + len(items) * 4
        items.append(("*.lib", "NIL", -1))
        items.extend((library, "NIL", lib_wildcard) for library in libraries)

    for name, item_rule, parent in items:
        lines.extend((str(item_id), "MItem", str(len(name)), name))
        emit_string(lines, item_id + 1, "WString", item_rule)
        lines.extend((str(item_id + 2), "WVList", "0", str(item_id + 3),
                      "WVList", "0", str(parent), "1", "1", "0"))
        item_id += 4

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(("\n".join(lines) + "\n").encode("ascii"))


def write_project(path, targets):
    lines = ["39", "projectIdent", "0", "VpeMain", "1", "WRect", "0", "0",
             "7680", "10240", "2", "MProject", "3", "MCommand", "0", "4",
             "MCommand", "0", str(len(targets))]
    for object_id, target in enumerate(targets, 5):
        emit_string(lines, object_id, "WFileName", target)
    list_id = 5 + len(targets)
    lines.extend((str(list_id), "WVList", str(len(targets))))
    first_component = list_id + 1
    for index, target in enumerate(targets):
        component_id = first_component + index * 3
        lines.extend((str(component_id), "VComponent", str(component_id + 1),
                      "WRect", str((index % 2) * 3900),
                      str(120 + (index // 2) * 4000), "3750", "3880", "0",
                      "0"))
        emit_string(lines, component_id + 2, "WFileName", target)
        lines.extend(("0", "-1"))
    lines.append(str(first_component))
    path.write_bytes(("\n".join(lines) + "\n").encode("ascii"))


def main():
    write_target(
        IDE / "wgadlib" / "wgadlib.tgt", "WGADLIB.lib", "LIB", "d_2sn",
        ["..\\..\\..\\platforms\\dos\\WG_DOS_ADLIB.c"], [])
    write_target(
        IDE / "game" / "wolf3d-dos.tgt", "WOLF3D.exe", "EXE", "dr2en",
        ["..\\..\\..\\platforms\\dos\\WG_DOS.c",
         "..\\..\\..\\platforms\\dos\\WG_DOS_SB16.c"],
        ["..\\..\\..\\lib\\wolf3d\\ide\\open-watcom\\engine\\WOLF3D.lib",
         "..\\wgadlib\\WGADLIB.lib"])
    write_project(
        IDE / "wolf3d-portable.wpj",
        ["..\\..\\lib\\wolf3d\\ide\\open-watcom\\engine\\wolf3d-lib.tgt",
         "wgadlib\\wgadlib.tgt", "game\\wolf3d-dos.tgt"])


if __name__ == "__main__":
    main()
