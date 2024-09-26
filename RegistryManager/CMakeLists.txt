
qt_add_library(RegistryManager STATIC "RegistryManager.cpp")

target_include_directories(RegistryManager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET RegistryManager PROPERTY CXX_STANDARD 20)
endif()