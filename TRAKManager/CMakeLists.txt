
qt_add_library(TRAKManager STATIC "TRAKManager.cpp")

target_include_directories(TRAKManager INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(
	TRAKManager PUBLIC
	Qt6::Core
	Qt6::Sql
	Qt6::Network
	SimpleIni
	EventManager
	HenchmanServiceException
	RegistryManager
	ServiceHelper
	DatabaseManager
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET TRAKManager PROPERTY CXX_STANDARD 20)
endif()