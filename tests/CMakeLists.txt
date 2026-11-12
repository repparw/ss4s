if (NOT SS4S_ENABLE_TESTS)
    return()
endif ()

enable_testing()

message(STATUS "Configuring SS4S tests")
set(TEST_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../include ${CMAKE_CURRENT_SOURCE_DIR}/../src)
set(TEST_LIBRARIES pthread)

function(ss4s_add_test NAME)
    cmake_parse_arguments(TEST "" "" "SOURCES;ARGS;LIBRARIES" ${ARGN})
    set(SS4S_TEST_CASE "ss4s_test_${NAME}")
    add_executable(${SS4S_TEST_CASE} ${TEST_SOURCES})
    add_test(${NAME} ${SS4S_TEST_CASE} ${TEST_ARGS})
    target_include_directories(${SS4S_TEST_CASE} PRIVATE ${TEST_INCLUDES})
    target_link_libraries(${SS4S_TEST_CASE} PRIVATE ss4s ${TEST_LIBRARIES})
    set_target_properties(${SS4S_TEST_CASE} PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH_USE_LINK_PATH TRUE
            INSTALL_RPATH "${SS4S_MODULE_LIBRARY_OUTPUT_DIRECTORY}")
    set_tests_properties(${NAME} PROPERTIES SKIP_RETURN_CODE 127)
    set(SS4S_TEST_CASE ${SS4S_TEST_CASE} PARENT_SCOPE)
endfunction()

configure_file(../samples/av-threaded/sample.pcm ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)
configure_file(../samples/av-threaded/sample.h264 ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)

ss4s_add_test(initialization SOURCES test_initialization.c)

ss4s_add_test(feed_guard SOURCES test_feed_guard.c)

ss4s_add_test(video_concurrent_lifecycle_dummy SOURCES test_video_concurrent_lifecycle.c ARGS dummy)

ss4s_add_test(pcm_playback_sdl SOURCES test_pcm_playback.c ARGS sdl)
ss4s_add_test(pcm_playback_alsa SOURCES test_pcm_playback.c ARGS alsa)
ss4s_add_test(pcm_playback_pulse SOURCES test_pcm_playback.c ARGS pulse)

ss4s_add_test(pcm_playback_mmal SOURCES test_h264_playback.c ARGS mmal)

ss4s_add_test(h264_playback_lgnc SOURCES test_h264_playback.c ARGS lgnc)
ss4s_add_test(ndl_diag_stats SOURCES test_ndl_diag_stats.c)

if (TARGET ss4s-module-ndl-webos5)
    include(WebOSUserland)
    foreach(mode IN ITEMS on off)
        add_executable(ndl_diag_lifecycle_${mode} test_ndl_diag_lifecycle.c
            ../modules/webos/ndl/webos5/ndl_player.c ../modules/webos/ndl/webos5/ndl_video.c)
        target_include_directories(ndl_diag_lifecycle_${mode} PRIVATE
            ../modules/interface/include ../include ../modules/webos/utils ${WEBOS_USERLAND_INCLUDE_DIR}/libndl-media)
        target_link_libraries(ndl_diag_lifecycle_${mode} PRIVATE pthread)
        if (mode STREQUAL "on")
            target_sources(ndl_diag_lifecycle_${mode} PRIVATE ../modules/webos/ndl/webos5/ndl_diagnostics.c)
            target_compile_definitions(ndl_diag_lifecycle_${mode} PRIVATE SS4S_NDL_PRESENT_DIAGNOSTICS)
        endif ()
        add_test(NAME ndl_diag_lifecycle_${mode} COMMAND ndl_diag_lifecycle_${mode})
    endforeach ()
endif ()
