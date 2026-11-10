add_library(opal_c SHARED src/opal_c.cpp)
add_library(Opal::C ALIAS opal_c)

target_link_libraries(opal_c PRIVATE Opal::Opal)
target_include_directories(opal_c PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_definitions(opal_c PRIVATE OPAL_C_BUILD)
target_compile_features(opal_c PRIVATE cxx_std_20)
set_target_properties(opal_c PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    OUTPUT_NAME opal_c
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

include(GNUInstallDirs)
install(TARGETS opal_c
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES include/opal_c.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ../docs/c-abi.md ../docs/c-abi-reference.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

option(OPAL_BUILD_C_ABI_EXAMPLES "Build the Opal C ABI examples" OFF)
if(OPAL_BUILD_C_ABI_EXAMPLES)
    add_executable(opal_c_clear examples/clear.c)
    target_compile_features(opal_c_clear PRIVATE c_std_99)
    target_link_libraries(opal_c_clear PRIVATE Opal::C ${OPAL_SDL_TARGET})
endif()
