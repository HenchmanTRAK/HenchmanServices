
qt_add_library(TRAKManager STATIC "TRAKManager.cpp" "TRAKManager.h")
qt_disable_unicode_defines(TRAKManager)

target_link_libraries(
	TRAKManager PRIVATE
	DatabaseManager
	EmployeesManager
	QueryManager
	NetworkManager
	SQLiteManager2
	HenchmanServiceException
	EventManager
	ServiceHelper
	RegistryManager
	SimpleIni
	Qt6::Concurrent
	Qt6::Network
	Qt6::Sql
	Qt6::Core
)

target_include_directories(TRAKManager INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})


if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET TRAKManager PROPERTY CXX_STANDARD 20)
endif()