
qt_add_library(SQLiteManager2 SHARED "SQLiteManager2.cpp")
qt_disable_unicode_defines(SQLiteManager2)

target_link_libraries(
	SQLiteManager2 PRIVATE
	Qt6::Core
	Qt6::Sql
	HenchmanServiceException
	EventManager
	ServiceHelper
	RegistryManager
)

target_include_directories(SQLiteManager2 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET SQLiteManager2 PROPERTY CXX_STANDARD 20)
endif()