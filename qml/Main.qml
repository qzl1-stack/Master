import QtQuick 6.0
import QtQuick.Controls 6.0
import QtQuick.Layouts 6.0
import QtQuick.Window 6.0
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

    // ==================== 属性定义 ====================
    property bool workspaceSelected: false
    property string currentWorkspacePath: ""
    property int currentViewIndex: 0

    // ==================== 主布局 ====================
    StackView {
        id: mainStackView
        anchors.fill: parent

        // 初始显示工作区选择器
        initialItem: workspaceSelector
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
