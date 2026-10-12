set(MARK_SHOT_REGION_DETECTION_SOURCES
    src/region_detection/image_gray.cpp
    src/region_detection/image_gray.h
    src/region_detection/image_filters.cpp
    src/region_detection/image_filters.h
    src/region_detection/image_components.cpp
    src/region_detection/image_components.h
    src/region_detection/region_detection.cpp
    src/region_detection/region_detection_internal.h
    src/region_detection/region_detector.cpp
    src/region_detection/region_detector.h
    src/region_detection/region_edge_field.cpp
    src/region_detection/region_proposals.cpp
    src/region_detection/region_scoring.cpp
)

# Debug 下像素循环没有 -O 会比 babry/imageproc 慢一个数量级；区域识别源单独开优化。
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set_source_files_properties(
        ${MARK_SHOT_REGION_DETECTION_SOURCES}
        PROPERTIES
            COMPILE_OPTIONS "-O2"
    )
endif()
