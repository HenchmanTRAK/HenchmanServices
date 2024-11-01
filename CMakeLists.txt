# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.25)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

if(MSVC)
	target_compile_options(<target> PUBLIC "/ZI")
	target_link_options(<target> PUBLIC "/INCREMENTAL")
endif()

# set the project version. 
#set(PROJECT_VER_PATCH 6)
#set(PROJECT_VER_MINOR 1)
#set(PROJECT_VER_MAJOR 1)
#set(PROJECT_VER "${PROJECT_VER_MAJOR}.${PROJECT_VER_MINOR}.${PROJECT_VER_PATCH}")
file(READ version PROJECT_VER)


project (HenchmanServices VERSION ${PROJECT_VER} LANGUAGES CXX C)
#set_property(GLOBAL PROPERTY USE_FOLDERS ON)
#set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "")

#if(EXISTS $ENV{VCPKG_ROOT})
#	set(CMAKE_TOOLCHAIN_FILE $ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)
#elseif(EXISTS $ENV{VCPKG_INSTALLATION_ROOT})
#	set(CMAKE_TOOLCHAIN_FILE $ENV{VCPKG_INSTALLATION_ROOT}/scripts/buildsystems/vcpkg.cmake)
#endif()

message(STATUS "generator toolset: " ${CMAKE_CURRENT_SOURCE_DIR})
#set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT HenchmanService)

set(TRACEDESIGNTIME TRUE)
#set(CMAKE_VS_INCLUDE_INSTALL_TO_DEFAULT_BUILD 1)

set($ENV{VERSION} ${PROJECT_VERSION})

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

#execute_process (
#    COMMAND echo "${Project_Version}" >> "$ENV{VERSION}"
#    OUTPUT_VARIABLE outVar
#)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
message(STATUS "Using c++ standard c++ ${CMAKE_CXX_STANDARD}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/archive)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

message (STATUS "CMake version: ${CMAKE_VERSION}")
message (STATUS "Project version: ${PROJECT_VERSION}")

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

include(FetchContent)
include(ExternalProject)
include(GNUInstallDirs)
set (CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
include (InstallRequiredSystemLibraries)

# Second, for multi-config builds (e.g. msvc)
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
	string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
	string( TOLOWER ${OUTPUTCONFIG} CONFIGTYPE )
	set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} )
	set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} )
	set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY} )
	set (CMAKE_CXX_FLAGS_${OUTPUTCONFIG}                "/ZI /Od")
	set (CMAKE_EXE_LINKER_FLAGS_${OUTPUTCONFIG}         "/${CONFIGTYPE}")
endforeach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )

set(Qt6_DIR "C:/Qt/OnlineInstall/6.7.2/msvc2019_64")
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH};$ENV{QTDIR};${QT_ROOT_DIR})
message(STATUS "CMAKE PREFEX PATH: " ${CMAKE_PREFIX_PATH})
find_package(Qt6 REQUIRED COMPONENTS Core Sql Network)
qt_standard_project_setup()


#INPUT = @CMAKE_CURRENT_SOURCE_DIR@/
#COMMAND $<$<CONFIG:Release>:${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile>

find_package(Doxygen REQUIRED dot)

#file(TO_CMAKE_PATH "C:\\Program Files\\OpenSSL-Win64" OPENSSL_ROOT_DIR)
#message(WARNING "root: ${OPENSSL_ROOT_DIR}")

#set(mariadbcpp_Dir "C:\\Program Files\\MariaDB\\MariaDB C++ Connector 64-bit")
#find_library(mariadbcpp PATHS ${mariadbcpp_Dir})
#find_package(mariadbcpp)
#include_directories(${mariadbcpp_Dir}/include)
#link_directories(${mariadbcpp_Dir}/lib)

#find_package(sqlite3 REQUIRED GLOBAL)
#find_package(OpenSSL REQUIRED GLOBAL)

#message(STATUS "mysql-concpp: " ${MYSQL_CONCPP_VERSION_NUMBER})

#message(WARNING "OpenSSL_VERSION: ${OPENSSL_VERSION}")
#message(WARNING "OpenSSL_INCLUDE_DIR: ${OPENSSL_INCLUDE_DIR}")
#message(WARNING "OpenSSL_LIBRARIES: ${OPENSSL_LIBRARIES}")

option(BUILD_SHARED_LIBS "Build shared libraries (DLLs)." ON)
option(SQLITECPP_RUN_DOXYGEN "Runs Doxygen for SQLite CPP" OFF)
option(SQLITECPP_BUILD_TESTS "Build and run tests." OFF)
option(SQLITECPP_INTERNAL_SQLITE "Add the internal SQLite3 source to the project." OFF)
option(SQLITECPP_RUN_CPPLINT "Run c++ lint" OFF)

qt_policy(SET QTP0003 NEW)

#set(OPENSSL_ROOT_DIR "C:/Program Files/OpenSSL")

find_package(OpenSSL REQUIRED)

message(STATUS "Fetching content for GoogleTest")
FetchContent_Declare(
  googletest
  GIT_REPOSITORY	https://github.com/google/googletest.git
  GIT_TAG			main
)
message(STATUS "Fetching content for SimpleIni")
FetchContent_Declare(
	SimpleIni
	GIT_REPOSITORY	https://github.com/brofield/simpleini.git
	GIT_TAG			v4.22
)
message(STATUS "Fetching content for SQLiteCpp")
FetchContent_Declare(
	SQLiteCpp
	GIT_REPOSITORY	https://github.com/SRombauts/SQLiteCpp.git
	GIT_TAG			master
)
message(STATUS "Fetching content for OpenSSL")
FetchContent_Declare(
	OpenSSL
	URL				https://download.firedaemon.com/FireDaemon-OpenSSL/openssl-3.3.2.zip
	URL_HASH		SHA256=B2C1902786FEF6FE600DD8AEB38E33BDCAF306929FDADF28170CCA71999C30AA
	FIND_PACKAGE_ARGS HINTS x64
)
message(STATUS "Fetching content for QMYSQL")
FetchContent_Declare(
	QMYSQL
	URL https://github.com/user-attachments/files/15905074/qsqlmysql.dll_Qt_SQL_driver_6.7.2_MSVC2019_64-bit.zip
)

set(QMYSQL_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/_deps/qmysql-src)

FetchContent_MakeAvailable(SimpleIni OpenSSL QMYSQL)

#set(mysql-concpp_DIR "C:/Program Files/MySQL/MySQL Connector C++ 9.0")

#add_library(my_openssl INTERFACE)
#target_link_directories(my_openssl INTERFACE ${OPENSSL_LIBRARY_DIR})
#target_link_libraries(my_openssl INTERFACE OpenSSL::SSL OpenSSL::Crypto)
#add_library(mysql::openssl ALIAS my_openssl)

#set(WITH_JDBC true)
#find_package(mysql-concpp REQUIRED debug)

#add_compile_definitions($<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>)

# Include sub-projects.
#add_subdirectory ("simpleini-4.22")
#add_subdirectory (thirdparty/SQLiteCpp-3.3.2)
add_subdirectory (HenchmanService)
add_subdirectory (ServiceHelper)
#add_subdirectory (SQLiteManager)
add_subdirectory (SQLiteManager2)
add_subdirectory (DatabaseManager)
add_subdirectory (TRAKManager)
add_subdirectory (HenchmanServiceException)
add_subdirectory (EventManager)
add_subdirectory (RegistryManager)
add_subdirectory (ServiceController)
#add_subdirectory (HenchmanServiceMailer)

if(DOXYGEN_FOUND)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)

add_custom_target(
	DOC
	Doxygen::doxygen ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMENT "Generating API documentation with Doxygen" VERBATIM
)

#add_dependencies(HenchmanService DOC)

add_custom_command(
	TARGET DOC POST_BUILD
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs/latex
	COMMAND make.bat
	COMMENT "Running latex pdf creator" VERBATIM
)

endif(DOXYGEN_FOUND)

file(COPY ${QMYSQL_SOURCE_DIR}/ DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# App bundles on macOS have an .app suffix
if(APPLE)
    set(executable_path "$<TARGET_FILE_NAME:HenchmanService>.app")
#elseif(MSVC)
#    set(executable_path "$<TARGET_FILE_NAME:HenchmanServices>")
else()
    set(executable_path "${CMAKE_INSTALL_BINDIR}/$<TARGET_FILE_NAME:HenchmanService>")
endif()

message(STATUS ${executable_path})
# Generate a deployment script to be executed at install time
qt_generate_deploy_script(
	TARGET HenchmanService
	OUTPUT_SCRIPT deploy_script
	#NO_UNSUPPORTED_PLATFORM_ERROR
	CONTENT "
qt_deploy_runtime_dependencies(
	EXECUTABLE \"${executable_path}\"
	PRE_EXCLUDE_REGEXES \"api-ms-\" \"ext-ms-\" 
	POST_EXCLUDE_REGEXES \".*system32/.*\\.dll\" 
	NO_UNSUPPORTED_PLATFORM_ERROR
	NO_TRANSLATIONS
	GENERATE_QT_CONF
	VERBOSE
)"
)

if(EXISTS $ENV{QTDIR}/bin/Qt6Cored.dll)
	message(STATUS "Checking for Qt6Core in " $ENV{QTDIR}/bin ": Yes")
else()
	message(STATUS "Checking for Qt6Core in " $ENV{QTDIR}/bin ": No")
endif()

message(STATUS "Current Source: " ${CMAKE_CURRENT_SOURCE_DIR})

configure_file(service.ini ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/service.ini COPYONLY)

file(
	COPY ${CMAKE_SOURCE_DIR}/resources/ 
	DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
	FILES_MATCHING
	PATTERN "*.dll"
)

install (
	DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/
	DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Binaries
	FILES_MATCHING 
	PATTERN "*.dll"
	PATTERN logs EXCLUDE
)

#install (DIRECTORY ${QMYSQL_SOURCE_DIR}/ DESTINATION ${CMAKE_INSTALL_BINDIR})
#install (DIRECTORY ${OPENSSL_RUNTIME_DIR}/ DESTINATION ${CMAKE_INSTALL_BINDIR} FILES_MATCHING PATTERN "*.dll")
#install (DIRECTORY ${QT_DIR}/bin/ DESTINATION bin FILES_MATCHING PATTERN "*.dll")

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/service.ini)
install (
	FILES ${CMAKE_CURRENT_SOURCE_DIR}/service.ini 
	#${MYSQL_CONCPP_RUNTIME_LIBRARY_DIR}/mysqlcppconn-10-vs14.dll 
	#${MYSQL_CONCPP_RUNTIME_LIBRARY_DIR}/mysqlcppconn-2-vs14.dll 
	DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT DataFiles
)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/README.md)
install (
	FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
	DESTINATION ${CMAKE_INSTALL_BINDIR}/.. COMPONENT Documentation
)
endif()

install(
	TARGETS HenchmanService
	BUNDLE DESTINATION .
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}							COMPONENT Runtime
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}							COMPONENT Runtime
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/${CMAKE_PROJECT_NAME}	COMPONENT Development
)
#set_property(TARGET HenchmanService PROPERTY WIN32_EXECUTABLE TRUE)
install (PROGRAMS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
		DESTINATION ${CMAKE_INSTALL_BINDIR}	COMPONENT Runtime)

install(SCRIPT ${deploy_script} COMPONENT Runtime)

message(STATUS "Found Doxy: " ${DOXYGEN_FOUND})

if(DOXYGEN_FOUND)
if(EXISTS ${CMAKE_CURRENT_BINARY_DIR}/docs/html )
	install(
		DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs/html 
		DESTINATION share/docs COMPONENT Documentation
	)
endif(EXISTS ${CMAKE_CURRENT_BINARY_DIR}/docs/html)
if(EXISTS ${CMAKE_CURRENT_BINARY_DIR}/docs/latex/refman.pdf)
	install(
		FILES ${CMAKE_CURRENT_BINARY_DIR}/docs/latex/refman.pdf
		DESTINATION share/docs COMPONENT Documentation RENAME HenchmanService.pdf
	)
endif(EXISTS ${CMAKE_CURRENT_BINARY_DIR}/docs/latex/refman.pdf)
endif(DOXYGEN_FOUND)
#IF (WIN32)
	# for windows, use nsis
#	SET(CPACK_GENERATOR NSIS)
#ENDIF(WIN32)

#set(CPACK_PACKAGE_INSTALL_DIRECTORY "release")
include(CPackConfig)

add_custom_target(
	Generate_Project_Installer
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND cmake --build . --config Release --target PACKAGE
	COMMENT "Generate installer for project using custom target" VERBATIM
)
if(DOXYGEN_FOUND)
add_dependencies(Generate_Project_Installer DOC)
endif(DOXYGEN_FOUND)
#add_dependencies(Generate_Project_Installer HenchmanService)


#"generator": "Ninja",
#"generator": "Visual Studio 17 2022",
# "CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
# .suo;.sln;.ncb;.sdf;.vcxproj;.csproj;.user;.vbproj;.scc;.vsscc;.vspscc;.old;.filters;.vcxitems