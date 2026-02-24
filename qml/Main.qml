import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import Master // 导入 Master 模块以使用 WorkspaceSelector 组件

/**
 * @brief Main 主界面管理器
 *
 * 作为整个应用程序的界面管理中心，负责：
 * 1. 管理工作区选择和主控制器界面的切换
 * 2. 提供统一的导航和布局框架
 * 3. 处理全局的界面状态和主题管理
 * 4. 管理工作目录的选择和历史记录
 */
ApplicationWindow {
    id: mainWindow
    title: "JT Studio"
    width: 1400
    height: 900
    visible: true
    visibility: Window.Maximized

    // 去除原生窗口栏，实现无边框
    flags: Qt.Window | Qt.FramelessWindowHint

    // ==================== 自定义标题栏 (黑色系，合并了菜单栏和窗口控件) ====================
    header: Rectangle {
        id: titleBar
        height: 40
        color: "#1e1e1e" // 黑色标题栏直接设色，不依赖背景层

        RowLayout {
            anchors.fill: parent
            spacing: 0

            // 应用图标（显示在菜单栏最左侧）
            Image {
                id: appIcon
                source: "qrc:/qt/qml/Master/src/app.ico"
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                Layout.leftMargin: 20
                Layout.rightMargin: 4
                fillMode: Image.PreserveAspectFit
            }

            // 还原原生的菜单栏
            MenuBar {
                id: customMenuBar
                Layout.fillHeight: true
                background: Rectangle {
                    color: "transparent"
                }

                delegate: MenuBarItem {
                    id: menuBarItem
                    contentItem: Text {
                        text: menuBarItem.text
                        font.pixelSize: 14   // 缩小字体
                        color: "white"
                        opacity: menuBarItem.highlighted ? 1.0 : 0.8
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                    background: Rectangle {
                        color: menuBarItem.highlighted ? "#333333" : "transparent"
                    }
                }

                Menu {
                    title: "关于"
                    implicitWidth: 130

                    background: Rectangle {
                        implicitWidth: 130
                        color: "#1e1e1e"
                        border.color: "#3a3a3a"
                        border.width: 1
                        radius: 4
                    }

                    MenuItem {
                        id: aboutItem1
                        text: "关于 JT Studio"
                        implicitHeight: 32
                        contentItem: Text {
                            text: aboutItem1.text
                            font.pixelSize: 13
                            color: "white"
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 12
                        }
                        background: Rectangle {
                            color: aboutItem1.highlighted ? "#2a2a2a" : "transparent"
                        }
                        onTriggered: aboutDialog.visible = true
                    }

                    MenuSeparator {
                        contentItem: Rectangle {
                            implicitHeight: 1
                            color: "#3a3a3a"
                        }
                    }

                    MenuItem {
                        id: aboutItem2
                        text: "退出"
                        implicitHeight: 32
                        contentItem: Text {
                            text: aboutItem2.text
                            font.pixelSize: 13
                            color: "white"
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 12
                        }
                        background: Rectangle {
                            color: aboutItem2.highlighted ? "#2a2a2a" : "transparent"
                        }
                        onTriggered: Qt.quit()
                    }
                }

                Menu {
                    title: "语言切换"
                    implicitWidth: 130

                    background: Rectangle {
                        implicitWidth: 130
                        color: "#1e1e1e"
                        border.color: "#3a3a3a"
                        border.width: 1
                        radius: 4
                    }

                    MenuItem {
                        id: langItem1
                        text: "中文"
                        implicitHeight: 32
                        contentItem: Text {
                            text: langItem1.text
                            font.pixelSize: 13
                            color: "white"
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 12
                        }
                        background: Rectangle {
                            color: langItem1.highlighted ? "#2a2a2a" : "transparent"
                        }
                        onTriggered: console.log("切换到中文")
                    }

                    MenuSeparator {
                        contentItem: Rectangle {
                            implicitHeight: 1
                            color: "#3a3a3a"
                        }
                    }

                    MenuItem {
                        id: langItem2
                        text: "English"
                        implicitHeight: 32
                        contentItem: Text {
                            text: langItem2.text
                            font.pixelSize: 13
                            color: "white"
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 12
                        }
                        background: Rectangle {
                            color: langItem2.highlighted ? "#2a2a2a" : "transparent"
                        }
                        onTriggered: console.log("Switch to English")
                    }
                }
            }

            // 占位区域，同时处理拖动和双击最大化
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Text {
                    anchors.centerIn: parent
                    text: mainWindow.title
                    font.pixelSize: 14
                    color: "white" // 黑色标题栏使用白色文字
                    opacity: 0.9
                }

                DragHandler {
                    target: null // 不移动Item本身，只触发窗口系统的移动
                    onActiveChanged: if (active) {
                        mainWindow.startSystemMove();
                    }
                }
            }

            // 右侧窗口控制按钮区
            RowLayout {
                Layout.fillHeight: true
                spacing: 0

                Button {
                    Layout.preferredWidth: 46
                    Layout.fillHeight: true
                    text: "—"
                    font.pixelSize: 14
                    contentItem: Text {
                        text: parent.text
                        font: parent.font
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#333333" : "transparent"
                    }
                    onClicked: mainWindow.showMinimized()
                }

                Button {
                    Layout.preferredWidth: 46
                    Layout.fillHeight: true
                    text: mainWindow.visibility === Window.Maximized ? "❐" : "☐"
                    font.pixelSize: 14
                    contentItem: Text {
                        text: parent.text
                        font: parent.font
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#333333" : "transparent"
                    }
                    onClicked: {
                        if (mainWindow.visibility === Window.Maximized) {
                            mainWindow.showNormal();
                        } else {
                            mainWindow.showMaximized();
                        }
                    }
                }

                Button {
                    Layout.preferredWidth: 46
                    Layout.fillHeight: true
                    text: "✕"
                    font.pixelSize: 16
                    background: Rectangle {
                        color: parent.hovered ? "#e81123" : "transparent"
                    }
                    contentItem: Text {
                        text: parent.text
                        font: parent.font
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: mainWindow.close()
                }
            }
        }
    }

    // ==================== 属性定义 ====================
    property bool workspaceSelected: false
    property string currentWorkspacePath: ""
    property int currentViewIndex: 0

    // ==================== 主容器布局 ====================
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ==================== 主内容 ====================
        StackView {
            id: mainStackView
            Layout.fillWidth: true
            Layout.fillHeight: true

            // 初始显示工作区选择器
            initialItem: workspaceSelector
        }
    }

    Rectangle {
        id: windowBorderOverlay
        parent: Overlay.overlay
        anchors.fill: parent
        color: "transparent"
        radius: mainWindow.visibility === Window.Maximized ? 0 : 8
        border.color: mainWindow.visibility === Window.Maximized ? "transparent" : "#3a3a3a"
        border.width: 1
        z: 9999
        antialiasing: true
        smooth: true
    }

    // ==================== 关于对话框 ====================
    AboutDialog {
        id: aboutDialog
    }

    // ==================== 更新通知卡片 ====================
    UpdateNotificationCard {
        id: updateNotificationCard
        anchors {
            left: parent.left
            bottom: parent.bottom
            leftMargin: 20
            bottomMargin: 20
        }
        visible: false
        z: 1000

        // 信号处理
        onUpdateClicked: {
            console.log("用户点击了立即更新按钮");
            if (typeof mainControllerInstance !== 'undefined' && mainControllerInstance) {
                console.log("调用MainController.CheckForUpdates()");
                mainControllerInstance.CheckForUpdates();
            }
            hide();
        }

        onCloseClicked: {
            console.log("用户关闭了更新通知");
            hide();
        }

        onLaterClicked: {
            console.log("用户选择稍后提醒");
            hide();
        }
    }

    // ==================== UpdateChecker信号连接 ====================
    Connections {
        target: updateCheckerInstance

        function onNewVersionFound(version, notes, downloadUrl, currentVer) {
            console.log("发现新版本:", version, "当前版本:", currentVer);
            // 确保属性在主线程中设置
            Qt.callLater(function () {
                updateNotificationCard.newVersion = version;
                updateNotificationCard.releaseNotes = notes;
                updateNotificationCard.currentVersion = currentVer;
                updateNotificationCard.show();
            });
        }

        function onUpdateCheckCompleted(hasUpdate) {
            console.log("更新检查完成，有更新:", hasUpdate);
        }

        function onUpdateCheckFailed(error) {
            console.log("更新检查失败:", error);
        }
    }

    // ==================== 工作区选择器组件 ====================
    Component {
        id: workspaceSelector

        WorkspaceSelector {
            mainController: typeof mainControllerInstance !== 'undefined' ? mainControllerInstance : null

            onWorkspaceSelected: function (workspacePath) {
                console.log("选择工作区: " + workspacePath);
                currentWorkspacePath = workspacePath;

                // 切换到主控制器界面
                mainStackView.push(mainControllerComponent);
            }
        }
    }

    // ==================== 主控制器组件 ====================
    Component {
        id: mainControllerComponent

        // 主控制器界面 (直接显示，无顶部工具栏)
        Maincontroller {
            // 将MainController C++实例传递给QML界面
            mainController: typeof mainControllerInstance !== 'undefined' ? mainControllerInstance : null

            Component.onCompleted: {
                // 设置工作目录到MainController (在初始化之前)
                if (mainController && mainWindow.currentWorkspacePath) {
                    mainController.SetWorkspaceDirectory(mainWindow.currentWorkspacePath);
                    currentWorkspacePath = mainWindow.currentWorkspacePath;
                    console.log("[Main] MainController工作目录已设置: " + mainWindow.currentWorkspacePath);
                    appendLog("MainController工作目录已设置: " + mainWindow.currentWorkspacePath);
                }

                // 自动初始化和启动系统
                console.log("自动初始化系统...");
                if (mainController.Initialize()) {
                    systemInitialized = true; // 直接在 Maincontroller 实例上设置属性
                    appendLog("系统自动初始化成功");
                    appendLog("当前工作区: " + mainWindow.currentWorkspacePath);

                    console.log("自动启动系统...");
                    if (mainController.Start()) {
                        systemStarted = true; // 直接在 Maincontroller 实例上设置属性
                        currentSystemStatus = "运行中"; // 直接在 Maincontroller 实例上设置属性
                        appendLog("系统自动启动成功");
                        updateProcessList();
                    } else {
                        appendLog("系统自动启动失败");
                    }
                } else {
                    appendLog("系统自动初始化失败");
                }
            }
        }
    }

    /**
     * @brief 获取工作区名称
     */
    function getWorkspaceName(path) {
        if (!path)
            return "";
        var parts = path.split(/[\/\\]/);
        return parts[parts.length - 1] || path;
    }

    // ==================== 初始化和清理 ====================

    Component.onCompleted: {
        console.log("Main界面管理器已初始化");
        console.log("显示工作区选择器");

        // 确保窗口获得焦点以响应键盘事件
        mainWindow.requestActivate();
    }

    Component.onDestruction: {
        console.log("Main界面管理器正在销毁");
    }

    // ==================== 窗口事件处理 ====================

    onClosing: {
        console.log("应用程序即将关闭");

        // 这里可以添加清理逻辑，比如保存窗口状态、清理资源等
    }
}
