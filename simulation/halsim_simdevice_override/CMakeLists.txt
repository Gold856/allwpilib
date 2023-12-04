project(halsim_simdevice_override)

include(CompileWarnings)

file(GLOB halsim_simdevice_override_src src/main/native/cpp/*.cpp)

add_library(halsim_simdevice_override SHARED ${halsim_simdevice_override_src})
wpilib_target_warnings(halsim_simdevice_override)
set_target_properties(halsim_simdevice_override PROPERTIES DEBUG_POSTFIX "d")
target_link_libraries(halsim_simdevice_override PUBLIC hal)

set_property(TARGET halsim_simdevice_override PROPERTY FOLDER "libraries")

install(TARGETS halsim_simdevice_override EXPORT halsim_simdevice_override)
