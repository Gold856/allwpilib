include(PlatformVars)
function(config_maven_build target)
    cmake_parse_arguments(artifact "" "CONFIG_FILE;SOURCE_DIR;ARTIFACT" "" ${ARGN})
    get_target_property(library_type ${target} TYPE)
    if(library_type STREQUAL SHARED_LIBRARY)
        set(lib_type shared)
    elseif(library_type STREQUAL EXECUTABLE)
        set(lib_type "")
    else()
        set(lib_type static)
    endif()
    set(library_dest ${artifact_ARTIFACT}/${platform}/${arch}/${lib_type})

    install(TARGETS ${target} EXPORT ${target} DESTINATION ${library_dest})
    if(library_type STREQUAL EXECUTABLE)
        return()
        # Nothing below applies to executables
    endif()

    # Exported targets always have a config file
    if(artifact_CONFIG_FILE)
        install(FILES ${WPILIB_BINARY_DIR}/${artifact_CONFIG_FILE} DESTINATION ${artifact_ARTIFACT})
        install(EXPORT ${target} DESTINATION ${artifact_ARTIFACT})
    endif()

    if(artifact_SOURCE_DIR)
        install(DIRECTORY ${artifact_SOURCE_DIR} DESTINATION sources/${artifact_ARTIFACT})
    else()
        install(DIRECTORY src/main/native/cpp/ DESTINATION sources/${artifact_ARTIFACT})
    endif()

    # If there are generated source files, install them
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/generated/main/native/cpp/)
        install(DIRECTORY src/generated/main/native/cpp/ DESTINATION sources/${artifact_ARTIFACT})
    endif()

    if(EXISTS ${CMAKE_CURRENT_BINARY_DIR}/generated/main/cpp/)
        install(
            DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated/main/cpp/
            DESTINATION sources/${artifact_ARTIFACT}
        )
    endif()
endfunction()
