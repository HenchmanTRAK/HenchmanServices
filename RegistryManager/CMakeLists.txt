
add_library(RegistryManager "RegistryManager.cpp")

target_include_directories(RegistryManager PUBLIC ".")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET RegistryManager PROPERTY CXX_STANDARD 20)
endif()