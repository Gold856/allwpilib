include(PlatformVars)
if(NILibraries_FOUND)
elseif(BUILD_MAVEN)
    include(DownloadAndCheck)
    set(base_url https://frcmaven.wpi.edu/artifactory/release/edu/wpi/first/ni-libraries)
    set(version 2024.2.1)
    set(libs chipobject netcomm visa)
    set(dest ${WPILIB_BINARY_DIR}/ni-libraries)
    foreach(lib ${libs})
        set(base_artifact_name ${lib}-${version}-linuxathena)
        set(library_artifact ${base_artifact_name}-linuxathena)
        set(headers_artifact ${base_artifact_name}-headers)
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
        file(GLOB ni_lib ${dest}/${lib}/linux/athena/*)
        add_library(${lib} SHARED IMPORTED GLOBAL)
        target_include_directories(${lib} INTERFACE ${dest}/${lib}/include)
        set_target_properties(
            ${lib}
            PROPERTIES IMPORTED_LOCATION_DEBUG ${ni_lib} IMPORTED_LOCATION_RELWITHDEBINFO ${ni_lib}
        )
        list(APPEND NI_LIBRARIES ${lib})
    endforeach()
    set(NILibraries_FOUND true)
endif()
