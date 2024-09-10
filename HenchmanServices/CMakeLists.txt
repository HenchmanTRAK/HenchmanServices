# CMakeList.txt : CMake project for HenchmanServices, include source and define
# project specific logic here.
#


# Add source to this project's executable.
add_executable(HenchmanServices "HenchmanServices.cpp")

target_include_directories(HenchmanServices PRIVATE "include")
target_link_libraries(HenchmanServices PRIVATE 
	HenchmanServiceException 
	ServiceHelper
	SimpleIni 
	SQLiteCpp 
	sqlite3 
	SQLiteManager 
	RegistryManager
)

set_target_properties(HenchmanServices PROPERTIES LINK_FLAGS
	"/MANIFESTUAC:\"level='requireAdministrator'\"" )

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET HenchmanServices PROPERTY CXX_STANDARD 20)
endif()
if (CMAKE_VERSION VERSION_GREATER 2.21)
  install(FILES $<TARGET_RUNTIME_DLLS:HenchmanServices> TYPE BIN)
endif()

# TODO: Add tests and install targets if needed.
