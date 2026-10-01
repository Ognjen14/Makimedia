cmake_minimum_required(VERSION 3.16)

project(makimedia VERSION 1.0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Qt6 REQUIRED COMPONENTS Core Quick Qml OpenGL Concurrent Sql Network)

qt_standard_project_setup(REQUIRES 6.10)

option(MM_LOG "Enable MM_LOG logging" ON)
option(MM_LOG_VERBOSE "Keep debug lines in the log; OFF for a store release keeps info, warnings and errors" ON)
option(MM_BUILD_TESTS "Build the unit tests" ON)

set(MM_VIDEO_SUFFIXES
    mkv mp4 avi mov webm m4v ts m2ts mts wmv flv mpg mpeg ogv 3gp divx vob rmvb)

if(MM_BUILD_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    enable_testing()
endif()

add_library(makimedia_core STATIC
    src/MmLog.cpp
    src/AppSettings.cpp
    src/SingleInstance.cpp
    src/TextFold.cpp
    src/Data/Database.cpp
    src/Data/MediaRepository.cpp
    src/Data/FileRepository.cpp
    src/Data/PlaybackStateRepository.cpp
    src/Data/FolderRepository.cpp
    src/Data/SubtitleRepository.cpp
    src/Library/BrowseOptions.cpp
    src/Library/GenreShelf.cpp
    src/Library/CollectionShelf.cpp
    src/Library/UniverseCatalog.cpp
    src/Library/CollectionPage.cpp
    src/Library/ListText.cpp
    src/Library/TitleArrangement.cpp
    src/Library/MediaHandle.cpp
    src/Library/SubtitleEncoding.cpp
    src/Library/SubtitleNaming.cpp
    src/Library/SubtitleFinder.cpp
    src/Platform/MediaFormats.cpp
    src/Metadata/FileNameParser.cpp
    src/Metadata/MatchScorer.cpp
    src/Metadata/MatchDecision.cpp
    src/Metadata/ShowGrouping.cpp
    src/Metadata/TmdbAsk.cpp
    src/Metadata/TmdbError.cpp
    src/Metadata/TmdbJsonReader.cpp
    src/Metadata/TmdbReplyError.cpp
    src/Metadata/TmdbImageUrl.cpp
    src/Metadata/TmdbRuntimeConfig.cpp
    src/Metadata/TmdbRequestBuilder.cpp
    src/Metadata/PosterCache.cpp
    src/Streaming/AddressFilter.cpp
    src/Streaming/HttpMessage.cpp
    src/Streaming/StreamServer.cpp
    src/Streaming/Discovery.cpp
    src/Streaming/RemoteMediaSource.cpp
    src/Streaming/FoundServerModel.cpp
    src/Streaming/LibrarySnapshot.cpp
    src/Streaming/Mirror.cpp
    src/Streaming/MirrorSync.cpp
    src/Streaming/StoredMirrorModel.cpp
    src/Streaming/ArtworkLookup.cpp
    src/Streaming/SessionTable.cpp
    src/Streaming/ConnectedDeviceModel.cpp

    include/MmLog.h
    include/AppSettings.h
    include/SingleInstance.h
    include/TextFold.h
    include/Data/Database.h
    include/Data/MediaRepository.h
    include/Data/FileRepository.h
    include/Data/PlaybackState.h
    include/Data/PlaybackStateRepository.h
    include/Data/ScanFolder.h
    include/Data/FolderRepository.h
    include/Data/SubtitleRepository.h
    include/Library/BrowseOptions.h
    include/Library/GenreShelf.h
    include/Library/CollectionShelf.h
    include/Library/UniverseCatalog.h
    include/Library/CollectionPage.h
    include/Library/LibraryChange.h
    include/Library/ListPatch.h
    include/Library/ListText.h
    include/Library/TitleArrangement.h
    include/Library/RowPlacement.h
    include/Library/MediaHandle.h
    include/Library/SubtitleEncoding.h
    include/Library/SubtitleNaming.h
    include/Library/SubtitleFinder.h
    include/Platform/MediaFileInfo.h
    include/Metadata/FileNameParser.h
    include/Metadata/MatchScorer.h
    include/Metadata/MatchDecision.h
    include/Metadata/ShowGrouping.h
    include/Metadata/TmdbAsk.h
    include/Metadata/TmdbDtos.h
    include/Metadata/TmdbError.h
    include/Metadata/TmdbJsonReader.h
    include/Metadata/TmdbReplyError.h
    include/Metadata/TmdbImageUrl.h
    include/Metadata/TmdbRuntimeConfig.h
    include/Metadata/TmdbRequestBuilder.h
    include/Metadata/PosterCache.h
    include/Platform/IMediaSource.h
    include/Streaming/StreamProtocol.h
    include/Streaming/AddressFilter.h
    include/Streaming/HttpMessage.h
    include/Streaming/StreamServer.h
    include/Streaming/Discovery.h
    include/Streaming/RemoteMediaSource.h
    include/Streaming/FoundServerModel.h
    include/Streaming/LibrarySnapshot.h
    include/Streaming/Mirror.h
    include/Streaming/MirrorSync.h
    include/Streaming/StoredMirrorModel.h
    include/Streaming/ArtworkLookup.h
    include/Streaming/SessionTable.h
    include/Streaming/ConnectedDeviceModel.h
)

target_include_directories(makimedia_core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(makimedia_core PUBLIC cxx_std_17)

target_compile_definitions(makimedia_core
    PUBLIC $<$<BOOL:${MM_LOG}>:MM_LOG_ENABLED>
    PRIVATE $<$<BOOL:${MM_LOG_VERBOSE}>:MM_LOG_VERBOSE>
)

target_link_libraries(makimedia_core PUBLIC Qt6::Core Qt6::Sql Qt6::Network)

# The TMDB key is never committed. It lives XOR-obfuscated in
# include/Config/ApiKeys.h, which .gitignore excludes and
# scripts/generate_api_key.py writes:
#
#     python scripts/generate_api_key.py
#
# An empty stub is written here when there is none, so a fresh clone builds
# and runs with no key at all - every file stays a first-class item, and only
# matching is unavailable.
set(MM_API_KEYS_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/Config/ApiKeys.h")

if(EXISTS "${MM_API_KEYS_HEADER}")
    message(STATUS "TMDB key: bundled, obfuscated in include/Config/ApiKeys.h")
else()
    message(STATUS "TMDB key: none bundled. Run scripts/generate_api_key.py "
                   "to add one; matching is unavailable until then.")
    file(WRITE "${MM_API_KEYS_HEADER}"
"#pragma once

// Placeholder written by CMake because no key has been generated yet.
// Run scripts/generate_api_key.py to replace it.

#include <cstddef>

namespace Makimedia::Config
{
inline constexpr unsigned char TmdbApiKeyMask[] = { 0x00 };
inline constexpr unsigned char TmdbApiKeyObfuscated[] = { 0x00 };
inline constexpr std::size_t TmdbApiKeyLength = 0;
inline constexpr unsigned char OpenSubtitlesApiKeyMask[] = { 0x00 };
inline constexpr unsigned char OpenSubtitlesApiKeyObfuscated[] = { 0x00 };
inline constexpr std::size_t OpenSubtitlesApiKeyLength = 0;
}
")
endif()

include(FindLibmpv)

qt_add_executable(appmakimedia
    main.cpp

    #SOURCE
    src/SystemBridge.cpp

    src/Player/MpvController.cpp
    src/Player/MpvRenderItem.cpp
    src/Player/SeekPreviewService.cpp
    src/Player/TrackListModel.cpp

    src/Platform/PlatformServices.cpp
    src/Platform/FilesystemMediaSource.cpp
    src/Platform/FilesystemScanner.cpp

    src/Metadata/TmdbClient.cpp
    src/Metadata/PosterUrlResolver.cpp
    src/Metadata/MakimediaTmdbConfig.cpp
    src/Metadata/MetadataService.cpp

    src/Library/LibraryController.cpp
    src/Library/LibrarySetup.cpp
    src/Library/FolderListModel.cpp
    src/Library/FileListModel.cpp
    src/Library/MediaListModel.cpp
    src/Library/GenreRowModel.cpp
    src/Library/CollectionListModel.cpp
    src/Library/CollectionPageModel.cpp
    src/Library/BrowseListModel.cpp
    src/Library/UnmatchedShowModel.cpp
    src/Library/RemovedFileModel.cpp
    src/Library/ShowEpisodeModel.cpp
    src/Library/ProbeService.cpp
    src/Library/ThumbnailService.cpp

    src/Streaming/StreamingController.cpp

    src/Subtitles/SubtitleSearch.cpp

    #INCLUDE
    include/Subtitles/SubtitleSearch.h

    include/SystemBridge.h

    include/Player/MpvController.h
    include/Player/MpvRenderItem.h
    include/Player/SeekPreviewService.h
    include/Player/TrackListModel.h
    include/Player/VideoTraits.h

    include/Platform/IMediaSource.h
    include/Platform/IFolderPicker.h
    include/Platform/IScanner.h
    include/Platform/IScreenLock.h
    include/Platform/IAudioFocus.h
    include/Platform/ITransportControls.h
    include/Platform/PlatformServices.h
    include/Platform/FilesystemMediaSource.h
    include/Platform/FilesystemScanner.h

    include/Metadata/TmdbClient.h
    include/Metadata/PosterUrlResolver.h
    include/Config/ApiKeys.h
    include/Metadata/MakimediaTmdbConfig.h
    include/Metadata/MetadataService.h

    include/Library/LibraryController.h
    include/Library/LibrarySetup.h
    include/Library/FolderListModel.h
    include/Library/FileListModel.h
    include/Library/MediaListModel.h
    include/Library/GenreRowModel.h
    include/Library/CollectionListModel.h
    include/Library/CollectionPageModel.h
    include/Library/BrowseListModel.h
    include/Library/UnmatchedShowModel.h
    include/Library/RemovedFileModel.h
    include/Library/ShowEpisodeModel.h
    include/Library/ProbeService.h
    include/Library/ThumbnailService.h

    include/Streaming/StreamingController.h
)

if(WIN32)
    target_sources(appmakimedia PRIVATE
        src/Platform/Windows/WindowsMediaSource.cpp
        src/Platform/Windows/WindowsFolderPicker.cpp
        src/Platform/Windows/WindowsFileTypes.cpp
        src/Platform/Windows/WindowsMiniPlayer.cpp
        src/Platform/Windows/WindowsAudioFocus.cpp
        src/Platform/Windows/WindowsTransportControls.cpp
        src/Platform/Windows/WindowsTrayIcon.cpp

        makimedia.rc

        include/Platform/Windows/WindowsMediaSource.h
        include/Platform/Windows/WindowsFolderPicker.h
        include/Platform/Windows/WindowsFileTypes.h
        include/Platform/Windows/WindowsMiniPlayer.h
        include/Platform/Windows/WindowsAudioFocus.h
        include/Platform/Windows/WindowsTransportControls.h
        include/Platform/Windows/WindowsTrayIcon.h
    )
    target_link_libraries(appmakimedia PRIVATE ole32 shell32 windowsapp)

    set_source_files_properties(makimedia.rc PROPERTIES
        OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/makimedia.ico")

    if(MSVC)
        set_source_files_properties(
            src/Platform/Windows/WindowsTransportControls.cpp
            PROPERTIES
                COMPILE_OPTIONS "/std:c++20"
                COMPILE_DEFINITIONS "_SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS")
    endif()

    get_filename_component(MM_WINSDK_ROOT
        "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]"
        ABSOLUTE)
    if(NOT EXISTS "${MM_WINSDK_ROOT}")
        get_filename_component(MM_WINSDK_ROOT
            "[HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]"
            ABSOLUTE)
    endif()

    file(GLOB MM_CPPWINRT_DIRS "${MM_WINSDK_ROOT}/Include/*/cppwinrt")
    if(MM_CPPWINRT_DIRS)
        list(SORT MM_CPPWINRT_DIRS)
        list(GET MM_CPPWINRT_DIRS -1 MM_CPPWINRT_INCLUDE_DIR)
        target_include_directories(appmakimedia PRIVATE "${MM_CPPWINRT_INCLUDE_DIR}")
        message(STATUS "C++/WinRT headers: ${MM_CPPWINRT_INCLUDE_DIR}")
    else()
        message(FATAL_ERROR
            "C++/WinRT headers were not found under ${MM_WINSDK_ROOT}/Include/*/cppwinrt. "
            "They ship with the Windows 10/11 SDK and the media transport controls need them.")
    endif()
endif()

if(ANDROID)
    target_sources(appmakimedia PRIVATE
        src/Platform/Android/AndroidMediaSource.cpp
        src/Platform/Android/AndroidAudioFocus.cpp
        src/Platform/Android/AndroidTransportControls.cpp
        src/Platform/Android/AndroidVideoSurface.cpp

        include/Platform/Android/AndroidMediaSource.h
        include/Platform/Android/AndroidAudioFocus.h
        include/Platform/Android/AndroidTransportControls.h
        include/Platform/Android/AndroidVideoSurface.h
    )
endif()

target_include_directories(appmakimedia
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(appmakimedia
    PRIVATE cxx_std_17
)

target_compile_definitions(appmakimedia
    PRIVATE $<$<BOOL:${MM_LOG}>:MM_LOG_ENABLED>
)

set_source_files_properties(qml/Singletons/AppTheme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(qml/Singletons/PopupRegistry.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(qml/Singletons/Icons.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(qml/Singletons/DevTools.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(qml/Singletons/Format.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(qml/Singletons/SubtitleColours.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(appmakimedia
    URI makimedia
    QML_FILES
        qml/Main.qml

        #Singletons
        qml/Singletons/AppTheme.qml
        qml/Singletons/DevTools.qml
        qml/Singletons/Format.qml
        qml/Singletons/Icons.qml
        qml/Singletons/PopupRegistry.qml
        qml/Singletons/SubtitleColours.qml

        #Controls
        qml/Controls/AccentPicker.qml
        qml/Controls/AppBar.qml
        qml/Controls/AppButton.qml
        qml/Controls/AppDialog.qml
        qml/Controls/AppScrollBar.qml
        qml/Controls/Card.qml
        qml/Controls/CastFace.qml
        qml/Controls/CastGrid.qml
        qml/Controls/Chip.qml
        qml/Controls/ChipGroup.qml
        qml/Controls/ColourSwatch.qml
        qml/Controls/Divider.qml
        qml/Controls/FontSizePicker.qml
        qml/Controls/EmptyState.qml
        qml/Controls/FocusRing.qml
        qml/Controls/FocusScroller.qml
        qml/Controls/HoldToolTip.qml
        qml/Controls/IconButton.qml
        qml/Controls/KeyHints.qml
        qml/Controls/KeyValueRow.qml
        qml/Controls/LinearProgress.qml
        qml/Controls/ListRow.qml
        qml/Controls/MediaTile.qml
        qml/Controls/ModalScrim.qml
        qml/Controls/NavDrawer.qml
        qml/Controls/NavItem.qml
        qml/Controls/PlayerButton.qml
        qml/Controls/Rail.qml
        qml/Controls/RemoteButton.qml
        qml/Controls/RemoteKeyboard.qml
        qml/Controls/RoundedClip.qml
        qml/Controls/RowHeader.qml
        qml/Controls/SetupWindow.qml
        qml/Controls/SearchBar.qml
        qml/Controls/SectionLabel.qml
        qml/Controls/SeekBar.qml
        qml/Controls/SettingsRow.qml
        qml/Controls/SortFilterSheet.qml
        qml/Controls/Snackbar.qml
        qml/Controls/StackedShowTile.qml
        qml/Controls/StateLayer.qml
        qml/Controls/CollectionTile.qml
        qml/Controls/NewCollectionWizard.qml
        qml/Controls/PageSkeleton.qml
        qml/Controls/SurpriseMe.qml
        qml/Controls/SelectField.qml
        qml/Controls/ChoiceSheet.qml
        qml/Controls/GridSizeSwitch.qml
        qml/Controls/PillChip.qml
        qml/Controls/BottomSheet.qml
        qml/Controls/PlayerOsd.qml
        qml/Controls/RadioIndicator.qml
        qml/Controls/FindSubtitlesSheet.qml
        qml/Controls/FixMatchSheet.qml
        qml/Controls/SheetRow.qml
        qml/Controls/SubtitleOverlay.qml
        qml/Controls/ThemedIcon.qml
        qml/Controls/TmdbCredit.qml
        qml/Controls/ToggleSwitch.qml
        qml/Controls/ToolChip.qml

        #Pages
        qml/Pages/AllScreen.qml
        qml/Pages/AppShell.qml
        qml/Pages/DetailsScreen.qml
        qml/Pages/DetailsHero.qml
        qml/Pages/HeldImage.qml
        qml/Pages/HeroBackdrop.qml
        qml/Pages/TV/TvAmbientBackdrop.qml
        qml/Pages/TV/TvArtworkCard.qml
        qml/Pages/TV/TvCastStrip.qml
        qml/Pages/TV/TvCollectionScreen.qml
        qml/Pages/TV/TvDetailsRouter.qml
        qml/Pages/TV/TvEpisodeScreen.qml
        qml/Pages/TV/TvMovieScreen.qml
        qml/Pages/TV/TvShowScreen.qml
        qml/Pages/TV/TvSubtitleSheet.qml
        qml/Pages/FoldersScreen.qml
        qml/Pages/GalleryScreen.qml
        qml/Pages/HomeScreen.qml
        qml/Pages/SearchScreen.qml
        qml/Pages/LibraryRulesScreen.qml
        qml/Pages/SettingsScreen.qml
        qml/Pages/SettingsScreens/AppearancePage.qml
        qml/Pages/SettingsScreens/PlayerPage.qml
        qml/Pages/SettingsScreens/ShortcutsPage.qml
        qml/Pages/StreamingScreen.qml
        qml/Pages/WelcomeScreen.qml
        qml/Pages/PermissionScreen.qml
        qml/Pages/MediaGridScreen.qml
        qml/Pages/CollectionsScreen.qml
        qml/Pages/CollectionScreen.qml
        qml/Pages/TvShowDetailsScreen.qml
        qml/Pages/UnmatchedShowsScreen.qml
        qml/Pages/PlayerScreen.qml
        qml/Pages/PlayerTouchInput.qml
        qml/Pages/PlayerDesktopInput.qml
)

# The launcher artwork lives in android/ because that is where the Android
# build reads it from. Aliased rather than copied, so the About page and the
# launcher can never drift on to two different icons.
set_source_files_properties(android/play_store_512.png
    PROPERTIES QT_RESOURCE_ALIAS assets/app-icon.png)

qt_add_resources(appmakimedia "ASSETS"
    PREFIX "/"

    FILES
    licenses/NOTICE.txt
    licenses/GPL-2.0.txt
    licenses/GPL-3.0.txt
    licenses/LGPL-2.1.txt
    licenses/LGPL-3.0.txt
    licenses/Apache-2.0.txt
    android/play_store_512.png
    assets/check.png
    assets/tmdb_logo_blue.svg
    assets/fonts/Roboto-Regular.ttf
    assets/fonts/Roboto-Medium.ttf
    assets/fonts/Roboto-SemiBold.ttf
    assets/fonts/Roboto-Bold.ttf
    assets/fonts/RobotoMono-Regular.ttf
    assets/moon_dark_theme.svg
    assets/sun_light_theme.svg
    assets/system_theme.svg
    assets/icons/alert-circle.svg
    assets/icons/all.svg
    assets/icons/appearance.svg
    assets/icons/identify-shows.svg
    assets/icons/suggested.svg
    assets/icons/unmatched.svg
    assets/icons/check.svg
    assets/icons/chevron-down.svg
    assets/icons/chevron-left.svg
    assets/icons/chevron-right.svg
    assets/icons/close.svg
    assets/icons/continue-watching.svg
    assets/icons/folder.svg
    assets/icons/grid-large.svg
    assets/icons/grid-medium.svg
    assets/icons/grid-small.svg
    assets/icons/home.svg
    assets/icons/makimedia-mark.svg
    assets/icons/lock.svg
    assets/icons/menu.svg
    assets/icons/mini-player.svg
    assets/icons/more-vertical.svg
    assets/icons/movies.svg
    assets/icons/collections.svg
    assets/universes.json
    assets/icons/next-episode.svg
    assets/icons/no-video.svg
    assets/icons/pause.svg
    assets/icons/play.svg
    assets/icons/plus.svg
    assets/icons/recently-added.svg
    assets/icons/rescan.svg
    assets/icons/search.svg
    assets/icons/settings.svg
    assets/icons/skip-back.svg
    assets/icons/skip-forward.svg
    assets/icons/spinner.svg
    assets/icons/subtitles.svg
    assets/icons/tray.svg
    assets/icons/tv-shows.svg
    assets/icons/volume.svg
)

set_target_properties(appmakimedia PROPERTIES
    WIN32_EXECUTABLE TRUE
    QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
)

if(ANDROID)
    set_property(TARGET appmakimedia APPEND PROPERTY
        QT_ANDROID_EXTRA_LIBS ${LIBMPV_RUNTIME_LIBS})

    # Android ships no OpenSSL that Qt can link against, so Qt Network has no
    # TLS backend and every HTTPS request fails. Nothing else needs it, which
    # is why this only became a problem once TMDB arrived.
    option(MM_ANDROID_OPENSSL "Bundle OpenSSL so Qt Network can do TLS" ON)

    if(MM_ANDROID_OPENSSL)
        set(MM_ANDROID_OPENSSL_DIR
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/android_openssl"
            CACHE PATH "Local KDAB android_openssl checkout, if you have one")

        if(NOT EXISTS "${MM_ANDROID_OPENSSL_DIR}/android_openssl.cmake")
            message(STATUS "android_openssl not in third_party, fetching it")
            include(FetchContent)
            FetchContent_Declare(android_openssl
                DOWNLOAD_EXTRACT_TIMESTAMP TRUE
                URL https://github.com/KDAB/android_openssl/archive/refs/heads/master.zip)
            FetchContent_MakeAvailable(android_openssl)
            set(MM_ANDROID_OPENSSL_DIR "${android_openssl_SOURCE_DIR}")
        endif()

        if(EXISTS "${MM_ANDROID_OPENSSL_DIR}/android_openssl.cmake")
            include("${MM_ANDROID_OPENSSL_DIR}/android_openssl.cmake")
            add_android_openssl_libraries(appmakimedia)
            message(STATUS "Android OpenSSL from ${MM_ANDROID_OPENSSL_DIR}")
        else()
            message(WARNING
                "Android OpenSSL was not found. The app builds and plays video "
                "as normal, but every HTTPS request fails, so TMDB matching "
                "will not work on this build.")
        endif()
    endif()
endif()

target_link_libraries(appmakimedia
    PRIVATE Qt6::Quick Qt6::Qml Qt6::OpenGL Qt6::Concurrent Qt6::Sql Qt6::Network
            Libmpv::Libmpv
            makimedia_core
)

if(MM_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(WIN32 AND LIBMPV_RUNTIME_DLL)
    add_custom_command(TARGET appmakimedia POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LIBMPV_RUNTIME_DLL}"
            "$<TARGET_FILE_DIR:appmakimedia>"
    )
endif()

include(GNUInstallDirs)

if(WIN32)
    set(CMAKE_INSTALL_BINDIR ".")
endif()

install(TARGETS appmakimedia
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(WIN32 AND LIBMPV_RUNTIME_DLL)
    install(FILES "${LIBMPV_RUNTIME_DLL}"
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/licenses/"
    DESTINATION "${CMAKE_INSTALL_BINDIR}/licenses"
    FILES_MATCHING PATTERN "*.txt"
)

qt_generate_deploy_qml_app_script(
    TARGET appmakimedia
    OUTPUT_SCRIPT MM_DEPLOY_SCRIPT
    NO_UNSUPPORTED_PLATFORM_ERROR
    NO_TRANSLATIONS

    EXCLUDE_PLUGIN_TYPES
        qmltooling
        vectorimageformats
        platforminputcontexts
        generic

    EXCLUDE_PLUGINS
        qpdf qgif qico qicns qtga qtiff qwbmp qwebp
        qsqlibase qsqlmimer qsqloci qsqlodbc qsqlpsql

    DEPLOY_TOOL_OPTIONS --no-system-d3d-compiler
)

if(WIN32)
    install(CODE "set(QT_DEPLOY_BIN_DIR \".\")")
endif()

install(SCRIPT ${MM_DEPLOY_SCRIPT})

if(WIN32)
    set(MM_CRT_DLLS "")
    if(MSVC)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(_crt_arch "x64")
        else()
            set(_crt_arch "x86")
        endif()

        set(_vc_dir "${CMAKE_CXX_COMPILER}")
        foreach(_step RANGE 6)
            get_filename_component(_vc_dir "${_vc_dir}" DIRECTORY)
        endforeach()

        file(GLOB _crt_dirs
            "${_vc_dir}/Redist/MSVC/*/${_crt_arch}/Microsoft.VC*.CRT")
        list(SORT _crt_dirs)
        list(REVERSE _crt_dirs)

        foreach(_crt_dir IN LISTS _crt_dirs)
            file(GLOB MM_CRT_DLLS "${_crt_dir}/*.dll")
            if(MM_CRT_DLLS)
                message(STATUS "MSVC runtime: ${_crt_dir}")
                break()
            endif()
        endforeach()
    endif()

    if(MM_CRT_DLLS)
        install(FILES ${MM_CRT_DLLS} DESTINATION ${CMAKE_INSTALL_BINDIR})
    else()
        message(WARNING
            "No MSVC runtime DLLs found: the installer will need the Visual "
            "C++ redistributable already present on the target machine.")
    endif()

    set(MM_EXECUTABLE_NAME "appmakimedia.exe")

    set(MM_NSIS_OPEN_WITH_PROGIDS "")
    set(MM_NSIS_SUPPORTED_TYPES "")
    set(MM_NSIS_DELETE_PROGIDS "")
    set(MM_WIX_ASSOCIATIONS "")
    set(MM_MSIX_ASSOCIATIONS "")
    foreach(suffix IN LISTS MM_VIDEO_SUFFIXES)
        string(APPEND MM_MSIX_ASSOCIATIONS
            "              <uap:FileType>.${suffix}</uap:FileType>\n")
        string(APPEND MM_NSIS_OPEN_WITH_PROGIDS
            "WriteRegStr HKCU \"Software\\Classes\\.${suffix}\\OpenWithProgids\" \"Makimedia.Video\" \"\"\n")
        string(APPEND MM_NSIS_SUPPORTED_TYPES
            "WriteRegStr HKCU \"Software\\Classes\\Applications\\${MM_EXECUTABLE_NAME}\\SupportedTypes\" \".${suffix}\" \"\"\n")
        string(APPEND MM_NSIS_DELETE_PROGIDS
            "DeleteRegValue HKCU \"Software\\Classes\\.${suffix}\\OpenWithProgids\" \"Makimedia.Video\"\n")

        string(APPEND MM_WIX_ASSOCIATIONS
            "    <RegistryValue Root=\"HKMU\" Key=\"Software\\Classes\\.${suffix}\\OpenWithProgids\"\n"
            "                   Name=\"Makimedia.Video\" Type=\"string\" Value=\"\" KeyPath=\"no\" />\n"
            "    <RegistryValue Root=\"HKMU\"\n"
            "                   Key=\"Software\\Classes\\Applications\\${MM_EXECUTABLE_NAME}\\SupportedTypes\"\n"
            "                   Name=\".${suffix}\" Type=\"string\" Value=\"\" KeyPath=\"no\" />\n")
    endforeach()

    set(MM_PACKAGING_DIR "${CMAKE_CURRENT_BINARY_DIR}/packaging")
    foreach(script install-associations uninstall-associations)
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/packaging/${script}.nsh.in"
            "${MM_PACKAGING_DIR}/${script}.nsh"
            @ONLY
        )
    endforeach()

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/associations.wxs.in"
        "${MM_PACKAGING_DIR}/associations.wxs"
        @ONLY
    )

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        "${MM_PACKAGING_DIR}/License.txt"
        COPYONLY
    )

    set(MM_MSIX_IDENTITY_NAME "TopicDev.Makimedia")
    set(MM_MSIX_PUBLISHER "CN=5C288D7D-4BF6-4F55-AC01-BA40A2AAEFB9")
    set(MM_MSIX_PUBLISHER_DISPLAY_NAME "TopicDev")
    set(MM_MSIX_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}.0")

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/AppxManifest.xml.in"
        "${MM_PACKAGING_DIR}/AppxManifest.xml"
        @ONLY
    )

    file(GLOB MM_MAKEAPPX_CANDIDATES "${MM_WINSDK_ROOT}/bin/*/x64/makeappx.exe")
    if(MM_MAKEAPPX_CANDIDATES)
        list(SORT MM_MAKEAPPX_CANDIDATES)
        list(GET MM_MAKEAPPX_CANDIDATES -1 MM_MAKEAPPX)

        set(MM_MSIX_STAGE "${CMAKE_CURRENT_BINARY_DIR}/msix")
        set(MM_MSIX_OUTPUT
            "${CMAKE_CURRENT_BINARY_DIR}/Makimedia-${MM_MSIX_VERSION}-x64.msix")

        add_custom_target(msix
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${MM_MSIX_STAGE}"
            COMMAND ${CMAKE_COMMAND}
                --install "${CMAKE_CURRENT_BINARY_DIR}"
                --config $<CONFIG>
                --prefix "${MM_MSIX_STAGE}"
            COMMAND ${CMAKE_COMMAND} -E copy
                "${MM_PACKAGING_DIR}/AppxManifest.xml"
                "${MM_MSIX_STAGE}/AppxManifest.xml"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/packaging/msix-assets"
                "${MM_MSIX_STAGE}/Assets"
            COMMAND "${MM_MAKEAPPX}" pack /o /d "${MM_MSIX_STAGE}" /p "${MM_MSIX_OUTPUT}"
            COMMENT "Packing ${MM_MSIX_OUTPUT}"
            VERBATIM
        )
        add_dependencies(msix appmakimedia)
        message(STATUS "MSIX: ${MM_MAKEAPPX} -> target 'msix'")
    else()
        message(STATUS
            "MSIX: makeappx.exe not found under ${MM_WINSDK_ROOT}/bin/*/x64, "
            "so no 'msix' target. It ships with the Windows SDK.")
    endif()

    set(CPACK_PACKAGE_NAME "Makimedia")
    set(CPACK_PACKAGE_VENDOR "TopicDev")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Video player and library")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "Makimedia")

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(MM_ARCH "win64")
    else()
        set(MM_ARCH "win32")
    endif()
    set(CPACK_PACKAGE_FILE_NAME "Makimedia-${CPACK_PACKAGE_VERSION}-${MM_ARCH}")

    set(CPACK_RESOURCE_FILE_LICENSE "${MM_PACKAGING_DIR}/License.txt")

    set(CPACK_PACKAGE_EXECUTABLES "appmakimedia" "Makimedia")
    set(CPACK_CREATE_DESKTOP_LINKS "appmakimedia")

    set(CPACK_NSIS_PACKAGE_NAME "Makimedia")
    set(CPACK_NSIS_DISPLAY_NAME "Makimedia")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/makimedia.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/makimedia.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "appmakimedia.exe")
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN "appmakimedia.exe")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")

    set(CPACK_NSIS_DEFINES "RequestExecutionLevel user")
    set(CPACK_NSIS_INSTALL_ROOT "$LOCALAPPDATA\\\\Programs")

    function(mm_nsis_include out_var script)
        string(REPLACE "/" "\\\\" native "${MM_PACKAGING_DIR}/${script}")
        set(${out_var} "!include \\\"${native}\\\"" PARENT_SCOPE)
    endfunction()

    mm_nsis_include(CPACK_NSIS_EXTRA_INSTALL_COMMANDS install-associations.nsh)
    mm_nsis_include(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS uninstall-associations.nsh)

    set(CPACK_WIX_UPGRADE_GUID "1EE1FCCF-858E-448E-97A5-833BA2A466B2")
    set(CPACK_WIX_PRODUCT_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/makimedia.ico")
    set(CPACK_WIX_PROGRAM_MENU_FOLDER "Makimedia")
    set(CPACK_WIX_PATCH_FILE "${MM_PACKAGING_DIR}/associations.wxs")
    set(CPACK_WIX_PROPERTY_ARPCOMMENTS "Video player and library")

    set(CPACK_GENERATOR "NSIS")

    include(CPack)
endif()
