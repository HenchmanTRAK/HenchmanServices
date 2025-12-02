
add_library(ServiceController SHARED "ServiceController.cpp" "ServiceController.h")

target_include_directories(
	ServiceController INTERFACE 
	${CMAKE_CURRENT_SOURCE_DIR}
)


if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ServiceController PROPERTY CXX_STANDARD 20)
endif()