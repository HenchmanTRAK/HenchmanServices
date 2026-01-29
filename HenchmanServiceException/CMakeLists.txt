
qt_add_library(HenchmanServiceException STATIC "HenchmanServiceException.cpp" "HenchmanServiceException.h")
qt_disable_unicode_defines(HenchmanServiceException)

target_link_libraries(
	HenchmanServiceException PRIVATE
	EventManager
	ServiceHelper
	RegistryManager
	Qt6::Core
)

target_include_directories(HenchmanServiceException INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})


if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET HenchmanServiceException PROPERTY CXX_STANDARD 20)
endif()
