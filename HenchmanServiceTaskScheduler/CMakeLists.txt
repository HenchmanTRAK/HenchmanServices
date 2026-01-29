# CMakeList.txt : CMake project for HenchmanServices, include source and define
# project specific logic here.
#

add_library(HenchmanServiceTaskScheduler STATIC "HenchmanServiceTaskScheduler.cpp" "HenchmanServiceTaskScheduler.h")
#qt_add_library(HenchmanServiceTaskScheduler SHARED "HenchmanServiceTaskScheduler.cpp" "HenchmanServiceTaskScheduler.h")
# QT_DISABLE_UNICODE_DEFINES(HenchmanServiceTaskScheduler)


# target_link_libraries(
# 	HenchmanServiceTaskScheduler PRIVATE 
# 	Qt6::Core
# )

target_include_directories(
	HenchmanServiceTaskScheduler INTERFACE 
	${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
	HenchmanServiceTaskScheduler PRIVATE
#	ServiceHelper
#	RegistryManager
	Qt6::Core
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET HenchmanServiceTaskScheduler PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.