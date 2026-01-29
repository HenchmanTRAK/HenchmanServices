
add_library(RegistryManager SHARED "RegistryManager.cpp" "RegistryManager.h")

target_include_directories(
	RegistryManager INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}
)


if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET RegistryManager PROPERTY CXX_STANDARD 20)
endif()