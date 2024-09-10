
add_library(SQLiteManager "SQLiteManager.cpp")

target_include_directories(SQLiteManager PUBLIC ".")
target_link_libraries(SQLiteManager PRIVATE 
	SQLiteCpp 
	sqlite3
	ServiceHelper
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET SQLiteManager PROPERTY CXX_STANDARD 20)
endif()