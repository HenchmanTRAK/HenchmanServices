add_executable(HenchmanServiceMailer "HenchmanServiceMailer.cpp")

target_include_directories(HenchmanServiceMailer PRIVATE ".")
target_link_libraries(HenchmanServiceMailer PRIVATE HenchmanServiceException)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET HenchmanServiceMailer PROPERTY CXX_STANDARD 20)
endif()