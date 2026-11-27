if(WIN32 AND NOT TARGET spirv-cross-core)
    find_path(OpalSpirvCross_INCLUDE_DIR spirv_cross/spirv_cross.hpp
        HINTS ENV VULKAN_SDK PATH_SUFFIXES Include NO_DEFAULT_PATH NO_CACHE
    )
    find_library(OpalSpirvCross_LIBRARY NAMES spirv-cross-core
        HINTS ENV VULKAN_SDK PATH_SUFFIXES Lib NO_DEFAULT_PATH NO_CACHE
    )
    if(OpalSpirvCross_INCLUDE_DIR AND OpalSpirvCross_LIBRARY)
        add_library(spirv-cross-core STATIC IMPORTED)
        set_target_properties(spirv-cross-core PROPERTIES
            IMPORTED_LOCATION "${OpalSpirvCross_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OpalSpirvCross_INCLUDE_DIR}"
        )
    endif()
endif()

if(NOT TARGET Opal::Slang)
    find_path(OpalSlang_INCLUDE_DIR slang-com-ptr.h
        HINTS "${OpalSlang_ROOT}" ENV OpalSlang_ROOT ENV VULKAN_SDK
        PATH_SUFFIXES include/slang Include/slang include Include
    )
    find_library(OpalSlang_LIBRARY NAMES slang-compiler slang
        HINTS "${OpalSlang_ROOT}" ENV OpalSlang_ROOT ENV VULKAN_SDK
        PATH_SUFFIXES lib Lib lib64
    )
    mark_as_advanced(OpalSlang_INCLUDE_DIR OpalSlang_LIBRARY)

    if(OpalSlang_INCLUDE_DIR AND OpalSlang_LIBRARY)
        add_library(Opal::Slang UNKNOWN IMPORTED)
        set_target_properties(Opal::Slang PROPERTIES
            IMPORTED_LOCATION "${OpalSlang_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OpalSlang_INCLUDE_DIR}"
        )
    endif()
endif()
