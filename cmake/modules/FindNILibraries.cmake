include(PlatformVars)
include(DownloadAndCheck)
set(base_url https://frcmaven.wpi.edu/artifactory/release/edu/wpi/first/ni-libraries)
set(version 2024.2.1)
set(libs chipobject netcomm visa)
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
        if(NOT EXISTS ${dest}/${headers_artifact}.zip)
            download_and_check(
                ${base_url}/${lib}/${version}/${headers_artifact}.zip
                ${dest}/${headers_artifact}.zip
            )
        endif()
        file(ARCHIVE_EXTRACT INPUT ${dest}/${library_artifact}.zip DESTINATION ${dest}/${lib})
        file(
            ARCHIVE_EXTRACT
            INPUT ${dest}/${headers_artifact}.zip
            DESTINATION ${dest}/${lib}/include
        )
    endif()
    file(GLOB ni_lib ${dest}/${lib}/linux/athena/shared/*)
    add_library(${lib} SHARED IMPORTED)
    target_include_directories(${lib} INTERFACE ${dest}/${lib}/include)
    set_target_properties(
        ${lib}
        PROPERTIES IMPORTED_LOCATION_DEBUG ${ni_lib} IMPORTED_LOCATION_RELWITHDEBINFO ${ni_lib}
    )
    list(APPEND NI_LIBRARIES ${lib})
endforeach()
message(STATUS "Done.")
set(NILibraries_FOUND true PARENT_SCOPE)
