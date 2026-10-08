if(NOT DEFINED PROBE OR NOT DEFINED ROOT)
    message(FATAL_ERROR "PROBE and ROOT are required")
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

file(REMOVE_RECURSE "${ROOT}")
