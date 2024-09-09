# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.25)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()


project ("HenchmanServices" VERSION 0.0.1)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

message(STATUS "Using c++ standard c++${CMAKE_CXX_STANDARD}")
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message (STATUS "CMake version: ${CMAKE_VERSION}")
message (STATUS "Project version: ${PROJECT_VERSION}")

include(FetchContent)
include(GNUInstallDirs)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
file(TO_CMAKE_PATH "C:\\Program Files\\OpenSSL-Win64" OPENSSL_ROOT_DIR)
#message(WARNING "root: ${OPENSSL_ROOT_DIR}")
find_package(OpenSSL REQUIRED GLOBAL)
#find_package(sqlite3 REQUIRED GLOBAL)

#message(WARNING "OpenSSL_VERSION: ${OPENSSL_VERSION}")
#message(WARNING "OpenSSL_INCLUDE_DIR: ${OPENSSL_INCLUDE_DIR}")
#message(WARNING "OpenSSL_LIBRARIES: ${OPENSSL_LIBRARIES}")

FetchContent_Declare(
	SimpleIni
	GIT_REPOSITORY "https://github.com/brofield/simpleini.git"
	GIT_TAG v4.22
)
FetchContent_Declare(
	SQLiteCpp
	GIT_REPOSITORY "https://github.com/SRombauts/SQLiteCpp.git"
	GIT_TAG 3.3.2
)

#FetchContent_GetProperties(SimpleIni)
#if (NOT SimpleIni_POPULATED)
#FetchContent_Populate(SimpleIni)
#add_subdirectory(${SimpleIni_SOURCE_DIR} ${SimpleIni_BINARY_DIR})
#endif ()

FetchContent_MakeAvailable(SimpleIni SQLiteCpp)

option(SQLITECPP_BUILD_TESTS "Build and run tests." ON)
option(SQLITECPP_INTERNAL_SQLITE "Add the internal SQLite3 source to the project." ON)
option(BUILD_SHARED_LIBS "Build shared libraries (DLLs)." ON)

# Include sub-projects.
#add_subdirectory ("simpleini-4.22")
#add_subdirectory (thirdparty/SQLiteCpp-3.3.2)
add_subdirectory (HenchmanServiceSQLiteManager)
add_subdirectory (HenchmanServiceException)
#add_subdirectory (HenchmanServiceMailer)
add_subdirectory (HenchmanServices)

configure_file(service.ini ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/service.ini COPYONLY)
install(TARGETS HenchmanServices RUNTIME_DEPENDENCIES PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-" POST_EXCLUDE_REGEXES ".*system32/.*\\.dll")
install (FILES service.ini DESTINATION bin)