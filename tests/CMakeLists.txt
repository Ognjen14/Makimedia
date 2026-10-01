set(MM_TESTS
    tst_filenameparser
    tst_matchscorer
    tst_matchdecision
    tst_tmdbask
    tst_showgrouping
    tst_database
    tst_mediarepository
    tst_filerepository
    tst_browseoptions
    tst_rowplacement
    tst_listpatch
    tst_unmatchedshowmodel
    tst_removedfilemodel
    tst_showepisodemodel
    tst_tracklistmodel
    tst_videotraits
    tst_tmdbjsonreader
    tst_tmdbimageurl
    tst_tmdbrequestbuilder
    tst_appsettings
    tst_tmdbreplyerror
    tst_mediahandle
    tst_postercache
    tst_playbackstaterepository
    tst_textfold
    tst_folderrepository
    tst_subtitlerepository
    tst_subtitlenaming
    tst_subtitleencoding
    tst_listtext
    tst_genreshelf
    tst_titlearrangement
    tst_singleinstance
    tst_tmdbcollection
    tst_collectionshelf
    tst_universecatalog
    tst_collectionpage
    tst_streamserver
    tst_discovery
    tst_librarysnapshot
    tst_mirror
    tst_subtitlefinder
    tst_sessiontable
    tst_packagedformats
)

foreach(test IN LISTS MM_TESTS)
    add_executable(${test} ${test}.cpp)

    target_link_libraries(${test}
        PRIVATE makimedia_core Qt6::Test
    )

    add_test(NAME ${test} COMMAND ${test})
endforeach()

target_sources(tst_unmatchedshowmodel PRIVATE
    ${PROJECT_SOURCE_DIR}/src/Library/UnmatchedShowModel.cpp
    ${PROJECT_SOURCE_DIR}/include/Library/UnmatchedShowModel.h
)
target_sources(tst_removedfilemodel PRIVATE
    ${PROJECT_SOURCE_DIR}/src/Library/RemovedFileModel.cpp
    ${PROJECT_SOURCE_DIR}/include/Library/RemovedFileModel.h
)
target_sources(tst_showepisodemodel PRIVATE
    ${PROJECT_SOURCE_DIR}/src/Library/ShowEpisodeModel.cpp
    ${PROJECT_SOURCE_DIR}/include/Library/ShowEpisodeModel.h
)
target_compile_definitions(tst_universecatalog PRIVATE
    MM_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
string(JOIN " " MM_PACKAGED_VIDEO_SUFFIXES ${MM_VIDEO_SUFFIXES})
target_compile_definitions(tst_packagedformats PRIVATE
    MM_PACKAGED_VIDEO_SUFFIXES="${MM_PACKAGED_VIDEO_SUFFIXES}"
)
target_sources(tst_tmdbcollection PRIVATE
    ${PROJECT_SOURCE_DIR}/src/Metadata/TmdbClient.cpp
    ${PROJECT_SOURCE_DIR}/include/Metadata/TmdbClient.h
)
target_sources(tst_tracklistmodel PRIVATE
    ${PROJECT_SOURCE_DIR}/src/Player/TrackListModel.cpp
    ${PROJECT_SOURCE_DIR}/include/Player/TrackListModel.h
)

if(NOT ANDROID)
    set(MM_MPV_TESTS
        tst_thumbnailservice
        tst_probeservice
    )

    foreach(test IN LISTS MM_MPV_TESTS)
        add_executable(${test} ${test}.cpp)

        target_link_libraries(${test}
            PRIVATE makimedia_core Qt6::Test Qt6::Concurrent Libmpv::Libmpv
        )

        add_test(NAME ${test} COMMAND ${test})

        if(WIN32 AND LIBMPV_RUNTIME_DLL)
            add_custom_command(TARGET ${test} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${LIBMPV_RUNTIME_DLL}"
                    "$<TARGET_FILE_DIR:${test}>"
            )
        endif()
    endforeach()

    target_sources(tst_thumbnailservice PRIVATE
        ${PROJECT_SOURCE_DIR}/src/Library/ThumbnailService.cpp
        ${PROJECT_SOURCE_DIR}/include/Library/ThumbnailService.h
    )
    target_sources(tst_probeservice PRIVATE
        ${PROJECT_SOURCE_DIR}/src/Library/ProbeService.cpp
        ${PROJECT_SOURCE_DIR}/include/Library/ProbeService.h
    )
endif()
