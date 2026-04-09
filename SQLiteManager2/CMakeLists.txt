
# INCLUDE(GenerateExportHeader)

qt_add_library(SQLiteManager2 SHARED SQLiteManager2.cpp SQLiteManager2.h)
qt_disable_unicode_defines(SQLiteManager2)

# target_compile_definitions(SQLiteManager2 PRIVATE SQLITE_MANAGER_2)

#QT_WRAP_CPP(
#	SQLiteManager2 SQLiteManager2.cpp SQLiteManager2.h
#)

target_link_libraries(
	SQLiteManager2 PRIVATE
	HenchmanServiceException
	ServiceHelper
	EventManager
	RegistryManager
	Qt6::Sql
	Qt6::Core
)

target_include_directories(
	SQLiteManager2 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET SQLiteManager2 PROPERTY CXX_STANDARD 20)
endif()

# GENERATE_EXPORT_HEADER(SQLiteManager2)