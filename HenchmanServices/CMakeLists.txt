# CMakeList.txt : CMake project for HenchmanServices, include source and define
# project specific logic here.
#


# Add source to this project's executable.
add_executable(HenchmanServices "HenchmanServices.cpp")

target_include_directories(HenchmanServices PRIVATE "include" ${OPENSSL_INCLUDE_DIR})
target_link_libraries(HenchmanServices PRIVATE HenchmanServiceException OpenSSL::applink ${OPENSSL_CRYPTO_LIBRARY} ${OPENSSL_SSL_LIBRARY} SimpleIni SQLiteCpp sqlite3 SQLiteManager)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET HenchmanServices PROPERTY CXX_STANDARD 20)
endif()
if (CMAKE_VERSION VERSION_GREATER 2.21)
  install(FILES $<TARGET_RUNTIME_DLLS:HenchmanServices> TYPE BIN)
endif()

# TODO: Add tests and install targets if needed.
