cmake_minimum_required(VERSION 3.20)
project(OpalBar CXX RC)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /permissive- /utf-8")
add_compile_definitions(UNICODE _UNICODE)

add_executable(OpalBar WIN32 loader/main.cpp assets/icon.rc)

add_library(OpalTap SHARED tap/tap.cpp tap/tap.def)
target_compile_definitions(OpalTap PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(OpalTap PRIVATE windowsapp dxguid)

add_custom_command(TARGET OpalTap POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/config.ini $<TARGET_FILE_DIR:OpalTap>)

add_executable(OpalBarSettings WIN32 settings/main.cpp assets/settings.rc)
target_link_libraries(OpalBarSettings PRIVATE shlwapi)
add_custom_command(TARGET OpalBarSettings POST_BUILD
    COMMAND OpalBarSettings --selftest ${CMAKE_SOURCE_DIR}/config.ini VERBATIM)

configure_file(install/install.rc.in install.rc @ONLY)
set(PAYLOAD ${CMAKE_BINARY_DIR}/OpalBar.exe ${CMAKE_BINARY_DIR}/OpalTap.dll ${CMAKE_BINARY_DIR}/OpalBarSettings.exe ${CMAKE_SOURCE_DIR}/config.ini)
set_source_files_properties(${CMAKE_BINARY_DIR}/install.rc PROPERTIES OBJECT_DEPENDS "${PAYLOAD}")
add_executable(OpalBarInstall WIN32 install/main.cpp ${CMAKE_BINARY_DIR}/install.rc)
add_dependencies(OpalBarInstall OpalBar OpalTap OpalBarSettings)
