include(PlatformVars)
if(OpenCV_FOUND)
elseif(BUILD_MAVEN)
    include(DownloadAndCheck)
    set(base_url
        https://frcmaven.wpi.edu/artifactory/release/edu/wpi/first/thirdparty/frc2024/opencv/opencv-cpp/4.8.0-4
    )
    set(base_artifact_name opencv-cpp-4.8.0-4)
    set(library_artifact ${base_artifact_name}-${platform}${arch})
    set(headers_artifact ${base_artifact_name}-headers)
    set(dest ${WPILIB_BINARY_DIR}/opencv)
    message(STATUS "Downloading and unpacking OpenCV artifacts.")
    if(NOT EXISTS ${dest}/${library_artifact}.zip)
        download_and_check(${base_url}/${library_artifact}.zip ${dest}/${library_artifact}.zip)
    endif()
    if(NOT EXISTS ${dest}/${headers_artifact}.zip)
        download_and_check(${base_url}/${headers_artifact}.zip ${dest}/${headers_artifact}.zip)
    endif()
    if(NOT EXISTS ${dest}/${library_artifact}debug.zip)
        download_and_check(
            ${base_url}/${library_artifact}debug.zip
            ${dest}/${library_artifact}debug.zip
        )
    endif()

    file(ARCHIVE_EXTRACT INPUT ${dest}/${library_artifact}.zip DESTINATION ${dest}/release)
    file(ARCHIVE_EXTRACT INPUT ${dest}/${library_artifact}debug.zip DESTINATION ${dest}/debug)
    file(ARCHIVE_EXTRACT INPUT ${dest}/${headers_artifact}.zip DESTINATION ${dest}/include)
    message(STATUS "Done.")
    set(libs
        aruco
        calib3d
        core
        dnn
        features2d
        flann
        gapi
        highgui
        imgcodecs
        imgproc
        ml
        objdetect
        photo
        stitching
        video
        videoio
    )
    foreach(lib ${libs})
        add_library(opencv_${lib} SHARED IMPORTED GLOBAL)
        set(debug_lib_location ${dest}/debug/${platform}/${arch}/shared)
        set(release_lib_location ${dest}/release/${platform}/${arch}/shared)
        target_include_directories(opencv_${lib} INTERFACE ${dest}/include)
        if(MSVC)
            set_target_properties(
                opencv_${lib}
                PROPERTIES
                    IMPORTED_IMPLIB_DEBUG ${debug_lib_location}/opencv_${lib}480d.lib
                    IMPORTED_IMPLIB_RELWITHDEBINFO ${release_lib_location}/opencv_${lib}480.lib
                    IMPORTED_LOCATION_DEBUG ${debug_lib_location}/opencv_${lib}480d.dll
                    IMPORTED_LOCATION_RELWITHDEBINFO ${release_lib_location}/opencv_${lib}480.dll
            )
        elseif(APPLE)
            set_target_properties(
                opencv_${lib}
                PROPERTIES
                    IMPORTED_LOCATION_DEBUG ${debug_lib_location}/libopencv_${lib}d.4.8.dylib
                    IMPORTED_LOCATION_RELWITHDEBINFO
                        ${release_lib_location}/libopencv_${lib}.4.8.dylib
            )
        else()
            set_target_properties(
                opencv_${lib}
                PROPERTIES
                    IMPORTED_LOCATION_DEBUG ${debug_lib_location}/libopencv_${lib}d.so.4.8
                    IMPORTED_LOCATION_RELWITHDEBINFO ${release_lib_location}/libopencv_${lib}.so.4.8
            )
        endif()
        list(APPEND OpenCV_LIBS opencv_${lib})
    endforeach()
    if(WITH_JAVA)
        download_and_check(
            "https://frcmaven.wpi.edu/artifactory/release/edu/wpi/first/thirdparty/frc2024/opencv/opencv-java/4.8.0-4/opencv-java-4.8.0-4.jar"
            ${dest}/opencv-java-4.8.0-4.jar
        )
        set(OPENCV_JAR_FILE ${dest}/opencv-java-4.8.0-4.jar)
    endif()
    set(OpenCV_FOUND true)
else()
    find_package(OpenCV CONFIG REQUIRED)
endif()
