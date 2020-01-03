include_directories (${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_BINARY_DIR} ${QT_INCLUDE_DIRS} ${LIBDV_INCLUDE_DIR})
set(catdv_bin_SRCS
    BufferedWriter.cpp
    Clip.cpp
    Main.cpp
    Misc.cpp
    Frame.cpp
    Wav.cpp
    YUV420Extractor.cpp
    )
add_definitions(${QT_DEFINITIONS} -DHAVE_CONFIG_H -DHAVE_LIBDV -D_LARGEFILE64_SOURCE)
add_executable(catdv_bin ${catdv_bin_SRCS})
set_target_properties(catdv_bin PROPERTIES OUTPUT_NAME catdv)
target_link_libraries(catdv_bin ${QT_LIBRARIES} ${LIBDV_LIBRARIES})
install(TARGETS catdv_bin DESTINATION bin)
