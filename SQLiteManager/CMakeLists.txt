
qt_add_library(SQLiteManager STATIC "SQLiteManager.cpp")

target_include_directories(SQLiteManager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(
	SQLiteManager PRIVATE
	Qt6::Core
	EventManager
	HenchmanServiceException
	RegistryManager
	ServiceHelper
	SQLiteCpp 
	sqlite3
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET SQLiteManager PROPERTY CXX_STANDARD 20)
endif()