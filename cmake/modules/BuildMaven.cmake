include(PlatformVars)
macro(config_maven_build target)
    cmake_parse_arguments(artifact "" "CONFIG_FILE;SOURCE_DIR" "" ${ARGN})
    get_target_property(library_type ${target} TYPE)
    if(library_type STREQUAL SHARED_LIBRARY)
        set(lib_type shared)
    else()
        set(lib_type static)
    endif()
    set(library_dest ${PROJECT_NAME}/${platform}/${arch}/${lib_type})

    install(TARGETS ${target} EXPORT ${target} DESTINATION ${library_dest})
    # Exported targets always have a config file
    if(artifact_CONFIG_FILE)
        install(FILES ${WPILIB_BINARY_DIR}/${artifact_CONFIG_FILE} DESTINATION ${PROJECT_NAME})
        install(EXPORT ${target} DESTINATION ${PROJECT_NAME})
    endif()

    if(artifact_SOURCE_DIR)
        install(DIRECTORY ${artifact_SOURCE_DIR} DESTINATION sources/${PROJECT_NAME})
    else()
        install(DIRECTORY src/main/native/cpp/ DESTINATION sources/${PROJECT_NAME})
    endif()

    # If there are generated source files, install them
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/generated/main/native/cpp/)
        install(DIRECTORY src/generated/main/native/cpp/ DESTINATION sources/${PROJECT_NAME})
    endif()

    if(EXISTS ${CMAKE_CURRENT_BINARY_DIR}/generated/main/cpp/)
        install(
            DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated/main/cpp/
            DESTINATION sources/${PROJECT_NAME}
        )
    endif()
endmacro()
