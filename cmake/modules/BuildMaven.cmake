if(MSVC)
    set(platform windows)
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64")
        set(arch x86-64)
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM64")
        set(arch arm64)
    else()
        set(arch x86)
    endif()
elseif(APPLE)
    set(platform osx)
    set(arch universal)
elseif(LINUX)
    set(platform linux)
    if(ATHENA)
        set(arch athena)
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set(arch x86-64)
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        set(arch arm64)
    else()
        set(arch arm32)
    endif()
endif()
macro(config_maven_build target)
    cmake_parse_arguments(artifact "" "CONFIG_FILE" "" ${ARGN})
    get_target_property(library_type ${target} TYPE)
    if(library_type STREQUAL SHARED_LIBRARY)
        set(lib_type shared)
    else()
        set(lib_type static)
    endif()
    set(library_dest ${PROJECT_NAME}/${platform}/${arch}/${lib_type})
    install(TARGETS ${target} EXPORT ${target} DESTINATION ${library_dest})
    if(artifact_CONFIG_FILE)
        install(
            FILES ${WPILIB_BINARY_DIR}/${artifact_CONFIG_FILE}
            DESTINATION ${PROJECT_NAME}
        )
        install(EXPORT ${target} DESTINATION ${PROJECT_NAME})
    endif()
endmacro()
macro(create_maven_archive)
    cmake_parse_arguments(artifact "" "" "FILES" ${ARGN})
    get_target_property(library_type ${target} TYPE)
    if(NOT library_type STREQUAL SHARED_LIBRARY)
        set(static static)
    endif()
    file(MAKE_DIRECTORY ${WPILIB_BINARY_DIR}/maven)
    file(
        ARCHIVE_CREATE
        OUTPUT
            $<INSTALL_PREFIX>/maven/${PROJECT_NAME}-cpp-2024.5.0-${platform}${arch}${static}$<$<STREQUAL:CONFIG,Debug>:debug>.zip
        PATHS
            $<INSTALL_PREFIX>/${PROJECT_NAME}/${platform}
            $<INSTALL_PREFIX>/LICENSE.md
            $<INSTALL_PREFIX>/ThirdPartyNotices.txt
            ${artifact_FILES}
        FORMAT zip
    )
endmacro()
