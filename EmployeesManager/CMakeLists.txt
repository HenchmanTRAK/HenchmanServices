
qt_add_library(EmployeesManager STATIC "EmployeesManager.cpp" "EmployeesManager.h")
qt_disable_unicode_defines(EmployeesManager)

# target_compile_definitions(EmployeesManager PRIVATE EMPLOYEES_MANAGER)

target_link_libraries(
	EmployeesManager PRIVATE
	TRAKEntriesManager
	QueryManager
	NetworkManager
	SQLiteManager2
	HenchmanServiceException
	EventManager
	ServiceHelper
	RegistryManager
	Qt6::Concurrent
	Qt6::Network
	Qt6::Sql
	Qt6::Core
)

target_include_directories(
	EmployeesManager INTERFACE 
	${CMAKE_CURRENT_SOURCE_DIR}
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET EmployeesManager PROPERTY CXX_STANDARD 20)
endif()