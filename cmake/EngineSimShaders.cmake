function(engine_sim_add_shader_artifacts target source output_directory)
    # Accept an explicit local build without requiring a PATH modification.
    # This is useful on Apple Silicon, where vcpkg does not package its DXC
    # dependency even though SDL_shadercross builds natively from source.
    set(ENGINE_SIM_SHADERCROSS_EXECUTABLE "" CACHE FILEPATH
        "Path to the SDL_shadercross offline compiler")
    if(NOT ENGINE_SIM_SHADERCROSS_EXECUTABLE)
        find_program(ENGINE_SIM_SHADERCROSS_EXECUTABLE NAMES shadercross
            HINTS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/sdl3-shadercross"
                  "$ENV{VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/tools/sdl3-shadercross")
    endif()
    if(NOT ENGINE_SIM_SHADERCROSS_EXECUTABLE)
        message(STATUS
            "shadercross was not found; ${target} will not generate SDL GPU shader artifacts. "
            "Set ENGINE_SIM_SHADERCROSS_EXECUTABLE to a locally built SDL_shadercross tool "
            "to enable the drawable desktop renderer.")
        return()
    endif()

    file(MAKE_DIRECTORY "${output_directory}")
    set(outputs)
    foreach(stage IN ITEMS vertex fragment)
        if(stage STREQUAL "vertex")
            set(entrypoint VSMain)
        else()
            set(entrypoint PSMain)
        endif()
        set(spirv "${output_directory}/engine_sim.${stage}.spv")
        set(msl "${output_directory}/engine_sim.${stage}.msl")
        set(dxil "${output_directory}/engine_sim.${stage}.dxil")

        add_custom_command(
            OUTPUT "${spirv}"
            COMMAND "${ENGINE_SIM_SHADERCROSS_EXECUTABLE}" "${source}"
                --source HLSL --dest SPIRV --stage "${stage}" --entrypoint "${entrypoint}"
                --output "${spirv}"
            DEPENDS "${source}"
            VERBATIM)
        add_custom_command(
            OUTPUT "${msl}"
            COMMAND "${ENGINE_SIM_SHADERCROSS_EXECUTABLE}" "${spirv}"
                --source SPIRV --dest MSL --stage "${stage}" --entrypoint "${entrypoint}"
                --output "${msl}"
            DEPENDS "${spirv}"
            VERBATIM)
        add_custom_command(
            OUTPUT "${dxil}"
            COMMAND "${ENGINE_SIM_SHADERCROSS_EXECUTABLE}" "${source}"
                --source HLSL --dest DXIL --stage "${stage}" --entrypoint "${entrypoint}"
                --output "${dxil}"
            DEPENDS "${source}"
            VERBATIM)
        list(APPEND outputs "${spirv}" "${msl}" "${dxil}")
    endforeach()

    # Keep the checked-in artifacts in assets/shaders in sync with the HLSL
    # source whenever the shader target is built.
    option(ENGINE_SIM_UPDATE_SHADER_ARTIFACTS
        "Copy regenerated SDL GPU shader artifacts into assets/shaders" ON)
    add_custom_target(${target} ALL DEPENDS ${outputs}
        COMMENT "Generating SDL GPU shader artifacts")
    if(ENGINE_SIM_UPDATE_SHADER_ARTIFACTS)
        get_filename_component(source_directory "${source}" DIRECTORY)
        foreach(output IN LISTS outputs)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${output}" "${source_directory}"
                VERBATIM)
        endforeach()
    endif()
endfunction()
