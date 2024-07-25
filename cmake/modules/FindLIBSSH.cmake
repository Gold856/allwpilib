# - Try to find LibSSH
# Once done this will define
#
#  LIBSSH_FOUND - system has LibSSH
#  LIBSSH_INCLUDE_DIRS - the LibSSH include directory
#  LIBSSH_LIBRARIES - link these to use LibSSH
#  LIBSSH_VERSION -
#
#  Author Michal Vasko <mvasko@cesnet.cz>
#  Copyright (c) 2020 CESNET, z.s.p.o.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#
#  1. Redistributions of source code must retain the copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. The name of the author may not be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
#  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
#  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
#  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
#  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
#  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
#  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
include(FindPackageHandleStandardArgs)

if(LIBSSH_LIBRARIES AND LIBSSH_INCLUDE_DIRS)
    # in cache already
    set(LIBSSH_FOUND TRUE)
elseif(BUILD_MAVEN)
    include(FetchContent)
    include(PlatformVars)
    set(base_url
        https://frcmaven.wpi.edu/artifactory/release/edu/wpi/first/thirdparty/frc2024/libssh/0.105-1
    )
    set(base_artifact_name libssh-0.105-1)
    set(library_artifact ${base_artifact_name}-${platform}${arch}static)
    message(STATUS "Downloading and unpacking libssh artifacts.")
    fetchcontent_declare(libssh_debug URL ${base_url}/${library_artifact}debug.zip)
    fetchcontent_declare(libssh_release URL ${base_url}/${library_artifact}.zip)
    fetchcontent_declare(libssh_headers URL ${base_url}/${base_artifact_name}-headers.zip)
    fetchcontent_makeavailable(libssh_debug libssh_release libssh_headers)
    message(STATUS "Done.")
    file(
        GLOB debug_lib_location
        ${libssh_debug_SOURCE_DIR}/${platform}/${arch}/static/*${CMAKE_STATIC_LIBRARY_SUFFIX}
    )
    file(
        GLOB release_lib_location
        ${libssh_release_SOURCE_DIR}/${platform}/${arch}/static/*${CMAKE_STATIC_LIBRARY_SUFFIX}
    )
    add_library(libssh STATIC IMPORTED)
    set_target_properties(
        libssh
        PROPERTIES
            IMPORTED_LOCATION_DEBUG ${debug_lib_location}
            IMPORTED_LOCATION_RELWITHDEBINFO ${release_lib_location}
    )
    target_include_directories(libssh INTERFACE ${libssh_headers_SOURCE_DIR})
    if(MSVC)
        target_link_libraries(libssh INTERFACE ws2_32.lib crypt32.lib)
    elseif(APPLE)
        target_link_options(libssh INTERFACE "-framework Kerberos")
    endif()
    set(LIBSSH_LIBRARIES libssh)
    set(LIBSSH_INCLUDE_DIRS ${libssh_headers_SOURCE_DIR})
    set(LIBSSH_VERSION 0.105.1)
    mark_as_advanced(LIBSSH_INCLUDE_DIRS LIBSSH_LIBRARIES)

    find_package_handle_standard_args(
        LIBSSH
        FOUND_VAR LIBSSH_FOUND
        REQUIRED_VARS LIBSSH_INCLUDE_DIRS LIBSSH_LIBRARIES
        VERSION_VAR LIBSSH_VERSION
    )
else()
    find_path(
        LIBSSH_INCLUDE_DIR
        NAMES libssh/libssh.h
        PATHS
            /usr/include
            /usr/local/include
            /opt/local/include
            /sw/include
            ${CMAKE_INCLUDE_PATH}
            ${CMAKE_INSTALL_PREFIX}/include
    )

    find_library(
        LIBSSH_LIBRARY
        NAMES ssh.so libssh.so libssh.dylib
        PATHS
            /usr/lib
            /usr/local/lib
            /opt/local/lib
            /sw/lib
            ${CMAKE_LIBRARY_PATH}
            ${CMAKE_INSTALL_PREFIX}/lib
    )

    if(LIBSSH_INCLUDE_DIR AND LIBSSH_LIBRARY)
        # learn libssh version
        if(EXISTS ${LIBSSH_INCLUDE_DIR}/libssh/libssh_version.h)
            set(LIBSSH_HEADER_PATH ${LIBSSH_INCLUDE_DIR}/libssh/libssh_version.h)
        else()
            set(LIBSSH_HEADER_PATH ${LIBSSH_INCLUDE_DIR}/libssh/libssh.h)
        endif()
        file(
            STRINGS
            ${LIBSSH_HEADER_PATH}
            LIBSSH_VERSION_MAJOR
            REGEX "#define[ ]+LIBSSH_VERSION_MAJOR[ ]+[0-9]+"
        )
        if(NOT LIBSSH_VERSION_MAJOR)
            message(
                STATUS
                "LIBSSH_VERSION_MAJOR not found, assuming libssh is too old and cannot be used!"
            )
            set(LIBSSH_INCLUDE_DIR "LIBSSH_INCLUDE_DIR-NOTFOUND")
            set(LIBSSH_LIBRARY "LIBSSH_LIBRARY-NOTFOUND")
        else()
            string(REGEX MATCH "[0-9]+" LIBSSH_VERSION_MAJOR ${LIBSSH_VERSION_MAJOR})
            file(
                STRINGS
                ${LIBSSH_HEADER_PATH}
                LIBSSH_VERSION_MINOR
                REGEX "#define[ ]+LIBSSH_VERSION_MINOR[ ]+[0-9]+"
            )
            string(REGEX MATCH "[0-9]+" LIBSSH_VERSION_MINOR ${LIBSSH_VERSION_MINOR})
            file(
                STRINGS
                ${LIBSSH_HEADER_PATH}
                LIBSSH_VERSION_PATCH
                REGEX "#define[ ]+LIBSSH_VERSION_MICRO[ ]+[0-9]+"
            )
            string(REGEX MATCH "[0-9]+" LIBSSH_VERSION_PATCH ${LIBSSH_VERSION_PATCH})

            set(LIBSSH_VERSION
                ${LIBSSH_VERSION_MAJOR}.${LIBSSH_VERSION_MINOR}.${LIBSSH_VERSION_PATCH}
            )

            if(LIBSSH_VERSION VERSION_LESS 0.8.0)
                # libssh_threads also needs to be linked for these versions
                string(
                    REPLACE
                    "libssh.so"
                    "libssh_threads.so"
                    LIBSSH_THREADS_LIBRARY
                    ${LIBSSH_LIBRARY}
                )
                string(
                    REPLACE
                    "libssh.dylib"
                    "libssh_threads.dylib"
                    LIBSSH_THREADS_LIBRARY
                    ${LIBSSH_THREADS_LIBRARY}
                )
                string(
                    REPLACE
                    "ssh.so"
                    "ssh_threads.so"
                    LIBSSH_THREADS_LIBRARY
                    ${LIBSSH_THREADS_LIBRARY}
                )
            endif()
        endif()
    endif()

    set(LIBSSH_INCLUDE_DIRS ${LIBSSH_INCLUDE_DIR})
    set(LIBSSH_LIBRARIES ${LIBSSH_LIBRARY} ${LIBSSH_THREADS_LIBRARY})
    mark_as_advanced(LIBSSH_INCLUDE_DIRS LIBSSH_LIBRARIES)

    find_package_handle_standard_args(
        LIBSSH
        FOUND_VAR LIBSSH_FOUND
        REQUIRED_VARS LIBSSH_INCLUDE_DIRS LIBSSH_LIBRARIES
        VERSION_VAR LIBSSH_VERSION
    )
endif()
