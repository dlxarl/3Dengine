# Copies built-in resources next to an executable, where Resources.cpp finds them at
# runtime: <exe dir>/resources, or Contents/Resources in an app bundle. Runs on every
# build, so a shader edit does not need a relink.
#
#   engine_add_resources(target [EDITOR_SHADERS dir] [FONTS file...])
function(engine_add_resources target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "EDITOR_SHADERS" "FONTS")

    get_target_property(is_bundle ${target} MACOSX_BUNDLE)
    if (is_bundle)
        set(resource_dir "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources")
    else()
        set(resource_dir "$<TARGET_FILE_DIR:${target}>/resources")
    endif()

    set(commands
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${ENGINE_SHADER_DIR}" "${resource_dir}/shaders")

    if (ARG_EDITOR_SHADERS)
        list(APPEND commands
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${ARG_EDITOR_SHADERS}" "${resource_dir}/editor/shaders")
    endif()

    if (ARG_FONTS)
        list(APPEND commands
                COMMAND ${CMAKE_COMMAND} -E make_directory "${resource_dir}/fonts"
                COMMAND ${CMAKE_COMMAND} -E copy ${ARG_FONTS} "${resource_dir}/fonts/")
    endif()

    add_custom_target(${target}_resources ALL ${commands} VERBATIM)
    add_dependencies(${target} ${target}_resources)
endfunction()
