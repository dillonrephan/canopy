cmake_minimum_required(VERSION 3.22)

project(Canopy VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2 Test)

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
        src/model/FakeTreeData.cpp
        src/model/FakeTreeData.h
        src/model/FileNode.h
        src/rendering/TreeSceneItem.cpp
        src/rendering/TreeSceneItem.h
)

target_link_libraries(canopy PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
)

target_include_directories(canopy PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/layout
    ${CMAKE_CURRENT_SOURCE_DIR}/src/model
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering
)

include(CTest)

if(BUILD_TESTING)
    qt_add_executable(canopy_unit_tests
        tests/unit/TreeLayoutEngineTest.cpp
        src/layout/TreeLayoutEngine.cpp
        src/model/FakeTreeData.cpp
    )
    target_include_directories(canopy_unit_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/layout
        ${CMAKE_CURRENT_SOURCE_DIR}/src/model
    )
    target_link_libraries(canopy_unit_tests PRIVATE Qt6::Test)
    add_test(NAME canopy_unit_tests COMMAND canopy_unit_tests)
endif()

install(TARGETS canopy
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
