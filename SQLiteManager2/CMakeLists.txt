
qt_add_library(SQLiteManager2 STATIC "SQLiteManager2.cpp")

target_include_directories(SQLiteManager2 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(
	SQLiteManager2 PUBLIC
	Qt6::Core
	Qt6::Sql
	RegistryManager
	DatabaseManager
	ServiceHelper
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET SQLiteManager2 PROPERTY CXX_STANDARD 20)
endif()