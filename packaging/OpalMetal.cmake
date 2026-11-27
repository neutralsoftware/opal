if(NOT TARGET Opal::MetalFrameworks)
    find_library(OPAL_METAL_FRAMEWORK Metal)
    find_library(OPAL_METALFX_FRAMEWORK MetalFX)
    find_library(OPAL_COCOA_FRAMEWORK Cocoa)
    find_library(OPAL_QUARTZCORE_FRAMEWORK QuartzCore)
    mark_as_advanced(OPAL_METAL_FRAMEWORK OPAL_METALFX_FRAMEWORK OPAL_COCOA_FRAMEWORK OPAL_QUARTZCORE_FRAMEWORK)

    if(OPAL_METAL_FRAMEWORK AND OPAL_METALFX_FRAMEWORK AND OPAL_COCOA_FRAMEWORK AND OPAL_QUARTZCORE_FRAMEWORK)
        add_library(Opal::MetalFrameworks INTERFACE IMPORTED)
        target_link_libraries(Opal::MetalFrameworks INTERFACE
            "${OPAL_METAL_FRAMEWORK}"
            "${OPAL_METALFX_FRAMEWORK}"
            "${OPAL_COCOA_FRAMEWORK}"
            "${OPAL_QUARTZCORE_FRAMEWORK}"
        )
    endif()
endif()
