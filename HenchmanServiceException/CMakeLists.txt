
qt_add_library(HenchmanServiceException STATIC "HenchmanServiceException.cpp")
qt_disable_unicode_defines(HenchmanServiceException)

target_include_directories(HenchmanServiceException PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(
	HenchmanServiceException PRIVATE
	Qt6::Core
	EventManager
	ServiceHelper
	RegistryManager
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET HenchmanServiceException PROPERTY CXX_STANDARD 20)
endif()
