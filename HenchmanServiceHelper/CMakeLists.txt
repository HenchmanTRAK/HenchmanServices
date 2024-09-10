
add_library(ServiceHelper "ServiceHelper.cpp")

target_include_directories(ServiceHelper PUBLIC ".")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ServiceHelper PROPERTY CXX_STANDARD 20)
endif()