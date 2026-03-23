#INCLUDE(GenerateExportHeader)

qt_add_library(UsersManager SHARED UsersManager.cpp UsersManager.h)
qt_disable_unicode_defines(UsersManager)

# target_compile_definitions(EmployeesManager PRIVATE EMPLOYEES_MANAGER)

#QT_WRAP_CPP(
#	UsersManager UsersManager.cpp
#)

target_link_libraries(
	UsersManager PRIVATE
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
	UsersManager INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
#	PUBLIC ${CMAKE_CURRENT_BINARY_DIR}
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET UsersManager PROPERTY CXX_STANDARD 20)
endif()

#GENERATE_EXPORT_HEADER(UsersManager)