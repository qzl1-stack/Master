cmake_minimum_required(VERSION 3.16)

project(Master VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用QML语言服务器支持
set(QT_QML_GENERATE_QMLLS_INI ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Core Network QuickControls2 Widgets )

qt_standard_project_setup(REQUIRES 6.5)


qt_add_executable(appMaster
    src/main.cpp
    src/ProjectConfig.h
    src/ProjectConfig.cpp
    src/DataStore.h
    src/DataStore.cpp
    src/IIpcCommunication.h
    src/IpcMessage.cpp
    src/ILogStorage.h
    src/LogEntry.cpp
    src/LogAggregator.cpp
    src/ProcessManager.h
    src/ProcessManager.cpp
    src/MainController.h
    src/MainController.cpp
    src/LocalSocketIpcCommunication.h
    src/LocalSocketIpcCommunication.cpp
    src/IpcCommunicationFactory.cpp
    src/FileLogStorage.h
    src/FileLogStorage.cpp
    src/update_checker.h
    src/update_checker.cpp
    src/FolderDialogHelper.h
    src/FolderDialogHelper.cpp
)

qt_add_qml_module(appMaster
    URI Master
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Maincontroller.qml
        qml/Settings.qml
        qml/WorkspaceSelector.qml
        qml/UpdateNotificationCard.qml
)

set_target_properties(appMaster PROPERTIES
        WIN32_EXECUTABLE TRUE
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_link_libraries(appMaster
    PRIVATE Qt6::Quick Qt6::Core Qt6::Network Qt6::QuickControls2 Qt6::Widgets
)

include(GNUInstallDirs)
install(TARGETS appMaster
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
add_subdirectory(updater)