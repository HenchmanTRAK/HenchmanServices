
qt_add_library(TRAKManager STATIC "TRAKManager.cpp")
qt_disable_unicode_defines(TRAKManager)

target_include_directories(TRAKManager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(
	TRAKManager PRIVATE
	Qt6::Core
	Qt6::Network
	Qt6::Sql
	SimpleIni
	HenchmanServiceException
	EventManager
	ServiceHelper
	RegistryManager
	SQLiteManager2
	DatabaseManager
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET TRAKManager PROPERTY CXX_STANDARD 20)
endif()