qt_add_executable(mark-shot-arrow-rendering-test
    tests/arrow_rendering_test.cpp
    src/annotation/arrow_rendering.cpp
    src/line_skeleton_path.cpp
)
target_include_directories(mark-shot-arrow-rendering-test PRIVATE src)
target_link_libraries(mark-shot-arrow-rendering-test PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME arrow-rendering COMMAND mark-shot-arrow-rendering-test)

qt_add_executable(mark-shot-ocr-result-window-geometry-test
    tests/ocr_result_window_geometry_test.cpp
    src/ocr_result_window/ocr_result_window_geometry.cpp
    src/ocr_result_window/ocr_result_window_geometry.h
)
target_include_directories(mark-shot-ocr-result-window-geometry-test PRIVATE src)
target_link_libraries(mark-shot-ocr-result-window-geometry-test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)
add_test(NAME ocr-result-window-geometry COMMAND mark-shot-ocr-result-window-geometry-test)

qt_add_executable(mark-shot-selection-adjustment-test
    tests/selection_adjustment_test.cpp
    src/selection_adjustment.cpp
    src/selection_cursor_nudge.cpp
)
target_include_directories(mark-shot-selection-adjustment-test PRIVATE src)
target_link_libraries(mark-shot-selection-adjustment-test PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME selection-adjustment COMMAND mark-shot-selection-adjustment-test)

qt_add_executable(mark-shot-ocr-result-window-config-test
    tests/ocr_result_window_config_test.cpp
    src/ocr_result_window/ocr_result_window_config.cpp
)
target_include_directories(mark-shot-ocr-result-window-config-test PRIVATE src)
target_link_libraries(mark-shot-ocr-result-window-config-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME ocr-result-window-config COMMAND mark-shot-ocr-result-window-config-test)

qt_add_executable(mark-shot-ocr-text-pane-test
    tests/ocr_text_pane_test.cpp
    src/ocr_result_window/ocr_text_pane.cpp
    src/ocr_result_window/ocr_text_pane.h
    src/ocr_result_window/ocr_result_window_style.cpp
    src/ui/icons.cpp
    src/ui/theme.cpp
)
target_include_directories(mark-shot-ocr-text-pane-test PRIVATE src)
target_link_libraries(mark-shot-ocr-text-pane-test PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME ocr-text-pane COMMAND mark-shot-ocr-text-pane-test)

qt_add_executable(mark-shot-disclosure-section-test
    tests/disclosure_section_test.cpp
    src/ui/disclosure_section.cpp
    src/ui/disclosure_section.h
    src/ui/theme.cpp
)
target_include_directories(mark-shot-disclosure-section-test PRIVATE src)
target_link_libraries(mark-shot-disclosure-section-test PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME disclosure-section COMMAND mark-shot-disclosure-section-test)

qt_add_executable(mark-shot-window-resize-grip-test
    tests/window_resize_grip_test.cpp
    src/ui/window_resize_grip.cpp
    src/ui/window_resize_grip.h
)
target_include_directories(mark-shot-window-resize-grip-test PRIVATE src)
target_link_libraries(mark-shot-window-resize-grip-test PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME window-resize-grip COMMAND mark-shot-window-resize-grip-test)

qt_add_executable(mark-shot-interaction-cursor-test
    tests/interaction_cursor_test.cpp
    src/ui/interaction_cursor.cpp
    src/ui/interaction_cursor.h
)
target_include_directories(mark-shot-interaction-cursor-test PRIVATE src)
target_link_libraries(mark-shot-interaction-cursor-test PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME interaction-cursor COMMAND mark-shot-interaction-cursor-test -o -,txt)

qt_add_executable(mark-shot-selection-aspect-ratio-test
    tests/selection_aspect_ratio_test.cpp
    src/selection_aspect/selection_aspect_ratio.cpp
    src/selection_aspect/selection_aspect_ratio.h
)
target_include_directories(mark-shot-selection-aspect-ratio-test PRIVATE src)
target_link_libraries(mark-shot-selection-aspect-ratio-test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)
add_test(NAME selection-aspect-ratio COMMAND mark-shot-selection-aspect-ratio-test)

qt_add_executable(mark-shot-annotation-filters-test
    tests/annotation_filters_test.cpp
    src/annotation_filters/image_filter_effects.cpp
    src/annotation_filters/image_filter_effects.h
    src/annotation_filters/spotlight_overlay.cpp
    src/annotation_filters/spotlight_overlay.h
)
target_include_directories(mark-shot-annotation-filters-test PRIVATE src)
target_link_libraries(mark-shot-annotation-filters-test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)
add_test(NAME annotation-filters COMMAND mark-shot-annotation-filters-test)

qt_add_executable(mark-shot-ocr-table-layout-test
    tests/ocr_table_layout_test.cpp
    src/ocr_result.cpp
    src/ocr_result.h
    src/ocr_table/ocr_table_layout.cpp
    src/ocr_table/ocr_table_layout.h
)
target_include_directories(mark-shot-ocr-table-layout-test PRIVATE src)
target_link_libraries(mark-shot-ocr-table-layout-test
    PRIVATE
        Qt6::Core
        Qt6::Test
)
add_test(NAME ocr-table-layout COMMAND mark-shot-ocr-table-layout-test)
