if(ANDROID)
    set(MAKIMEDIA_MPV_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mpv-android"
        CACHE PATH "Root of the Android libmpv SDK: expects include/mpv/client.h and <abi>/libmpv.so")
else()
    set(MAKIMEDIA_MPV_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mpv"
        CACHE PATH "Root of the libmpv SDK: expects include/mpv/client.h and a link library")
endif()

if(ANDROID AND LIBMPV_LIBRARY AND NOT LIBMPV_LIBRARY MATCHES "\\.so$")
    message(STATUS
        "Discarding cached LIBMPV_LIBRARY '${LIBMPV_LIBRARY}': it is not an "
        "Android shared library, so this cache predates the Android support "
        "in FindLibmpv.cmake")
    unset(LIBMPV_LIBRARY CACHE)
    unset(LIBMPV_INCLUDE_DIR CACHE)
endif()

find_path(LIBMPV_INCLUDE_DIR
    NAMES mpv/client.h
    HINTS "${MAKIMEDIA_MPV_ROOT}/include"
          "${CMAKE_CURRENT_SOURCE_DIR}/third_party/include"
    NO_CMAKE_FIND_ROOT_PATH
)

if(ANDROID)
    set(_makimedia_abi_candidates
        "${MAKIMEDIA_MPV_ROOT}/${CMAKE_ANDROID_ARCH_ABI}"
        "${MAKIMEDIA_MPV_ROOT}/lib/${CMAKE_ANDROID_ARCH_ABI}"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/${CMAKE_ANDROID_ARCH_ABI}"
    )

    set(MAKIMEDIA_MPV_ABI_DIR "")
    foreach(_candidate IN LISTS _makimedia_abi_candidates)
        if(EXISTS "${_candidate}/libmpv.so")
            set(MAKIMEDIA_MPV_ABI_DIR "${_candidate}")
            break()
        endif()
    endforeach()

    if(NOT MAKIMEDIA_MPV_ABI_DIR)
        set(MAKIMEDIA_MPV_ABI_DIR "${MAKIMEDIA_MPV_ROOT}/${CMAKE_ANDROID_ARCH_ABI}")
    endif()

    find_library(LIBMPV_LIBRARY
        NAMES mpv
        HINTS "${MAKIMEDIA_MPV_ABI_DIR}"
        NO_CMAKE_FIND_ROOT_PATH
        NO_DEFAULT_PATH
    )
elseif(MSVC)
    find_library(LIBMPV_LIBRARY
        NAMES mpv libmpv
        HINTS "${MAKIMEDIA_MPV_ROOT}/lib" "${MAKIMEDIA_MPV_ROOT}"
              "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
        NO_CMAKE_FIND_ROOT_PATH
    )
else()
    find_library(LIBMPV_LIBRARY
        NAMES mpv libmpv libmpv.dll.a
        HINTS "${MAKIMEDIA_MPV_ROOT}/lib" "${MAKIMEDIA_MPV_ROOT}"
              "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
        NO_CMAKE_FIND_ROOT_PATH
    )
endif()

if(NOT LIBMPV_INCLUDE_DIR OR NOT LIBMPV_LIBRARY)
    if(ANDROID)
        message(FATAL_ERROR
            "libmpv for Android was not found.\n"
            "Expected layout:\n"
            "  third_party/mpv-android/include/mpv/client.h\n"
            "  third_party/mpv-android/${CMAKE_ANDROID_ARCH_ABI}/libmpv.so\n"
            "Looked in: ${MAKIMEDIA_MPV_ABI_DIR}\n"
            "See third_party/README.md for how to produce or obtain them.\n"
            "Override the location with -DMAKIMEDIA_MPV_ROOT=<path>."
        )
    else()
        message(FATAL_ERROR
            "libmpv was not found under ${MAKIMEDIA_MPV_ROOT} or third_party/.\n"
            "Expected either layout:\n"
            "  third_party/mpv/include/mpv/client.h + mpv/lib + mpv/bin\n"
            "  third_party/include/mpv/client.h     + third_party/\n"
            "MSVC needs mpv.lib, generated from mpv.def with lib /def:.\n"
            "The MinGW libmpv.dll.a is not searched for under MSVC because it\n"
            "cannot be linked. See third_party/README.md.\n"
            "Override the location with -DMAKIMEDIA_MPV_ROOT=<path>."
        )
    endif()
endif()

if(ANDROID AND NOT LIBMPV_LIBRARY MATCHES "\\.so$")
    message(FATAL_ERROR
        "LIBMPV_LIBRARY resolved to '${LIBMPV_LIBRARY}', which is not an\n"
        "Android shared library. This is almost always a stale CMake cache\n"
        "from a desktop configure. Delete the Android build directory, or\n"
        "remove LIBMPV_LIBRARY and LIBMPV_INCLUDE_DIR from its CMakeCache.txt,\n"
        "and configure again."
    )
endif()

if(NOT TARGET Libmpv::Libmpv)
    if(ANDROID)
        add_library(Libmpv::Libmpv SHARED IMPORTED)
    else()
        add_library(Libmpv::Libmpv UNKNOWN IMPORTED)
    endif()
    set_target_properties(Libmpv::Libmpv PROPERTIES
        IMPORTED_LOCATION "${LIBMPV_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBMPV_INCLUDE_DIR}"
    )
endif()

if(ANDROID)
    file(GLOB _makimedia_abi_libs "${MAKIMEDIA_MPV_ABI_DIR}/*.so")

    set(LIBMPV_RUNTIME_LIBS "")
    foreach(_lib IN LISTS _makimedia_abi_libs)
        get_filename_component(_libname "${_lib}" NAME)
        if(_libname STREQUAL "libc++_shared.so")
            message(STATUS
                "Skipping ${_libname}: androiddeployqt always packages the "
                "NDK's own copy and an extra lib cannot override it. If libmpv "
                "fails to dlopen on a missing std:: symbol, the libmpv build "
                "needs a newer libc++ than this NDK provides - use an NDK at "
                "least as new as the one libmpv was built with, or a libmpv "
                "built against an older one.")
        elseif(_libname STREQUAL "libplayer.so")
            message(STATUS
                "Skipping ${_libname}, it is the mpv-android app's own native "
                "library and expects that app's Java classes")
        else()
            list(APPEND LIBMPV_RUNTIME_LIBS "${_lib}")
        endif()
    endforeach()

    if(NOT LIBMPV_RUNTIME_LIBS)
        message(WARNING
            "No .so files found in ${MAKIMEDIA_MPV_ABI_DIR}. "
            "Nothing will be packaged into the APK and the app will not start.")
    endif()
    message(STATUS "libmpv Android ABI: ${CMAKE_ANDROID_ARCH_ABI}")
    message(STATUS "libmpv packaged libs: ${LIBMPV_RUNTIME_LIBS}")
endif()

if(WIN32 AND NOT ANDROID)
    find_file(LIBMPV_RUNTIME_DLL
        NAMES libmpv-2.dll mpv-2.dll libmpv.dll
        HINTS "${MAKIMEDIA_MPV_ROOT}/bin" "${MAKIMEDIA_MPV_ROOT}"
              "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
        NO_CMAKE_FIND_ROOT_PATH
    )
    if(NOT LIBMPV_RUNTIME_DLL)
        message(WARNING
            "libmpv runtime DLL not found under ${MAKIMEDIA_MPV_ROOT}. "
            "The build will link but the executable will not start.")
    endif()
endif()

message(STATUS "libmpv headers: ${LIBMPV_INCLUDE_DIR}")
message(STATUS "libmpv library: ${LIBMPV_LIBRARY}")
