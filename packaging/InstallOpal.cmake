include(CMakePackageConfigHelpers)

set(OPAL_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/Opal")
configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/OpalConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/OpalConfig.cmake"
    INSTALL_DESTINATION "${OPAL_CMAKE_INSTALL_DIR}"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/OpalConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
)

install(TARGETS opal EXPORT OpalTargets ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(DIRECTORY include/opal DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(EXPORT OpalTargets NAMESPACE Opal:: DESTINATION "${OPAL_CMAKE_INSTALL_DIR}")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/OpalConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/OpalConfigVersion.cmake"
    DESTINATION "${OPAL_CMAKE_INSTALL_DIR}"
)

if(OPAL_RESOLVED_BACKEND STREQUAL "METAL")
    install(FILES packaging/OpalMetal.cmake DESTINATION "${OPAL_CMAKE_INSTALL_DIR}")
elseif(OPAL_RESOLVED_BACKEND STREQUAL "VULKAN")
    install(FILES packaging/OpalVulkan.cmake DESTINATION "${OPAL_CMAKE_INSTALL_DIR}")
else()
    install(FILES third-party/glad/glad.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/glad")
    install(FILES third-party/KHR/khrplatform.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/KHR")
endif()

install(FILES LICENSE.md docs/installing.md DESTINATION "${CMAKE_INSTALL_DOCDIR}")
if(OPAL_INSTALL_GLM_LICENSE)
    install(FILES "${glm_SOURCE_DIR}/copying.txt" DESTINATION "${CMAKE_INSTALL_DOCDIR}" RENAME GLM-LICENSE.txt)
endif()
if(WIN32)
    install(FILES Install-Opal.ps1 DESTINATION .)
endif()

if(PROJECT_IS_TOP_LEVEL)
    set(CPACK_PACKAGE_NAME Opal)
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    string(TOLOWER "${OPAL_RESOLVED_BACKEND}" OPAL_PACKAGE_BACKEND)
    set(CPACK_PACKAGE_FILE_NAME "Opal-${PROJECT_VERSION}-${OPAL_PACKAGE_BACKEND}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
    include(CPack)
endif()
