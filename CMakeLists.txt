cmake_minimum_required(VERSION 3.22)

project(Canopy VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Concurrent Quick QuickControls2 Test)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(canopy
    src/main.cpp
)

qt_add_qml_module(canopy
    URI Canopy
    VERSION 1.0
    QML_FILES
        qml/Main.qml
    SOURCES
        src/layout/TreeLayoutEngine.cpp
        src/layout/TreeLayoutEngine.h
        src/layout/PlanarProjection.cpp
        src/layout/PlanarProjection.h
        src/layout/PerspectiveProjection.cpp
        src/layout/PerspectiveProjection.h
        src/layout/PerspectiveCameraPolicy.cpp
        src/layout/PerspectiveCameraPolicy.h
        src/layout/PerspectivePresentationPolicy.cpp
        src/layout/PerspectivePresentationPolicy.h
        src/layout/ProjectedSurfaceGeometry.cpp
        src/layout/ProjectedSurfaceGeometry.h
        src/layout/ProjectedNode.h
        src/layout/ViewState.h
        src/layout/WorldLayout.h
        src/layout/WorldLayoutEngine.cpp
        src/layout/WorldLayoutEngine.h
        src/filesystem/DirectoryScanner.cpp
        src/filesystem/DirectoryScanner.h
        src/filesystem/DirectoryScanController.cpp
        src/filesystem/DirectoryScanController.h
        src/filesystem/DirectoryContentsModel.cpp
        src/filesystem/DirectoryContentsModel.h
        src/model/DirectoryPresentationPolicy.cpp
        src/model/DirectoryPresentationPolicy.h
        src/model/FilesystemTreeModel.cpp
        src/model/FilesystemTreeModel.h
        src/model/FileNode.h
        src/navigation/NavigationState.cpp
        src/navigation/NavigationState.h
        src/rendering/LabelVisibilityPolicy.cpp
        src/rendering/LabelVisibilityPolicy.h
        src/rendering/TreeSceneItem.cpp
        src/rendering/TreeSceneItem.h
        src/theme/ThemeProfile.h
        src/theme/ThemeRegistry.cpp
        src/theme/ThemeRegistry.h
        src/theme/ThemeTokens.cpp
        src/theme/ThemeTokens.h
        src/themes/sakura/SakuraMorningTheme.cpp
        src/themes/sakura/SakuraMorningTheme.h
)

target_link_libraries(canopy PRIVATE
    Qt6::Concurrent
    Qt6::Quick
    Qt6::QuickControls2
)

target_include_directories(canopy PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/layout
    ${CMAKE_CURRENT_SOURCE_DIR}/src/filesystem
    ${CMAKE_CURRENT_SOURCE_DIR}/src/model
    ${CMAKE_CURRENT_SOURCE_DIR}/src/navigation
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering
    ${CMAKE_CURRENT_SOURCE_DIR}/src/theme
    ${CMAKE_CURRENT_SOURCE_DIR}/src/themes/sakura
)

include(CTest)

if(BUILD_TESTING)
    qt_add_executable(canopy_unit_tests
        tests/unit/TreeLayoutEngineTest.cpp
        src/layout/TreeLayoutEngine.cpp
        src/layout/PlanarProjection.cpp
        src/layout/PerspectiveProjection.cpp
        src/layout/PerspectiveCameraPolicy.cpp
        src/layout/PerspectivePresentationPolicy.cpp
        src/layout/ProjectedSurfaceGeometry.cpp
        src/layout/WorldLayoutEngine.cpp
        src/model/DirectoryPresentationPolicy.cpp
        src/model/FakeTreeData.cpp
        src/navigation/NavigationState.cpp
        src/rendering/LabelVisibilityPolicy.cpp
        src/theme/ThemeRegistry.cpp
        src/themes/sakura/SakuraMorningTheme.cpp
    )
    target_include_directories(canopy_unit_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/layout
        ${CMAKE_CURRENT_SOURCE_DIR}/src/model
        ${CMAKE_CURRENT_SOURCE_DIR}/src/navigation
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering
        ${CMAKE_CURRENT_SOURCE_DIR}/src/theme
        ${CMAKE_CURRENT_SOURCE_DIR}/src/themes/sakura
    )
    target_link_libraries(canopy_unit_tests PRIVATE Qt6::Gui Qt6::Test)
    add_test(NAME canopy_unit_tests COMMAND canopy_unit_tests)

    qt_add_executable(canopy_directory_scanner_tests
        tests/unit/DirectoryScannerTest.cpp
        src/filesystem/DirectoryContentsModel.cpp
        src/filesystem/DirectoryScanController.cpp
        src/filesystem/DirectoryScanner.cpp
        src/model/DirectoryPresentationPolicy.cpp
        src/model/FilesystemTreeModel.cpp
    )
    target_include_directories(canopy_directory_scanner_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/filesystem
        ${CMAKE_CURRENT_SOURCE_DIR}/src/model
    )
    target_link_libraries(canopy_directory_scanner_tests PRIVATE
        Qt6::Concurrent
        Qt6::Test
    )
    add_test(
        NAME canopy_directory_scanner_tests
        COMMAND canopy_directory_scanner_tests
    )

    qt_add_executable(canopy_tree_scene_tests
        tests/unit/TreeSceneItemTest.cpp
        src/layout/TreeLayoutEngine.cpp
        src/layout/PlanarProjection.cpp
        src/layout/PerspectiveProjection.cpp
        src/layout/PerspectiveCameraPolicy.cpp
        src/layout/PerspectivePresentationPolicy.cpp
        src/layout/ProjectedSurfaceGeometry.cpp
        src/layout/WorldLayoutEngine.cpp
        src/filesystem/DirectoryContentsModel.cpp
        src/filesystem/DirectoryScanController.cpp
        src/filesystem/DirectoryScanner.cpp
        src/model/DirectoryPresentationPolicy.cpp
        src/model/FilesystemTreeModel.cpp
        src/navigation/NavigationState.cpp
        src/rendering/LabelVisibilityPolicy.cpp
        src/rendering/TreeSceneItem.cpp
        src/theme/ThemeRegistry.cpp
        src/themes/sakura/SakuraMorningTheme.cpp
    )
    target_include_directories(canopy_tree_scene_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/layout
        ${CMAKE_CURRENT_SOURCE_DIR}/src/filesystem
        ${CMAKE_CURRENT_SOURCE_DIR}/src/model
        ${CMAKE_CURRENT_SOURCE_DIR}/src/navigation
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering
        ${CMAKE_CURRENT_SOURCE_DIR}/src/theme
        ${CMAKE_CURRENT_SOURCE_DIR}/src/themes/sakura
    )
    target_link_libraries(canopy_tree_scene_tests PRIVATE
        Qt6::Concurrent
        Qt6::Quick
        Qt6::Test
    )
    add_test(NAME canopy_tree_scene_tests COMMAND canopy_tree_scene_tests)
    set_tests_properties(canopy_tree_scene_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()

install(TARGETS canopy
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
