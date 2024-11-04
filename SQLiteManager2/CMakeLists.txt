
qt_add_library(SQLiteManager2 STATIC "SQLiteManager2.cpp")

target_include_directories(SQLiteManager2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(
	SQLiteManager2 PRIVATE
	Qt6::Core
	Qt6::Sql
	Qt6::Network
	DatabaseManager
	HenchmanServiceException
	EventManager
	ServiceHelper
	RegistryManager
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET SQLiteManager2 PROPERTY CXX_STANDARD 20)
endif()