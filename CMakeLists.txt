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
if(NOT ${CMAKE_BUILD_TYPE} STREQUAL "Release")
	target_compile_options(<target> PUBLIC "/ZI" /MDd)
else(NOT ${CMAKE_BUILD_TYPE} STREQUAL "Release")
	target_link_options(<target> PUBLIC "/INCREMENTAL")
endif(NOT ${CMAKE_BUILD_TYPE} STREQUAL "Release")
endif()

set(MSVC_ROOT_DIR C:/Users/Willem/source/repos)

# set the project version. 
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

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
message(STATUS "Using c++ standard c++ ${CMAKE_CXX_STANDARD}")
message(STATUS "Using c standard c ${CMAKE_C_STANDARD}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/archive)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

message (STATUS "CMake version: ${CMAKE_VERSION}")
message (STATUS "Project version: ${PROJECT_VERSION}")

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_CXX_VISIBILITY_PRESET "hidden")
set(CMAKE_VISIBILITY_INLINES_HIDDEN TRUE)
set(CMAKE_AUTOMOC ON)

# set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} /Wall /GR)

include(FetchContent)
include(ExternalProject)
include(GNUInstallDirs)
include (InstallRequiredSystemLibraries)
if(${CMAKE_BUILD_TYPE} STREQUAL "Release")
	set(CMAKE_INSTALL_DEBUG_LIBRARIES FALSE)
else()
	set(CMAKE_INSTALL_DEBUG_LIBRARIES TRUE)
endif(${CMAKE_BUILD_TYPE} STREQUAL "Release")
set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
# Second, for multi-config builds (e.g. msvc)
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
	message(${OUTPUTCONFIG})
	string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
	string( TOLOWER ${OUTPUTCONFIG} CONFIGTYPE )
	message(${OUTPUTCONFIG})
	message(${CONFIGTYPE})
	set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} )
	set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} )
	set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY} )
if(${CONFIGTYPE} STREQUAL "RELEASE" OR ${CMAKE_BUILD_TYPE} STREQUAL "Release")
	set (CMAKE_CXX_FLAGS_${OUTPUTCONFIG}	"/MD /O1 /O2")
else()
	set (CMAKE_CXX_FLAGS_${OUTPUTCONFIG}	"/MDd /ZI /Od")
endif(${CONFIGTYPE} STREQUAL "RELEASE" OR ${CMAKE_BUILD_TYPE} STREQUAL "Release")
	set (CMAKE_EXE_LINKER_FLAGS_${OUTPUTCONFIG}         "/${CONFIGTYPE}")
endforeach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )

MESSAGE(${CMAKE_BUILD_TYPE})

find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)

set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH};$ENV{QTDIR};${QT_ROOT_DIR};${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/quazip)
message(STATUS "CMAKE PREFEX PATH: " ${CMAKE_PREFIX_PATH})
find_package(Qt6 REQUIRED COMPONENTS Core Sql Network Concurrent)
qt_standard_project_setup()

if(WIN32)
	find_package(Doxygen REQUIRED dot)
endif(WIN32)

set(QUAZIP_QT_MAJOR_VERSION 6)

option(BUILD_SHARED_LIBS "Build shared libraries (DLLs)." ON)
option(SQLITECPP_RUN_DOXYGEN "Runs Doxygen for SQLite CPP" ON)
option(SQLITECPP_BUILD_TESTS "Build and run tests." OFF)
option(SQLITECPP_INTERNAL_SQLITE "Add the internal SQLite3 source to the project." OFF)
option(SQLITECPP_RUN_CPPLINT "Run c++ lint" ON)

qt_policy(SET QTP0003 NEW)

set(ADDITIONAL_LIBRARIES)

#set(OPENSSL_ROOT_DIR "C:/Program Files/OpenSSL")
#if(WIN32)
#find_package(OpenSSL REQUIRED)
#endif(WIN32)

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
#message(STATUS "Fetching content for OpenSSL")
#FetchContent_Declare(
#	OpenSSL
#	URL				https://download.firedaemon.com/FireDaemon-OpenSSL/openssl-3.3.2.zip
#	URL_HASH		SHA256=B2C1902786FEF6FE600DD8AEB38E33BDCAF306929FDADF28170CCA71999C30AA
#	FIND_PACKAGE_ARGS HINTS x64
#)

message(STATUS "Fetching content for QMYSQL version: ${Qt6_VERSION}")
if(${Qt6_VERSION} EQUAL "6.7.2")
	set(QMySQL_URL https://github.com/user-attachments/files/15905074/qsqlmysql.dll_Qt_SQL_driver_6.7.2_MSVC2019_64-bit.zip)
elseif(${Qt6_VERSION} EQUAL "6.8.0")
	set(QMySQL_URL https://github.com/user-attachments/files/17289450/qsqlmysql.dll_Qt_SQL_driver_6.8.0_MSVC2022_64-bit.zip)
elseif(${Qt6_VERSION} EQUAL "6.8.3")
	set(QMySQL_URL https://github.com/thecodemonkey86/qt_mysql_driver/releases/download/qmysql_6.8.3/qsqlmysql.dll_Qt_SQL_driver_6.8.3_MSVC2022_64-bit.zip)
endif()
FetchContent_Declare(
	QMYSQL
	URL ${QMySQL_URL}
)

set(QMYSQL_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/_deps/qmysql-src)

FetchContent_MakeAvailable(SimpleIni QMYSQL)

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
add_subdirectory (RegistryManager)
add_subdirectory (ServiceHelper)
add_subdirectory (EventManager)
add_subdirectory (HenchmanServiceException)
add_subdirectory (SQLiteManager2)
LIST(APPEND ADDITIONAL_LIBRARIES "SQLiteManager2")
add_subdirectory (NetworkManager)
LIST(APPEND ADDITIONAL_LIBRARIES "NetworkManager")
add_subdirectory (QueryManager)
add_subdirectory (TRAKEntriesManager)
add_subdirectory (EmployeesManager)
LIST(APPEND ADDITIONAL_LIBRARIES "EmployeesManager")
add_subdirectory (UsersManager)
LIST(APPEND ADDITIONAL_LIBRARIES "UsersManager")
add_subdirectory (DatabaseManager)
LIST(APPEND ADDITIONAL_LIBRARIES "DatabaseManager")
add_subdirectory (TRAKManager)
#LIST(APPEND ADDITIONAL_LIBRARIES "TRAKManager")
add_subdirectory (HenchmanServiceTaskScheduler)
add_subdirectory (ServiceController)
#LIST(APPEND ADDITIONAL_LIBRARIES "ServiceController")
add_subdirectory (HenchmanServiceLibrary)
LIST(APPEND ADDITIONAL_LIBRARIES "HenchmanService")
add_subdirectory (HenchmanServiceController)
add_subdirectory (HenchmanService)
#add_subdirectory (SQLiteManager)
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
	set(executable_path "\${CMAKE_INSTALL_BINDIR}/$<TARGET_FILE_NAME:HenchmanService>")
endif()

set(additional_libs)

foreach(lib IN LISTS ADDITIONAL_LIBRARIES)
	LIST(APPEND additional_libs "ADDITIONAL_LIBRARIES \"\${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<TARGET_FILE_NAME:${lib}>\"")
endforeach()

LIST(JOIN additional_libs " " additional_libs)

# Generate a deployment script to be executed at install time
qt_generate_deploy_script(
	TARGET HenchmanService
	OUTPUT_SCRIPT deploy_script
	#NO_UNSUPPORTED_PLATFORM_ERROR
	CONTENT "
qt_deploy_runtime_dependencies(
	EXECUTABLE \"${executable_path}\"
	${additional_libs}
	BIN_DIR \"\${CMAKE_INSTALL_BINDIR}\"
	PRE_EXCLUDE_REGEXES \"api-ms-\" \"ext-ms-\" 
	POST_EXCLUDE_REGEXES \".*system32/.*\\.dll\" \"api-ms-\" \"ext-ms-\"
	NO_UNSUPPORTED_PLATFORM_ERROR
	NO_TRANSLATIONS
	GENERATE_QT_CONF
	VERBOSE
)"
)

if(EXISTS $ENV{QTDIR}/bin/Qt6Cored.dll OR EXISTS $ENV{QTDIR}/bin/Qt6Core.dll)
	message(STATUS "Checking for Qt6Core in " $ENV{QTDIR}/bin ": Yes")
else()
	message(STATUS "Checking for Qt6Core in " $ENV{QTDIR}/bin ": No")
endif()

message(STATUS "Current Source: " ${CMAKE_CURRENT_SOURCE_DIR})

configure_file(service.ini ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/service.ini COPYONLY)
# if(NOT EXISTS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/service.ini)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/service.ini)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/version")
# endif(NOT EXISTS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/service.ini)

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

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/example_service.ini)
install (
	FILES ${CMAKE_CURRENT_SOURCE_DIR}/example_service.ini
	#${MYSQL_CONCPP_RUNTIME_LIBRARY_DIR}/mysqlcppconn-10-vs14.dll 
	#${MYSQL_CONCPP_RUNTIME_LIBRARY_DIR}/mysqlcppconn-2-vs14.dll 
	DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT DataFiles RENAME service.ini
)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/README.md)
install (
	FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
	DESTINATION ${CMAKE_INSTALL_BINDIR}/.. COMPONENT Documentation
)
endif()

install (
	TARGETS HenchmanServiceController
	BUNDLE DESTINATION .
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}							COMPONENT Runtime
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}							COMPONENT Runtime
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/${CMAKE_PROJECT_NAME}	COMPONENT Development
)

install(
	TARGETS HenchmanService
	BUNDLE DESTINATION .
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}							COMPONENT Runtime
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}							COMPONENT Runtime
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/${CMAKE_PROJECT_NAME}	COMPONENT Development
)
add_dependencies(HenchmanService HenchmanServiceController)
#set_property(TARGET HenchmanService PROPERTY WIN32_EXECUTABLE TRUE)
install (PROGRAMS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
		DESTINATION ${CMAKE_INSTALL_BINDIR}	COMPONENT Runtime)

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
	Generate_Release_Project_Build
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND cmake --build . --config ${CMAKE_BUILD_TYPE} --target ALL_BUILD
	COMMENT "Generate release build for project using all_build target" VERBATIM
)

add_custom_target(
	Generate_Installed_Project
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND cmake --build . --config ${CMAKE_BUILD_TYPE} --target INSTALL
	COMMENT "Generate installed build for project using install target" VERBATIM
)

add_custom_target(
	Generate_Project_Installer
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND cmake --build . --config ${CMAKE_BUILD_TYPE} --target PACKAGE
	COMMENT "Generate installer for project using package target" VERBATIM
)
if(DOXYGEN_FOUND)
	add_dependencies(Generate_Project_Installer DOC)
endif(DOXYGEN_FOUND)

install(SCRIPT ${deploy_script} COMPONENT Runtime)
#add_dependencies(Generate_Project_Installer HenchmanService)


#"generator": "Ninja",
#"generator": "Visual Studio 17 2022",
# "CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
# .suo;.sln;.ncb;.sdf;.vcxproj;.csproj;.user;.vbproj;.scc;.vsscc;.vspscc;.old;.filters;.vcxitems
# "environment": {
#        "QTDIR": "C:/Qt/OnlineInstall/6.7.2/msvc2019_64"
#      },
#      "vendor": {
#        "microsoft.com/VisualStudioSettings/CMake/1.0": {
#          "hostOS": "Windows"
#        }
#      }