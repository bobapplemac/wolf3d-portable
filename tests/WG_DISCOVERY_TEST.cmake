if(NOT DEFINED PROBE OR NOT DEFINED ROOT OR NOT DEFINED CONFIG_EXTENSION)
    message(FATAL_ERROR "PROBE, ROOT, and CONFIG_EXTENSION are required")
endif()

file(REMOVE_RECURSE "${ROOT}")

function(make_profile directory map_extension resource_extension)
    file(MAKE_DIRECTORY "${directory}")
    foreach(base GAMEMAPS MAPHEAD VSWAP)
        file(WRITE "${directory}/${base}.${map_extension}" "test")
    endforeach()
    foreach(base VGADICT VGAGRAPH VGAHEAD AUDIOHED AUDIOT)
        file(WRITE "${directory}/${base}.${resource_extension}" "test")
    endforeach()
endfunction()

function(expect_selection label root program expected_path expected_extension)
    execute_process(
        COMMAND "${PROBE}" "${root}" "${program}" ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error)
    file(TO_CMAKE_PATH "${expected_path}" expected_normalized)
    string(REPLACE "\\" "/" output "${output}")
    string(STRIP "${output}" output)
    set(expected "${expected_normalized}\n${expected_extension}")
    if(NOT result EQUAL 0 OR NOT output STREQUAL expected)
        message(FATAL_ERROR
            "${label}: expected '${expected}', got result ${result}, "
            "output '${output}', error '${error}'")
    endif()
endfunction()

set(wolf_root "${ROOT}/wolf-preference")
make_profile("${wolf_root}/GAMEDATA/WL1" WL1 WL1)
make_profile("${wolf_root}/GAMEDATA/deep/WL6" WL6 WL6)
make_profile("${wolf_root}/GAMEDATA/SOD" SOD SOD)
expect_selection("wolf prefers full Wolf3D" "${wolf_root}" wolf.exe
    "${wolf_root}/GAMEDATA/deep/WL6" WL6)
expect_selection("explicit shorthand" "${wolf_root}" wolf.exe
    "${wolf_root}/GAMEDATA/WL1" WL1 -WL1)

set(spear_root "${ROOT}/spear-preference")
make_profile("${spear_root}/data/SDM" SDM SDM)
make_profile("${spear_root}/data/SOD" SOD SOD)
make_profile("${spear_root}/data/WL6" WL6 WL6)
expect_selection("spear prefers full Spear" "${spear_root}" SPEAR.EXE
    "${spear_root}/data/SOD" SOD)

set(wolf_fallback "${ROOT}/wolf-fallback")
make_profile("${wolf_fallback}/nested/SDM" SDM SDM)
expect_selection("wolf falls back to Spear" "${wolf_fallback}" WOLF.EXE
    "${wolf_fallback}/nested/SDM" SDM)

set(spear_fallback "${ROOT}/spear-fallback")
make_profile("${spear_fallback}/nested/WL1" WL1 WL1)
expect_selection("spear falls back to Wolf3D" "${spear_fallback}" SPEAR.EXE
    "${spear_fallback}/nested/WL1" WL1)

set(neutral_root "${ROOT}/neutral")
make_profile("${neutral_root}/SOD" SOD SOD)
make_profile("${neutral_root}/WL6" WL6 WL6)
expect_selection("sod is not a filename hint" "${neutral_root}" SOD.EXE
    "${neutral_root}/WL6" WL6)
expect_selection("nonexact spear is neutral" "${neutral_root}" spear-test.exe
    "${neutral_root}/WL6" WL6)

set(mission_root "${ROOT}/mission")
make_profile("${mission_root}/packs/SD2" SD2 SOD)
expect_selection("mission requires explicit selection" "${mission_root}" SPEAR.EXE
    "${mission_root}/packs/SD2" SD2 -SD2)

set(config_root "${ROOT}/config")
make_profile("${config_root}/data/WL1" WL1 WL1)
make_profile("${config_root}/data/WL6" WL6 WL6)
make_profile("${config_root}/data/SOD" SOD SOD)
file(WRITE "${config_root}/wolf.${CONFIG_EXTENSION}"
    "# Shared defaults\n--game WL1\n--fullscreen\n")
expect_selection("normalized config fallback" "${config_root}" wolf-sdl3.exe
    "${config_root}/data/WL1" WL1)
file(WRITE "${config_root}/wolf-sdl3.${CONFIG_EXTENSION}"
    "; Wrapper-specific defaults\n--game SOD\n")
expect_selection("exact config takes priority" "${config_root}" wolf-sdl3.exe
    "${config_root}/data/SOD" SOD)
expect_selection("command line overrides config family" "${config_root}"
    wolf-sdl3.exe "${config_root}/data/WL6" WL6 -WL6)
expect_selection("no-config bypasses defaults" "${config_root}" wolf-sdl3.exe
    "${config_root}/data/WL6" WL6 --no-config)
file(WRITE "${config_root}/custom.args" "--game WL1\n")
expect_selection("explicit config path" "${config_root}" wolf-sdl3.exe
    "${config_root}/data/WL1" WL1 --config "${config_root}/custom.args")

set(relative_root "${ROOT}/relative-config")
make_profile("${relative_root}/Game Data/WL1" WL1 WL1)
file(WRITE "${relative_root}/wolf.${CONFIG_EXTENSION}"
    "--data \"Game Data/WL1\"\n--game WL1\n")
expect_selection("config paths are relative to the config file"
    "${relative_root}" wolf.exe "${relative_root}/Game Data/WL1" WL1)

set(ambiguous_root "${ROOT}/ambiguous")
make_profile("${ambiguous_root}/one" WL6 WL6)
make_profile("${ambiguous_root}/two" WL6 WL6)
execute_process(
    COMMAND "${PROBE}" "${ambiguous_root}" wolf.exe
    RESULT_VARIABLE ambiguous_result
    OUTPUT_VARIABLE ambiguous_output
    ERROR_VARIABLE ambiguous_error)
if(NOT ambiguous_result EQUAL 2
   OR NOT ambiguous_error MATCHES "Multiple valid WL6 data directories"
   OR NOT ambiguous_error MATCHES "/one"
   OR NOT ambiguous_error MATCHES "/two")
    message(FATAL_ERROR
        "ambiguity reporting failed: result ${ambiguous_result}, "
        "output '${ambiguous_output}', error '${ambiguous_error}'")
endif()

# Input and sound switches describe hardware. CLI replaces only its own
# launcher-default family, regardless of the DOS spelling accepted by the engine.
function(expect_options label defaults expected)
    file(WRITE "${config_root}/wolf.${CONFIG_EXTENSION}" "${defaults}\n")
    execute_process(COMMAND "${PROBE}" "${config_root}" wolf.exe --diag ${ARGN}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    string(REPLACE "\r" "" output "${output}")
    string(STRIP "${output}" output)
    if(NOT result EQUAL 0 OR NOT output STREQUAL expected)
        message(FATAL_ERROR "${label}: '${output}', expected '${expected}': ${error}")
    endif()
endfunction()

expect_options("preserve defaults" "--nomouse\n--nojoy\n--no-sound"
    "--nomouse\n--nojoy\n--no-sound")
expect_options("force presence replaces absence" "--nomouse\n--nojoy\n--no-sound"
    "--no-sound\n--mouse\n--joy" --mouse --joy)
expect_options("suppress presence" "--mouse\n--joy\n--adlib"
    "--adlib\n--nomouse\n--nojoy" --nomouse --nojoy)
foreach(sound --adlib --pc-speaker --no-sound -noal -NoSb /NOAL /NOSB noal --nosb)
    expect_options("CLI sound ${sound}" "--mouse\n--nojoy\n--pc-speaker"
        "--mouse\n--nojoy\n${sound}" "${sound}")
    if(sound MATCHES "^-")
        expect_options("config sound ${sound}" "${sound}\n--nomouse"
            "--nomouse\n--adlib" --adlib)
    endif()
endforeach()
expect_options("OPL is independent" "--no-sound\n--opl silent"
    "--no-sound\n--opl\nnuked" --opl nuked)
expect_options("no-config bypasses hardware defaults" "--nomouse\n--nojoy\n--no-sound"
    "--no-config\n--mouse" --no-config --mouse)

file(REMOVE_RECURSE "${ROOT}")
