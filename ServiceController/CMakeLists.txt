
add_library(ServiceController SHARED "ServiceController.cpp" "ServiceController.h")
QT_DISABLE_UNICODE_DEFINES(ServiceController)

target_include_directories(
	ServiceController INTERFACE 
	${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
	ServiceController PUBLIC
	HenchmanServiceTaskScheduler
)

target_link_libraries(
	ServiceController PRIVATE
	ServiceHelper
	RegistryManager
	Qt6::Core
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ServiceController PROPERTY CXX_STANDARD 20)
endif()