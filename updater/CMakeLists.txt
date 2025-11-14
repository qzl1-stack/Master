cmake_minimum_required(VERSION 3.14)

project(updater)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)


find_package(Qt6 COMPONENTS Core Network Widgets Qml Quick REQUIRED)
# find_package(ZLIB REQUIRED)

# 添加可执行文件
qt_add_executable(updater WIN32
    src/main.cpp
    src/updater.cpp
    include/updater.h
)

# 添加 QML 模块和资源
qt_add_qml_module(updater
    URI Updater
    VERSION 1.0
    QML_FILES
        qml/UpdaterWindow.qml
    RESOURCES
        updater.qrc
)

target_include_directories(updater PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 链接 Qt 库
target_link_libraries(updater PRIVATE 
    Qt6::Widgets 
    Qt6::Network 
    Qt6::Core 
    Qt6::Qml 
    Qt6::Quick
)

# --- Windows 平台特定设置：混合部署策略 ---
if(WIN32)
    # 让 CMake 找到 Qt ToolChain 的 bin 路径
    get_filename_component(Qt_BIN_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)

    set(RUNTIME_DLLS
        "${Qt_BIN_DIR}/libstdc++-6.dll"
        "${Qt_BIN_DIR}/libgcc_s_seh-1.dll"
        "${Qt_BIN_DIR}/libwinpthread-1.dll"
    )

    foreach(DLL ${RUNTIME_DLLS})
        add_custom_command(TARGET updater POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${DLL}" "$<TARGET_FILE_DIR:updater>"
            COMMENT "Copy ${DLL}"
        )
    endforeach()

    # windeployqt 负责 Qt 自己的 dll / 插件
    find_program(WINDEPLOYQT_PATH windeployqt HINTS "${Qt6_HOST_BINS}")
    add_custom_command(TARGET updater POST_BUILD
        COMMAND "${WINDEPLOYQT_PATH}"
                --dir "$<TARGET_FILE_DIR:updater>"
                --qml --no-translations --no-compiler-runtime
                --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml" # Use current source dir for QML
                "$<TARGET_FILE:updater>"
    )

    # 设置输出目录
    set_target_properties(updater PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()

install(TARGETS updater
    RUNTIME DESTINATION bin
) 