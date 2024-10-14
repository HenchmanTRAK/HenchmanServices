# installer rules. 
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Example cmake installation.")
set(CPACK_PACKAGE_VENDOR "HenchmanTRAK")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_MODIFY_PATH ON)

# if you have an icon set the path here 
# SET(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/my_cool_icon.ico")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "HenchmanTRAK\\\\HenchmanService")
set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\HenchmanService.exe")
set(CPACK_NSIS_DISPLAY_NAME "HenchmanService ${PROJECT_VER}")
#set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/EULA.txt")

include(CPackComponent)

cpack_add_install_type(Full DISPLAY_NAME "Install Everything")
cpack_add_install_type(Upgrade DISPLAY_NAME "Upgrade Only")

cpack_add_component(Runtime
    DISPLAY_NAME "Main application"
    DESCRIPTION "This will install the main application."
    REQUIRED
    INSTALL_TYPES Full Upgrade
)

cpack_add_component(Binaries
    DISPLAY_NAME "Application Binaries"
    DESCRIPTION "This will install all the binary files used by the main application."
    REQUIRED
    INSTALL_TYPES Full Upgrade
)

cpack_add_component(DataFiles
    DISPLAY_NAME "Data files"
    DESCRIPTION "This will install various data files used by the application."
    INSTALL_TYPES Full
)

cpack_add_component(Documentation
    DISPLAY_NAME "Application Documentation"
    DESCRIPTION "Various documentioned files to help explain various aspects of the application."
    INSTALL_TYPES Full
)

set(CPACK_COMPONENTS_ALL Runtime Binaries DataFiles Documentation)
if (CMAKE_CL_64)
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
else (CMAKE_CL_64)
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES")
endif (CMAKE_CL_64)
include(CPack)