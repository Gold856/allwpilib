include(PlatformVars)
include(DownloadAndCheck)
set(base_url https://frcmaven.wpi.edu/artifactory/release/edu/wpi/first/ni-libraries)
set(version 2024.2.1)
set(libs chipobject netcomm runtime visa)
set(dest ${WPILIB_BINARY_DIR}/ni-libraries)
message(STATUS "Downloading and unpacking NI libraries.")
foreach(lib ${libs})
    set(base_artifact_name ${lib}-${version})
    set(library_artifact ${base_artifact_name}-linuxathena)
    set(headers_artifact ${base_artifact_name}-headers)
    if(NOT NILibraries_FOUND)
        if(NOT EXISTS ${dest}/${library_artifact}.zip)
            download_and_check(
                ${base_url}/${lib}/${version}/${library_artifact}.zip
                ${dest}/${library_artifact}.zip
            )
        endif()
        if(NOT EXISTS ${dest}/${headers_artifact}.zip AND (NOT ${lib} STREQUAL runtime))
            download_and_check(
                ${base_url}/${lib}/${version}/${headers_artifact}.zip
                ${dest}/${headers_artifact}.zip
            )
        endif()
        file(ARCHIVE_EXTRACT INPUT ${dest}/${library_artifact}.zip DESTINATION ${dest}/${lib})
        if(NOT ${lib} STREQUAL runtime)
            file(
                ARCHIVE_EXTRACT
                INPUT ${dest}/${headers_artifact}.zip
                DESTINATION ${dest}/${lib}/include
            )
        endif()
    endif()
    file(GLOB ni_libs ${dest}/${lib}/linux/athena/shared/*)
    foreach(ni_lib ${ni_libs})
        cmake_path(GET ni_lib STEM lib_name)
        add_library(${lib_name} SHARED IMPORTED)
        if(NOT ${lib} STREQUAL runtime)
            target_include_directories(${lib_name} INTERFACE ${dest}/${lib}/include)
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
