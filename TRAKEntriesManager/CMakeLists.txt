
qt_add_library(TRAKEntriesManager STATIC "TRAKEntriesManager.cpp" "TRAKEntriesManager.h")
qt_disable_unicode_defines(TRAKEntriesManager)

# target_compile_definitions(EmployeesManager PRIVATE EMPLOYEES_MANAGER)

target_link_libraries(
	TRAKEntriesManager PRIVATE
	QueryManager
	NetworkManager
	SQLiteManager2
	HenchmanServiceException
	ServiceHelper
	EventManager
	RegistryManager
	Qt6::Concurrent
	Qt6::Network
	Qt6::Sql
	Qt6::Core
)

target_include_directories(
	TRAKEntriesManager INTERFACE 
	${CMAKE_CURRENT_SOURCE_DIR}
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET TRAKEntriesManager PROPERTY CXX_STANDARD 20)
endif()