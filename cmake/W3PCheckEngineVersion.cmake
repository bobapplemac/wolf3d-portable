# The platform ABI alone does not describe additive public API requirements.
function(w3p_check_engine_version engine_version)
    if("${engine_version}" VERSION_LESS "1.4.57")
        message(FATAL_ERROR
            "wolf3d-lib ${engine_version} is too old: wolf3d-portable requires "
            "1.4.57 or newer for wolf3d_GetCommandLineHelp. Run the root build "
            "script and accept the engine update, resolving any Git access errors first.")
    endif()
endfunction()
