# This determines the platform paths needed for Maven artifacts
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
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set(arch x86-64)
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        set(arch arm64)
    elseif(SOFTFP)
        set(arch athena)
    else()
        set(arch arm32)
    endif()
endif()