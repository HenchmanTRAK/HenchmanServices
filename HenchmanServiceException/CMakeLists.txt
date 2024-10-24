
qt_add_library(HenchmanServiceException STATIC "HenchmanServiceException.cpp")

target_include_directories(HenchmanServiceException INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(
	HenchmanServiceException PUBLIC
	RegistryManager
	ServiceHelper
	EventManager
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET HenchmanServiceException PROPERTY CXX_STANDARD 20)
endif()
