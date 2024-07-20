include(FetchContent)
include(PlatformVars)
set(base_url https://frcmaven.wpi.edu/artifactory/release/edu/wpi/first/ni-libraries)
set(version 2024.2.1)
set(libs chipobject netcomm runtime visa)
set(dest ${WPILIB_BINARY_DIR}/ni-libraries)
message(STATUS "Downloading and unpacking NI libraries.")
foreach(lib ${libs})
    set(base_artifact_name ${lib}-${version})
    set(dest ${WPILIB_BINARY_DIR}/ni-libraries/${lib})
    fetchcontent_declare(
        ${lib}
        URL ${base_url}/${lib}/${version}/${base_artifact_name}-linuxathena.zip
        DOWNLOAD_NO_EXTRACT true
    )
    if(NOT ${lib} STREQUAL runtime)
        fetchcontent_declare(
            ${lib}_headers
            URL ${base_url}/${lib}/${version}/${base_artifact_name}-headers.zip
            DOWNLOAD_NO_EXTRACT true
        )
        fetchcontent_makeavailable(${lib}_headers)
        file(
            ARCHIVE_EXTRACT
            INPUT ${${lib}_headers_SOURCE_DIR}/${base_artifact_name}-headers.zip
            DESTINATION ${dest}/include
        )
    endif()
    fetchcontent_makeavailable(${lib})
    file(
        ARCHIVE_EXTRACT
        INPUT ${${lib}_SOURCE_DIR}/${base_artifact_name}-linuxathena.zip
        DESTINATION ${dest}
    )

    file(GLOB ni_libs ${dest}/linux/athena/shared/*)
    foreach(ni_lib ${ni_libs})
        cmake_path(GET ni_lib STEM lib_name)
        add_library(${lib_name} SHARED IMPORTED)
        if(NOT ${lib} STREQUAL runtime)
            target_include_directories(${lib_name} INTERFACE ${dest}/include)
        endif()
        set_target_properties(
            ${lib_name}
            PROPERTIES IMPORTED_LOCATION_DEBUG ${ni_lib} IMPORTED_LOCATION_RELWITHDEBINFO ${ni_lib}
        )
        list(APPEND NI_LIBRARIES ${lib_name})
    endforeach()
endforeach()
message(STATUS "Done.")
set(NILibraries_FOUND true PARENT_SCOPE)
