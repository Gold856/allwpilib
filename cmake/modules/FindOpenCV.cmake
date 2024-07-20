if(BUILD_MAVEN)
    include(FetchContent)
    include(PlatformVars)
    set(base_url
        https://frcmaven.wpi.edu/artifactory/release/edu/wpi/first/thirdparty/frc2024/opencv
    )
    set(base_native_url ${base_url}/opencv-cpp/4.8.0-4)
    set(base_artifact_name opencv-cpp-4.8.0-4)
    set(library_artifact ${base_artifact_name}-${platform}${arch})
    message(STATUS "Downloading and unpacking OpenCV artifacts.")

    fetchcontent_declare(opencv_debug URL ${base_native_url}/${library_artifact}debug.zip)
    fetchcontent_declare(opencv_release URL ${base_native_url}/${library_artifact}.zip)
    fetchcontent_declare(opencv_headers URL ${base_native_url}/${base_artifact_name}-headers.zip)
    fetchcontent_makeavailable(opencv_debug opencv_release opencv_headers)

    if(MSVC)
        file(GLOB_RECURSE opencv_release_libs ${opencv_release_SOURCE_DIR}/*.dll)
        file(GLOB_RECURSE opencv_debug_libs ${opencv_debug_SOURCE_DIR}/*.dll)
        get_property(isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
        if(isMultiConfig)
            file(
                COPY ${opencv_release_libs}
                DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/RelWithDebInfo
            )
            file(COPY ${opencv_debug_libs} DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug)
        else()
            file(COPY ${opencv_release_libs} DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
            file(COPY ${opencv_debug_libs} DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
        endif()
    endif()
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
        set(debug_lib_location ${opencv_debug_SOURCE_DIR}/${platform}/${arch}/shared)
        set(release_lib_location ${opencv_release_SOURCE_DIR}/${platform}/${arch}/shared)
        target_include_directories(opencv_${lib} INTERFACE ${opencv_headers_SOURCE_DIR})
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
        fetchcontent_declare(
            opencv_jar
            URL ${base_url}/opencv-java/4.8.0-4/opencv-java-4.8.0-4.jar
            DOWNLOAD_NO_EXTRACT ON
        )
        fetchcontent_makeavailable(opencv_jar)
        set(OPENCV_JAR_FILE ${opencv_jar_SOURCE_DIR}/opencv-java-4.8.0-4.jar)
    endif()
    set(OpenCV_FOUND true)
else()
    find_package(OpenCV CONFIG REQUIRED)
endif()
