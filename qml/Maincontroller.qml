import QtQuick 6.0 
import QtQuick.Controls 6.0 
import QtQuick.Layouts 6.0 
import QtQuick.Window 6.0 

/**
* @brief MainController 主控制器界面
*
* 主要功能：
* 1. 系统状态监控和控制
* 2. 子进程生命周期管理
* 3. IP列表和工作目录配置管理
* 4. 实时日志和统计信息展示
* 5. IPC通信状态监控
*/
Rectangle {
    id: mainControllerRoot // 为根组件添加ID
    color: "#ffffff"
    anchors.fill: parent

    // ==================== 属性定义 ====================
    property var mainController: null // MainController C++ 实例引用
    property bool systemInitialized: false
    property bool systemStarted: false
    property string currentSystemStatus: "空闲"
    property var processStatusList: []
    property var ipList: []
    property string currentWorkspacePath: ""

    onCurrentWorkspacePathChanged: {
        if (currentWorkspacePath) {
            console.log("[Maincontroller] 工作目录已更改: " + currentWorkspacePath + "，将异步更新配置。")
            Qt.callLater(updateWorkspaceConfiguration)
        }
    }

    // 定时器用于定期更新状态
    Timer {
        id: statusUpdateTimer
        interval: 2000 // 2秒更新一次
        running: systemStarted
        repeat: true
        onTriggered: updateSystemStatus()
    }

    // ==================== 新的VSCode风格主布局 ====================
    RowLayout {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: logPanel.top
        spacing: 0

        // ==================== 左侧边栏 ====================
        Rectangle {
            id: sideBar
            Layout.preferredWidth: 280
            Layout.minimumWidth: 150 // 设置最小宽度
            Layout.maximumWidth: 500 // 设置最大宽度
            Layout.fillHeight: true
            color: "#2c2c2c"
            border.color: "#3e3e42"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // 侧边栏标题和切换按钮
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 35
                    color: "#37373d"

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 2

                        Button {
                            id: processesTab
                            text: "进程"
                            Layout.preferredWidth: 60
                            Layout.preferredHeight: 20
                            background: Rectangle {
                                color: sideBarCurrentTab === "processes" ? "#007acc" : "transparent"
                                border.color: sideBarCurrentTab === "processes" ? "#007acc" : "#3e3e42"
                                border.width: 1
                                radius: 3
                            }
                            contentItem: Text {
                                text: processesTab.text
                                font.pixelSize: 11
                                color: sideBarCurrentTab === "processes" ? "white" : "#cccccc"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: sideBarCurrentTab = "processes"
                        }

                        Button {
                            id: ipListTab
                            text: "IP列表"
                            Layout.preferredWidth: 60
                            Layout.preferredHeight: 20
                            background: Rectangle {
                                color: sideBarCurrentTab === "iplist" ? "#007acc" : "transparent"
                                border.color: sideBarCurrentTab === "iplist" ? "#007acc" : "#3e3e42"
                                border.width: 1
                                radius: 3
                            }
                            contentItem: Text {
                                text: ipListTab.text
                                font.pixelSize: 11
                                color: sideBarCurrentTab === "iplist" ? "white" : "#cccccc"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: sideBarCurrentTab = "iplist"
                        }
                    }
                }

                // 分隔线
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#3e3e42"
                }

                // 侧边栏内容区域
                StackLayout {
                    id: sideBarContent
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: sideBarCurrentTab === "processes" ? 0 : 1

                    // 可执行程序列表视图
                    Rectangle {
                        color: "#2c2c2c"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 8

                            Text {
                                text: "可执行程序"
                                color: "#cccccc"
                                font.pixelSize: 12
                                font.bold: true
                            }

                            ScrollView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true

                                ListView {
                                    id: sidebarProcessListView
                                    model: processStatusList
                                    delegate: sidebarProcessDelegate
                                    spacing: 2

                                    highlight: Rectangle {
                                        color: "#094771"
                                        radius: 3
                                    }
                                    highlightMoveDuration: 150
                                }
                            }
                        }
                    }

                    // IP列表视图
                    Rectangle {
                        color: "#2c2c2c"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true

                                Text {
                                    text: "IP地址列表"
                                    color: "#cccccc"
                                    font.pixelSize: 12
                                    font.bold: true
                                    Layout.fillWidth: true
                                }

                                RowLayout {
                                    spacing: 5
                                    
                                    Button {
                                        text: "+"
                                        Layout.preferredWidth: 25
                                        Layout.preferredHeight: 20
                                        background: Rectangle {
                                            color: parent.hovered ? "#3e3e42" : "transparent"
                                            border.color: "#3e3e42"
                                            border.width: 1
                                            radius: 3
                                        }
                                        contentItem: Text {
                                            text: parent.text
                                            color: "#cccccc"
                                            font.pixelSize: 12
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        onClicked: newIpDialog.visible = true
                                    }
                                    
                                    Button {
                                        text: "批量"
                                        Layout.preferredWidth: 35
                                        Layout.preferredHeight: 20
                                        background: Rectangle {
                                            color: parent.hovered ? "#3e3e42" : "transparent"
                                            border.color: "#3e3e42"
                                            border.width: 1
                                            radius: 3
                                        }
                                        contentItem: Text {
                                            text: parent.text
                                            color: "#cccccc"
                                            font.pixelSize: 11
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        onClicked: batchAddIpDialog.open()
                                    }
                                }
                            }

                            ScrollView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true

                                ListView {
                                    id: sidebarIpListView
                                    model: ipList
                                    delegate: sidebarIpDelegate
                                    spacing: 2

                                    highlight: Rectangle {
                                        color: "#094771"
                                        radius: 3
                                    }
                                    highlightMoveDuration: 150
                                }
                            }
                        }
                    }
                }

                // 占位符，用于将设置按钮推到底部
                Item {
                    Layout.fillHeight: true
                }

                // 设置按钮
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    color: "#37373d"

                    Button {
                        anchors.centerIn: parent
                        text: "⚙️ 设置"
                        background: Rectangle {
                            color: "transparent"
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "#cccccc"
                            font.pixelSize: 14
                        }
                        onClicked: {
                            // 在这里添加切换到设置页面的逻辑
                            stackLayout.push("Settings.qml", { "mainController": mainController })
                        }
                    }
                }
            }
        }

        // 分隔/调整条
        Rectangle {
            id: handle
            width: 3
            Layout.fillHeight: true
            color: "#3e3e42"

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.SizeHorCursor

                property int lastMouseX: 0

                onPressed: {
                    lastMouseX = mouseX
                }

                onMouseXChanged: {
                    if (pressed) {
                        var delta = mouseX - lastMouseX
                        var newWidth = sideBar.Layout.preferredWidth + delta

                        // 保证宽度在最小和最大值之间
                        if (newWidth >= sideBar.Layout.minimumWidth && newWidth <= sideBar.Layout.maximumWidth) {
                            sideBar.Layout.preferredWidth = newWidth
                        }
                    }
                }
            }
        }

        // ==================== 右侧主内容区域 ====================
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#1e1e1e"

            StackView {
                id: stackLayout
                anchors.fill: parent

                // 初始项是主内容区域
                initialItem: Item {
                    id: mainContentContainer

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        // 顶部标签页区域
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 35
                            color: "#252526"
                            border.color: "#3e3e42"
                            border.width: 1

                            ScrollView {
                                anchors.fill: parent
                                contentHeight: height
                                ScrollBar.vertical.policy: ScrollBar.AlwaysOff

                                RowLayout {
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.left: parent.left
                                    anchors.leftMargin: 8
                                    spacing: 0
                                    height: parent.height

                                    Repeater {
                                        model: openTabs
                                        delegate: tabDelegate
                                    }
                                }
                            }
                        }

                        // 主内容显示区域
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: "#1e1e1e"

                            StackLayout {
                                id: mainContentStack
                                anchors.fill: parent
                                anchors.margins: 10
                                currentIndex: {
                                    if (openTabs.length === 0) return -1;
                                    return Math.max(0, Math.min(currentTabIndex, openTabs.length - 1));
                                }

                                // 动态生成的内容视图将在这里显示
                                Repeater {
                                    model: openTabs
                                    delegate: contentViewDelegate
                                }
                            }

                            // 空状态显示
                            Rectangle {
                                anchors.centerIn: parent
                                width: 300
                                height: 200
                                color: "transparent"
                                visible: openTabs.length === 0
                            }
                        }
                    }
                }
            }
        }
    }
    // 底部实时日志区域（可折叠）
    Rectangle {
        id: logPanel
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: logPanelExpanded ? 200 : 30
        color: "#1e1e1e"
        border.color: "#3e3e42"
        border.width: 1

        // 日志面板标题栏
        Rectangle {
            id: logPanelHeader
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 30
            color: "#37373d"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 8

                Text {
                    text: "实时日志"
                    color: "#cccccc"
                    font.pixelSize: 12
                    font.bold: true
                }

                Rectangle {
                    Layout.fillWidth: true
                }

                Button {
                    text: logPanelExpanded ? "▼" : "▲"
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 16
                    background: Rectangle {
                        color: parent.hovered ? "#3e3e42" : "transparent"
                        radius: 3
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#cccccc"
                        font.pixelSize: 10
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: logPanelExpanded = !logPanelExpanded

                    Behavior on rotation {
                        NumberAnimation { duration: 200 }
                    }
                }
            }
        }

        // 日志内容区域
        ScrollView {
            anchors.top: logPanelHeader.bottom
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 5
            visible: logPanelExpanded

            TextArea {
                id: logTextArea
                text: logMessages.join('\n')
                color: "#cccccc"
                font.family: "Consolas, Monaco, monospace"
                font.pixelSize: 11
                readOnly: true
                selectByMouse: true
                background: Rectangle {
                    color: "#1e1e1e"
                }

                onTextChanged: {
                    cursorPosition = length
                }
            }
        }
    }

    // ==================== 属性定义（新增） ====================
    property string sideBarCurrentTab: "processes" // 当前选中的侧边栏标签
    property bool logPanelExpanded: false // 日志面板是否展开
    property var openTabs: [] // 打开的标签页列表
    property int currentTabIndex: -1 // 当前选中的标签页索引
    property var logMessages: [] // 日志消息列表

    // 侧边栏进程列表项委托
    Component {
        id: sidebarProcessDelegate

        Rectangle {
            width: ListView.view.width
            height: 32
            color: parent.ListView.isCurrentItem ? "#094771" : (processMouseArea.containsMouse ? "#37373d" : "transparent")
            radius: 3

            MouseArea {
                id: processMouseArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    sidebarProcessListView.currentIndex = index

                    // 打开详情标签页
                    openProcessTab(modelData)
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    color: getProcessStatusColor(modelData.status)
                }

                Text {
                    text: modelData.name || "未知进程"
                    color: "#cccccc"
                    font.pixelSize: 12
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Text {
                    text: modelData.pid || ""
                    color: "#999999"
                    font.pixelSize: 10
                    visible: text !== ""
                }
            }
        }
    }

    // 侧边栏IP列表项委托
    Component {
        id: sidebarIpDelegate

        Rectangle {
            width: ListView.view.width
            height: 28
            color: parent.ListView.isCurrentItem ? "#094771" : (ipMouseArea.containsMouse ? "#37373d" : "transparent")
            radius: 3

            MouseArea {
                id: ipMouseArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    sidebarIpListView.currentIndex = index
                    notifyIpSelection(modelData)
                    // openIpTab(modelData)
                }
                onDoubleClicked: {
                    // 双击编辑IP
                    editIpDialog.currentIp = modelData
                    editIpDialog.currentIndex = index
                    editIpDialog.open()
                }

                RowLayout { // 新增的RowLayout，用于包含IP信息和删除按钮
                    anchors.fill: parent
                    anchors.margins: 6 // 调整边距以匹配整体风格
                    spacing: 6

                    Rectangle { // IP状态指示器
                        width: 6
                        height: 6
                        radius: 3
                        color: "#4CAF50" // 在线状态
                    }

                    Text { // IP地址文本
                        text: modelData
                        color: "#cccccc"
                        font.pixelSize: 11
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    Button {
                        visible: ipMouseArea.containsMouse // 仍然依赖ipMouseArea的hover状态
                        text: "×"
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter // 确保垂直居中
                        Layout.rightMargin: 10 // 向左移动10像素
                        background: Rectangle {
                            color: "transparent" // 背景设置为透明
                        }
                        contentItem: Text {
                            text: parent.text
                            color: parent.hovered ? "#f44336" : "#cccccc" // 悬停时改变文本颜色
                            font.pixelSize: 20 // 适当增大字体以便点击
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            removeIpAddress(index)
                        }
                    }
                }
            }
        }
    }

    // 顶部标签页委托
    Component {
        id: tabDelegate

        Rectangle {
            width: Math.max(120, tabText.implicitWidth + 40)
            height: 35
            color: index === currentTabIndex ? "#1e1e1e" : (tabMouseArea.containsMouse ? "#2d2d30" : "#252526")
            border.color: index === currentTabIndex ? "#007acc" : "#3e3e42"
            border.width: index === currentTabIndex ? 2 : 1

            MouseArea {
                id: tabMouseArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: currentTabIndex = index
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                Text {
                    id: tabText
                    text: modelData.title || "未命名"
                    color: index === currentTabIndex ? "#cccccc" : "#999999"
                    font.pixelSize: 12
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Button {
                    visible: tabMouseArea.containsMouse || openTabs.length > 1
                    text: "×"
                    Layout.preferredWidth: 16 // 调整宽度
                    Layout.preferredHeight: 16 // 调整高度
                    background: Rectangle {
                        color: parent.hovered ? "#f44336" : "transparent"
                        radius: 12 // 调整圆角，使其看起来更圆润
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#cccccc"
                        font.pixelSize: 14 // 调整字体大小
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        anchors.fill: parent // 确保文字内容填充整个按钮区域，使其居中
                    }
                    onClicked: {
                        closeTab(index)
                        mouse.accepted = true
                    }
                }
            }
        }
    }

    // 主内容视图委托
    Component {
        id: contentViewDelegate
        Rectangle {
            color: "#1e1e1e"

            // 根据tab类型显示不同内容
            Loader {
                id:loader
                anchors.fill: parent
                sourceComponent: {
                    if (modelData.type === "process") return processDetailComponent
                }

                property var tabData: modelData || {}
            }
        }
    }


    // 进程详情组件 - 嵌入子进程窗口
    Component {
        id: processDetailComponent
        Rectangle {
            id: processDetailRoot // 为根组件添加ID以便于访问其内部元素
            property alias embeddedContainer: embeddedWindowContainer // 暴露内部的 embeddedWindowContainer
            color: "#1e1e1e"
            border.color: "#3e3e42"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // 顶部工具栏
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    color: "#2d2d30"
                    border.color: "#3e3e42"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 10

                        Text {
                            text: tabData.data ? tabData.data.name : "未知进程"
                            color: "#cccccc"
                            font.pixelSize: 12
                            font.bold: true
                        }

                        Item { Layout.fillWidth: true } // 弹性空间

                        Button {
                            text: "启动"
                            Layout.preferredWidth: 50
                            Layout.preferredHeight: 24
                            enabled: systemStarted && tabData.data && tabData.data.status !== "运行中"
                            background: Rectangle {
                                color: parent.enabled ? (parent.hovered ? "#4CAF50" : "#388E3C") : "#555555"
                                radius: 3
                            }
                            contentItem: Text {
                                text: parent.text
                                color: parent.enabled ? "white" : "#999999"
                                font.pixelSize: 10
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: {
                                if (tabData.data) {
                                    var processName = tabData.data.name; // 保存进程名称
                                    startProcessById(processName)
                                    // 启动后尝试嵌入窗口，进行多次重试
                                    // 使用 Qt.callLater 确保容器完全初始化后再尝试嵌入
                                    Qt.callLater(function() {
                                            var container = loader.item ? loader.item.embeddedContainer : null
                                            // 验证容器是否已关联到窗口
                                            if (container && container.window) {
                                                console.log("[QML] 开始嵌入窗口:", processName);
                                                startEmbeddingTask(processName, container);
                                            } else {
                                                console.warn("[QML] 容器尚未关联到窗口，将在窗口准备好后重试:", processName);
                                                // 等待窗口准备就绪后再尝试
                                                waitForContainerAndEmbed(processName, container);
                                            }
                                    });
                                }
                            }
                        }

                        Button {
                            text: "停止"
                            Layout.preferredWidth: 50
                            Layout.preferredHeight: 24
                            enabled: systemStarted && tabData.data && tabData.data.status === "运行中"
                            background: Rectangle {
                                color: parent.enabled ? (parent.hovered ? "#f44336" : "#d32f2f") : "#555555"
                                radius: 3
                            }
                            contentItem: Text {
                                text: parent.text
                                color: parent.enabled ? "white" : "#999999"
                                font.pixelSize: 10
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: {
                                if (tabData.data) {
                                    stopProcessById(tabData.data.name)
                                }
                            }
                        }
                    }
                }

                // 嵌入窗口容器
Rectangle {
    id: embeddedWindowContainer
    Layout.fillWidth: true
    Layout.fillHeight: true
    color: "#000000"
    border.color: "#3e3e42"
    border.width: 1

    property var windowId: 0
    property bool isReady: false // 标记容器是否已准备好

    Component.onCompleted: {
        console.log("[QML] embeddedWindowContainer Component.onCompleted");
        // 不再需要延迟函数，因为 onWindowChanged 会处理
    }

                    // 当没有嵌入窗口时显示的占位内容
                    // ColumnLayout {
                    // anchors.centerIn: parent
                    // visible: !tabData.data || tabData.data.status !== "运行中"
                    // spacing: 20

                    // Rectangle {
                    // Layout.alignment: Qt.AlignHCenter
                    // width: 64
                    // height: 64
                    // radius: 32
                    // color: "#2d2d30"
                    // border.color: "#3e3e42"
                    // border.width: 2

                    // Text {
                    // anchors.centerIn: parent
                    // text: "⚙"
                    // color: "#666666"
                    // font.pixelSize: 32
                    // }
                    // }

                    // Text {
                    //         Layout.alignment: Qt.AlignHCenter
                    //         text: tabData.data && tabData.data.status === "运行中" ?
                    //                   "正在加载进程窗口..." :
                    //                   "进程未运行\n点击上方\"启动\"按钮启动进程"
                    //         color: "#666666"
                    //         font.pixelSize: 14
                    //         horizontalAlignment: Text.AlignHCenter
                    //     }

                    //     Text {
                    //         Layout.alignment: Qt.AlignHCenter
                    //         text: tabData.data ? "进程: " + tabData.data.name : ""
                    //         color: "#888888"
                    //         font.pixelSize: 12
                    //         visible: tabData.data
                    //     }
                    // }
                }
            }
        }
    }


    // ==================== 对话框 ====================

    // 新增IP对话框
    Window {
        id: newIpDialog
        modality: Qt.ApplicationModal
        flags: Qt.Dialog | Qt.WindowStaysOnTopHint
        title: "添加新IP地址"
        width: 350
        height: 200
        visible: false
        x: (mainControllerRoot.width - width) / 2
        y: (mainControllerRoot.height - height) / 2

        Rectangle { // 作为背景
            anchors.fill: parent
            color: "#2d2d30"
            border.color: "#3e3e42"
            border.width: 1
            radius: 8
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 15

            Text {
                text: "请输入IP地址:"
                color: "#cccccc"
                font.pixelSize: 12
            }

            TextField {
                id: newIpInputField
                Layout.fillWidth: true
                placeholderText: "例如: 192.168.1.100"
                color: "#cccccc"
                placeholderTextColor: "#999999"
                background: Rectangle {
                    color: "#1e1e1e"
                    border.color: "#3e3e42"
                    border.width: 1
                    radius: 5
                }
            }

            RowLayout {
                Layout.fillWidth: true

                Item { Layout.fillWidth: true }

                Button {
                    text: "取消"
                    onClicked: {
                        newIpInputField.clear()
                        newIpDialog.visible = false
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#555555" : "#3e3e42"
                        radius: 5
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#cccccc"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Button {
                    text: "添加"
                    enabled: newIpInputField.text.trim().length > 0
                    onClicked: {
                        var newIp = newIpInputField.text.trim()
                        if (newIp && ipList.indexOf(newIp) === -1) {
                            addIpAddress(newIp)
                            newIpInputField.clear()
                            newIpDialog.visible = false
                        }
                    }
                    background: Rectangle {
                        color: parent.enabled ? (parent.hovered ? "#007acc" : "#005a9e") : "#555555"
                        radius: 5
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? "white" : "#999999"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    // 批量添加IP对话框
    Dialog {
        id: batchAddIpDialog
        title: "批量添加IP地址"
        width: 400
        height: 300
        modal: true
        
        // 设置 parent 为 ApplicationWindow 的 Overlay 以实现居中
        parent: Overlay.overlay

        // 设置 x 和 y 位置以实现居中（相对于 Overlay）
        x: parent ? (parent.width - width) / 2 : 0
        y: parent ? (parent.height - height) / 2 : 0

        property string startIp: ""
        property string endIp: ""
        property string subnetWarning: ""

        background: Rectangle {
            color: "#2d2d30"
            border.color: "#3e3e42"
            border.width: 1
            radius: 8
        }

        onOpened: {
            startIpInputField.text = ""
            endIpInputField.text = ""
            subnetWarningText.text = ""
            subnetWarningText.visible = false
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 15

            Text {
                text: "批量添加IP地址范围"
                color: "#cccccc"
                font.pixelSize: 13
                font.bold: true
            }

            Text {
                text: "起始IP地址:"
                color: "#cccccc"
                font.pixelSize: 12
            }

            TextField {
                id: startIpInputField
                Layout.fillWidth: true
                placeholderText: "例如: 192.168.1.1"
                color: "#cccccc"
                placeholderTextColor: "#999999"
                background: Rectangle {
                    color: "#1e1e1e"
                    border.color: subnetWarningText.visible && !isValidIp(startIpInputField.text) ? "#ff4444" : "#3e3e42"
                    border.width: 1
                    radius: 5
                }
                onTextChanged: {
                    batchAddIpDialog.startIp = text.trim()
                    validateIpRange()
                }
            }

            Text {
                text: "结束IP地址:"
                color: "#cccccc"
                font.pixelSize: 12
            }

            TextField {
                id: endIpInputField
                Layout.fillWidth: true
                placeholderText: "例如: 192.168.1.100"
                color: "#cccccc"
                placeholderTextColor: "#999999"
                background: Rectangle {
                    color: "#1e1e1e"
                    border.color: subnetWarningText.visible && !isValidIp(endIpInputField.text) ? "#ff4444" : "#3e3e42"
                    border.width: 1
                    radius: 5
                }
                onTextChanged: {
                    batchAddIpDialog.endIp = text.trim()
                    validateIpRange()
                }
            }

            Text {
                id: subnetWarningText
                text: batchAddIpDialog.subnetWarning
                color: "#ffaa44"
                font.pixelSize: 11
                visible: batchAddIpDialog.subnetWarning.length > 0
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Text {
                id: ipCountText
                text: {
                    var startIp = startIpInputField.text.trim()
                    var endIp = endIpInputField.text.trim()
                    if (startIp && endIp && isValidIp(startIp) && isValidIp(endIp) && isSameSubnet(startIp, endIp)) {
                        var count = getIpRangeCount(startIp, endIp)
                        return "将添加 " + count + " 个IP地址"
                    }
                    return ""
                }
                color: "#88cc88"
                font.pixelSize: 11
                visible: text.length > 0
                Layout.fillWidth: true
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true

                Item { Layout.fillWidth: true }

                Button {
                    text: "取消"
                    onClicked: batchAddIpDialog.close()
                    background: Rectangle {
                        color: parent.hovered ? "#555555" : "#3e3e42"
                        radius: 5
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#cccccc"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Button {
                    text: "添加"
                    enabled: {
                        var startIp = startIpInputField.text.trim()
                        var endIp = endIpInputField.text.trim()
                        return startIp && endIp && 
                               isValidIp(startIp) && 
                               isValidIp(endIp) && 
                               isSameSubnet(startIp, endIp) &&
                               getIpRangeCount(startIp, endIp) > 0
                    }
                    onClicked: {
                        var startIp = startIpInputField.text.trim()
                        var endIp = endIpInputField.text.trim()
                        if (isValidIp(startIp) && isValidIp(endIp) && isSameSubnet(startIp, endIp)) {
                            batchAddIpAddresses(startIp, endIp)
                            batchAddIpDialog.close()
                        }
                    }
                    background: Rectangle {
                        color: parent.enabled ? (parent.hovered ? "#007acc" : "#005a9e") : "#555555"
                        radius: 5
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? "white" : "#999999"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    // 编辑IP对话框
    Dialog {
        id: editIpDialog
        title: "编辑IP地址"
        width: 350
        height: 200
        modal: true
        
        // 设置 parent 为 ApplicationWindow 的 Overlay 以实现居中
        parent: Overlay.overlay

        property string currentIp: ""
        property int currentIndex: -1

        // 设置 x 和 y 位置以实现居中（相对于 Overlay）
        x: parent ? (parent.width - width) / 2 : 0
        y: parent ? (parent.height - height) / 2 : 0

        background: Rectangle {
            color: "#2d2d30"
            border.color: "#3e3e42"
            border.width: 1
            radius: 8
        }

        onOpened: {
            editIpInputField.text = editIpDialog.currentIp
            editIpInputField.selectAll()
            editIpInputField.focus = true
            
            // 确保居中显示
            if (parent) {
                x = (parent.width - width) / 2
                y = (parent.height - height) / 2
            }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 15

            Text {
                text: "修改IP:"
                color: "#cccccc"
                font.pixelSize: 12
            }

            TextField {
                id: editIpInputField
                Layout.fillWidth: true
                color: "#cccccc"
                placeholderText: "例如: 192.168.1.100"
                placeholderTextColor: "#999999"
                background: Rectangle {
                    color: "#1e1e1e"
                    border.color: "#3e3e42"
                    border.width: 1
                    radius: 5
                }
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        if (editIpInputField.text.trim().length > 0 && editIpDialog.currentIndex >= 0) {
                            var newIp = editIpInputField.text.trim()
                            updateIpAddress(editIpDialog.currentIndex, newIp)
                            editIpDialog.close()
                        }
                    } else if (event.key === Qt.Key_Escape) {
                        editIpDialog.close()
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true

                Item { Layout.fillWidth: true }

                Button {
                    text: "取消"
                    onClicked: editIpDialog.close()
                    background: Rectangle {
                        color: parent.hovered ? "#555555" : "#3e3e42"
                        radius: 5
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#cccccc"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Button {
                    text: "保存"
                    enabled: editIpInputField.text.trim().length > 0
                    onClicked: {
                        var newIp = editIpInputField.text.trim()
                        var ipIndex = editIpDialog.currentIndex
                        if (newIp && ipIndex >= 0) {
                            updateIpAddress(ipIndex, newIp)
                            editIpDialog.close()
                        }
                    }
                    background: Rectangle {
                        color: parent.enabled ? (parent.hovered ? "#007acc" : "#005a9e") : "#555555"
                        radius: 5
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? "white" : "#999999"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    // ==================== JavaScript 函数 ====================

    /**
    * @brief 更新系统状态
    */
    function updateSystemStatus() {
        if (mainController && systemStarted) {
            // 更新进程状态
            updateProcessList()

            // 更新IP列表
            updateIpList()

            // 更新系统统计信息
            // 这里可以调用 mainController.GetSystemStatistics() 等方法
        }
    }

    // ==================== 状态获取函数 ====================

    /**
    * @brief 更新IP列表
    */
    function updateIpList() {
        if (!mainController) {
            return;
        }
        try {
            var initialIpList = mainController.GetIpListFromDataStore();
            ipList = initialIpList.slice();
        } catch (e) {
            console.log("更新IP列表失败: " + e.message);
        }
    }

    // 函数用于根据状态值映射为可读字符串
    function mapProcessStatus(statusInt) {
        switch (statusInt) {
        case 0: return "未运行";
        case 1: return "启动中";
        case 2: return "运行中";
        case 3: return "停止中";
        case 4: return "错误";
        case 5: return "崩溃";
        default: return "未知";
        }
    }

    /**
    * @brief 更新进程列表和状态
    */
    function updateProcessList() {
        if (!mainController) {
            return;
        }
        // 1. 保存当前选中项索引
        var oldIndex = sidebarProcessListView.currentIndex;

        // 2. 获取初始进程名称列表并设置为默认状态
        var configuredNames = mainController.GetConfiguredProcessNames();
        var initialProcesses = [];
        for (var i = 0; i < configuredNames.length; i++) {
            initialProcesses.push({
                                      name: configuredNames[i],
                                      status: "未运行" // 默认状态
                                  });
        }
        processStatusList = initialProcesses;

        // 3. 获取所有进程的实时状态并更新列表
        try {
            var statusJson = mainController.GetAllProcessInfo();
            parseProcessStatus(statusJson);
        } catch (e) {
            console.log("更新进程列表失败: " + e.message);
        }

        // 4. 恢复选中项
        if (oldIndex >= 0 && oldIndex < processStatusList.length) {
            sidebarProcessListView.currentIndex = oldIndex;
        } else if (processStatusList.length > 0) {
            sidebarProcessListView.currentIndex = 0;
        } else {
            sidebarProcessListView.currentIndex = -1;
        }
    }

    /**
    * @brief 解析进程状态 JSON
    */
    function parseProcessStatus(statusJson) {
        // 构建 name->index 映射表，只在需要时构建或更新
        var processIndexMap = {};
        for (var i = 0; i < processStatusList.length; i++) {
            processIndexMap[processStatusList[i].name] = i;
        }

        // 只更新已有对象的 status 字段
        for (var key in statusJson) {
            if (statusJson.hasOwnProperty(key)) {
                var processData = statusJson[key];
                var idx = processIndexMap[key];
                if (typeof idx === "number") {
                    processStatusList[idx].status = mapProcessStatus(processData.status);
                } else {
                    // 如果是新进程，极少见。这里可以根据需求选择是否添加
                    // 如果这里添加了新进程，那么 processIndexMap 也需要更新
                    processStatusList.push({
                                               name: key,
                                               status: mapProcessStatus(processData.status)
                                           });
                }
            }
        }
        // 触发 UI 更新
        processStatusList = processStatusList.slice();
    }

    /**
    * @brief 删除选中的IP地址
    */
    function removeSelectedIp() {
        if (ipListView.currentIndex >= 0) {
            var removedIp = ipList[ipListView.currentIndex]
            ipList.splice(ipListView.currentIndex, 1)
            ipList = ipList.slice() // 触发属性更新
            appendLog("删除IP地址: " + removedIp)

            // 更新配置到后端
            updateIpConfiguration()
        }
    }


    /**
    * @brief 更新IP配置到后端
    */
    function updateIpConfiguration() {
        if (mainController) {
            // 构建配置更新对象
            var configUpdate = {
                "ip_table": ipList
            }

            try {
                var result = mainController.HotUpdateConfiguration(configUpdate)
                if (result) {
                    appendLog("IP配置热更新成功")
                } else {
                    appendLog("IP配置热更新失败")
                }
            } catch (e) {
                appendLog("IP配置更新异常: " + e.message)
            }
        }
    }

    function updateWorkspaceConfiguration() {
        if (mainController) {
            // 构建配置更新对象
            var configUpdate = {
                "work_directory": currentWorkspacePath
            }

            try {
                var result = mainController.HotUpdateConfiguration(configUpdate)
                if (result) {
                    appendLog("工作目录配置热更新成功")
                } else {
                    appendLog("工作目录配置热更新失败")
                }
            } catch (e) {
                appendLog("工作目录配置更新异常: " + e.message)
            }
        }
    }

    /**
    * @brief 获取进程状态颜色
    */
    function getProcessStatusColor(status) {
        switch (status) {
        case "运行中": return "#4CAF50"
        case "已停止": return "#f44336"
        case "启动中": return "#FF9800"
        case "错误": return "#f44336"
        default: return "#9E9E9E"
        }
    }

    /**
    * @brief 添加日志信息（适配新的日志系统）
    */
    function appendLog(message) {
        var timestamp = new Date().toLocaleTimeString()
        var logEntry = "[" + timestamp + "] " + message

        // 添加到日志消息数组
        logMessages.unshift(logEntry)

        // 限制日志数量，避免内存过度使用
        if (logMessages.length > 500) {
            logMessages = logMessages.slice(0, 400)
        }

        // 触发属性更新
        logMessages = logMessages.slice()
    }

    // ==================== VSCode风格界面控制函数 ====================

    /**
    * @brief 打开进程详情标签页
    */
    function openProcessTab(processData) {
        if (!processData) return

        // 如果当前不在主内容视图（例如在设置页面），则先返回
        if (stackLayout.depth > 1) {
            stackLayout.pop(null); // 弹出所有页面，直到返回根页面
        }

        // 检查是否已经打开了该进程的标签页
        var existingIndex = -1
        for (var i = 0; i < openTabs.length; i++) {
            if (openTabs[i].type === "process" && openTabs[i].data && openTabs[i].data.name === processData.name) {
                existingIndex = i
                break
            }
        }

        if (existingIndex >= 0) {
            // 如果已存在，切换到该标签页
            currentTabIndex = existingIndex
        } else {
            // 创建新的进程标签页
            var newTab = {
                type: "process",
                title: processData.name || "未知进程",
                data: processData
            }
            openTabs.push(newTab)
            openTabs = openTabs.slice() // 触发属性更新
            currentTabIndex = openTabs.length - 1
        }

        appendLog("打开进程详情: " + (processData.name || "未知进程"))
    }

    /**
    * @brief 打开IP详情标签页
    */
    function openIpTab(ipAddress) {
        if (!ipAddress) return

        // 检查是否已经打开了该IP的标签页
        var existingIndex = -1
        for (var i = 0; i < openTabs.length; i++) {
            if (openTabs[i].type === "ip" && openTabs[i].data === ipAddress) {
                existingIndex = i
                break
            }
        }

        if (existingIndex >= 0) {
            // 如果已存在，切换到该标签页
            currentTabIndex = existingIndex
        } else {
            // 创建新的IP标签页
            var newTab = {
                type: "ip",
                title: ipAddress,
                data: ipAddress
            }
            openTabs.push(newTab)
            openTabs = openTabs.slice() // 触发属性更新
            currentTabIndex = openTabs.length - 1
        }

        appendLog("打开IP详情: " + ipAddress)
    }

    /**
    * @brief 关闭指定标签页
    */
    function closeTab(tabIndex) {
        if (tabIndex < 0 || tabIndex >= openTabs.length) return

        var closedTab = openTabs[tabIndex]
        openTabs.splice(tabIndex, 1)
        openTabs = openTabs.slice() // 触发属性更新

        // 调整当前标签页索引
        if (currentTabIndex >= tabIndex) {
            currentTabIndex = Math.max(0, currentTabIndex - 1)
        }

        // 如果没有标签页了，重置索引
        if (openTabs.length === 0) {
            currentTabIndex = -1
        }

        appendLog("关闭标签页: " + (closedTab.title || "未命名"))
    }

    /**
    * @brief 关闭所有标签页
    */
    function closeAllTabs() {
        openTabs = []
        currentTabIndex = -1
        appendLog("关闭所有标签页")
    }

    /**
    * @brief 启动指定进程
    */
    function startProcessById(processName) {
        if (mainController && processName) {
            var result = mainController.StartSubProcess(processName)
            if (result) {
                appendLog("启动进程成功: " + processName)
                updateProcessList()
                // 更新打开的标签页中的进程数据
                updateProcessTabsData()
            } else {
                appendLog("启动进程失败: " + processName)
            }
        }
    }

    /**
    * @brief 停止指定进程
    */
    function stopProcessById(processName) {
        if (mainController && processName) {
            var result = mainController.StopSubProcess(processName)
            if (result) {
                appendLog("停止进程成功: " + processName)
                updateProcessList()

                // 更新打开的标签页中的进程数据
                updateProcessTabsData()
            } else {
                appendLog("停止进程失败: " + processName)
            }
        }
    }

    /**
    * @brief 重启指定进程
    */
    function restartProcessById(processName) {
        if (mainController && processName) {
            var result = mainController.RestartSubProcess(processName)
            if (result) {
                appendLog("重启进程成功: " + processName)
                updateProcessList()

                // 更新打开的标签页中的进程数据
                updateProcessTabsData()
            } else {
                appendLog("重启进程失败: " + processName)
            }
        }
    }

    /**
    * @brief 更新进程标签页中的数据
    */
    function updateProcessTabsData() {
        for (var i = 0; i < openTabs.length; i++) {
            if (openTabs[i].type === "process" && openTabs[i].data) {
                // 在进程状态列表中查找对应的进程数据
                for (var j = 0; j < processStatusList.length; j++) {
                    if (processStatusList[j].name === openTabs[i].data.name) {
                        openTabs[i].data = processStatusList[j]
                        break
                    }
                }
            }
        }

        // 触发属性更新
        openTabs = openTabs.slice()
    }

    /**
    * @brief 添加IP地址（新版本）
    */
    function addIpAddress(newIp) {
        if (!newIp) return

        newIp = newIp.trim()
        if (newIp && ipList.indexOf(newIp) === -1) {
            ipList.push(newIp)
            ipList = ipList.slice() // 触发属性更新
            appendLog("添加IP地址: " + newIp)

            // 更新配置到后端
            updateIpConfiguration()
        }
    }

    /**
    * @brief 删除IP地址（新版本）
    */
    function removeIpAddress(index) {
        if (index >= 0 && index < ipList.length) {
            var removedIp = ipList[index]
            ipList.splice(index, 1)
            ipList = ipList.slice() // 触发属性更新
            appendLog("删除IP地址: " + removedIp)

            // 关闭该IP相关的标签页
            for (var i = openTabs.length - 1; i >= 0; i--) {
                if (openTabs[i].type === "ip" && openTabs[i].data === removedIp) {
                    closeTab(i)
                    break
                }
            }

            // 更新配置到后端
            updateIpConfiguration()
        }
    }

    /**
     * @brief 更新IP地址
     */
    function updateIpAddress(index, newIp) {
        if (index >= 0 && index < ipList.length && newIp) {
            var oldIp = ipList[index]
            ipList[index] = newIp
            ipList = ipList.slice() // 触发属性更新
            appendLog("IP地址已更新: " + oldIp + " -> " + newIp)

            // 更新相关标签页
            for (var i = 0; i < openTabs.length; i++) {
                if (openTabs[i].type === "ip" && openTabs[i].data === oldIp) {
                    openTabs[i].data = newIp
                    openTabs[i].title = newIp
                    break
                }
            }
            openTabs = openTabs.slice() // 触发属性更新

            // 更新配置到后端
            updateIpConfiguration()
        }
    }

    /**
     * @brief 验证IP地址格式
     * @param ip IP地址字符串
     * @return 是否为有效IP地址
     */
    function isValidIp(ip) {
        if (!ip || typeof ip !== "string") return false
        
        var ipRegex = /^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/;
        var match = ip.trim().match(ipRegex);
        
        if (!match) return false
        
        // 检查每个段是否在0-255范围内
        for (var i = 1; i <= 4; i++) {
            var segment = parseInt(match[i], 10);
            if (segment < 0 || segment > 255) {
                return false;
            }
        }
        
        return true;
    }

    /**
     * @brief 检查两个IP是否在同一网段（前三个段相同）
     * @param ip1 第一个IP地址
     * @param ip2 第二个IP地址
     * @return 是否在同一网段
     */
    function isSameSubnet(ip1, ip2) {
        if (!isValidIp(ip1) || !isValidIp(ip2)) return false
        
        var parts1 = ip1.trim().split('.');
        var parts2 = ip2.trim().split('.');
        
        // 检查前三个段是否相同
        return parts1[0] === parts2[0] && 
               parts1[1] === parts2[1] && 
               parts1[2] === parts2[2];
    }

    /**
     * @brief 计算IP范围内的IP数量
     * @param startIp 起始IP地址
     * @param endIp 结束IP地址
     * @return IP数量（包含起始和结束IP）
     */
    function getIpRangeCount(startIp, endIp) {
        if (!isValidIp(startIp) || !isValidIp(endIp)) return 0
        if (!isSameSubnet(startIp, endIp)) return 0
        
        var startParts = startIp.trim().split('.');
        var endParts = endIp.trim().split('.');
        
        var startLast = parseInt(startParts[3], 10);
        var endLast = parseInt(endParts[3], 10);
        
        // 确保起始IP小于结束IP
        if (startLast > endLast) {
            return 0;
        }
        
        return endLast - startLast + 1;
    }

    /**
     * @brief 生成IP范围内的所有IP地址
     * @param startIp 起始IP地址
     * @param endIp 结束IP地址
     * @return IP地址数组
     */
    function generateIpRange(startIp, endIp) {
        if (!isValidIp(startIp) || !isValidIp(endIp)) return []
        if (!isSameSubnet(startIp, endIp)) return []
        
        var startParts = startIp.trim().split('.');
        var endParts = endIp.trim().split('.');
        
        var baseIp = startParts[0] + '.' + startParts[1] + '.' + startParts[2] + '.';
        var startLast = parseInt(startParts[3], 10);
        var endLast = parseInt(endParts[3], 10);
        
        // 确保起始IP小于结束IP
        if (startLast > endLast) {
            return [];
        }
        
        var ipList = [];
        for (var i = startLast; i <= endLast; i++) {
            ipList.push(baseIp + i);
        }
        
        return ipList;
    }

    /**
     * @brief 验证IP范围并设置警告信息
     */
    function validateIpRange() {
        var startIp = startIpInputField ? startIpInputField.text.trim() : ""
        var endIp = endIpInputField ? endIpInputField.text.trim() : ""
        
        if (!startIp && !endIp) {
            batchAddIpDialog.subnetWarning = ""
            return
        }
        
        if (!startIp) {
            batchAddIpDialog.subnetWarning = "请输入起始IP地址"
            return
        }
        
        if (!endIp) {
            batchAddIpDialog.subnetWarning = "请输入结束IP地址"
            return
        }
        
        if (!isValidIp(startIp)) {
            batchAddIpDialog.subnetWarning = "起始IP地址格式不正确"
            return
        }
        
        if (!isValidIp(endIp)) {
            batchAddIpDialog.subnetWarning = "结束IP地址格式不正确"
            return
        }
        
        if (!isSameSubnet(startIp, endIp)) {
            batchAddIpDialog.subnetWarning = "警告：两个IP地址不在同一网段，请确保前三个段相同（如：192.168.1.x）"
            return
        }
        
        // 检查起始IP是否小于结束IP
        var startParts = startIp.split('.');
        var endParts = endIp.split('.');
        var startLast = parseInt(startParts[3], 10);
        var endLast = parseInt(endParts[3], 10);
        
        if (startLast > endLast) {
            batchAddIpDialog.subnetWarning = "起始IP地址应小于结束IP地址"
            return
        }
        
        // 验证通过
        batchAddIpDialog.subnetWarning = ""
    }

    /**
     * @brief 批量添加IP地址
     * @param startIp 起始IP地址
     * @param endIp 结束IP地址
     */
    function batchAddIpAddresses(startIp, endIp) {
        if (!isValidIp(startIp) || !isValidIp(endIp)) {
            appendLog("批量添加IP失败：IP地址格式不正确")
            return
        }
        
        if (!isSameSubnet(startIp, endIp)) {
            appendLog("批量添加IP失败：两个IP地址不在同一网段")
            return
        }
        
        var ipRange = generateIpRange(startIp, endIp);
        if (ipRange.length === 0) {
            appendLog("批量添加IP失败：无法生成IP范围")
            return
        }
        
        var addedCount = 0
        var skippedCount = 0
        
        for (var i = 0; i < ipRange.length; i++) {
            var ip = ipRange[i]
            // 检查IP是否已存在
            if (ipList.indexOf(ip) === -1) {
                ipList.push(ip)
                addedCount++
            } else {
                skippedCount++
            }
        }
        
        // 触发属性更新
        ipList = ipList.slice()
        
        // 更新配置到后端
        updateIpConfiguration()
        
        // 记录日志
        if (addedCount > 0) {
            appendLog("批量添加IP地址：成功添加 " + addedCount + " 个IP地址（" + startIp + " - " + endIp + "）")
        }
        if (skippedCount > 0) {
            appendLog("批量添加IP地址：跳过 " + skippedCount + " 个已存在的IP地址")
        }
    }

    // ==================== 窗口嵌入辅助函数 ====================

    /**
     * @brief 等待容器准备就绪后再嵌入
     */
    function waitForContainerAndEmbed(processName, containerItem, maxWaitRetries, waitDelayMs) {
        if (!maxWaitRetries) maxWaitRetries = 10;
        if (!waitDelayMs) waitDelayMs = 100;
        
        if (maxWaitRetries <= 0) {
            console.error("[QML] 等待容器准备超时，放弃嵌入:", processName);
            return;
        }
        
        if (containerItem && containerItem.window) {
            console.log("[QML] 容器已准备好，开始嵌入:", processName);
            startEmbeddingTask(processName, containerItem);
        } else {
            console.debug("[QML] 等待容器准备就绪...剩余等待次数:", maxWaitRetries - 1);
            var timer = Qt.createQmlObject('import QtQuick 2.0; Timer { interval: ' + waitDelayMs + '; repeat: false; onTriggered: {} }', mainControllerRoot, "WaitTimer");
            timer.onTriggered.connect(function() {
                waitForContainerAndEmbed(processName, containerItem, maxWaitRetries - 1, waitDelayMs);
                timer.destroy();
            });
            timer.start();
        }
    }

    function startEmbeddingTask(processName, containerItem) { // 移除 containerWindowId
        if (!mainController) {
            console.warn("[QML] mainController 未定义，无法启动嵌入任务。");
            return;
        }
        
        // 验证容器项
        if (!containerItem) {
            console.error("[QML] containerItem 为 null，无法启动嵌入任务:", processName);
            return;
        }
        
        // 验证容器是否已关联到窗口
        if (!containerItem.window) {
            console.error("[QML] containerItem 尚未关联到窗口，无法启动嵌入任务:", processName);
            return;
        }
        
        console.log("[QML] 验证容器项有效性 - 容器:", containerItem, "窗口:", containerItem.window, "窗口ID:", containerItem.window ? containerItem.winId : "无效");
        
        // 立即设置"嵌入中"状态，以确保 onDestruction 能够正确取消
        mainController.startEmbeddingProcess(processName);
        // 开始重试逻辑
        tryEmbedProcessWindow(processName, containerItem, 6, 500); // 移除 containerWindowId
    }

    // tryEmbedProcessWindow 是一个辅助函数，用于多次尝试嵌入子进程窗口。
    // 它包含所有重试和状态检查逻辑，并在所有最终状态（成功、失败、取消）下调用 finishEmbeddingProcess。
    function tryEmbedProcessWindow(processName, containerItem, maxRetries, delayMs) { 

        // if (mainController.isEmbedCancelled(processName)) {
        //     console.log("[QML] 嵌入操作已取消:", processName);
        //     mainController.finishEmbeddingProcess(processName);
        //     return;
        // }

        if (maxRetries <= 0) {
            console.warn("[QML] 嵌入窗口重试次数已用尽，放弃嵌入:", processName);
            mainController.finishEmbeddingProcess(processName);
            return;
        }

        // 验证容器项在每次重试时仍然有效
        if (!containerItem) {
            console.error("[QML] 容器项在重试时变为无效，停止嵌入:", processName);
            mainController.finishEmbeddingProcess(processName);
            return;
        }
        
        // 验证容器是否仍然关联到窗口
        if (!containerItem.window) {
            console.warn("[QML] 容器在重试时未关联到窗口，等待后再试:", processName, "剩余重试:", maxRetries);
            // 等待一小段时间后重试
            var waitTimer = Qt.createQmlObject('import QtQuick 2.0; Timer { interval: ' + delayMs + '; repeat: false; onTriggered: {} }', mainControllerRoot, "WaitRetryTimer");
            waitTimer.onTriggered.connect(function() {
                tryEmbedProcessWindow(processName, containerItem, maxRetries - 1, delayMs);
                waitTimer.destroy();
            });
            waitTimer.start();
            return;
        }

        console.debug("[QML] 尝试嵌入窗口:", processName, "到容器:", containerItem.objectName || "未命名", "剩余重试:", maxRetries);

        // 使用 Qt.callLater 确保在 Qt 事件循环中执行，并避免阻塞UI
        Qt.callLater(function() {
            // 再次验证容器有效性（在回调中）
            if (!containerItem || !containerItem.window) {
                console.warn("[QML] 容器在回调中变为无效，停止重试:", processName);
                mainController.finishEmbeddingProcess(processName);
                return;
            }

            var success = mainController.EmbedProcessWindow(processName, containerItem);
            if (success) {
                console.info("[QML] 成功嵌入窗口:", processName);
                mainController.finishEmbeddingProcess(processName);
            } else {
                // 创建一个短暂的定时器，在延迟后再次尝试
                var timer = Qt.createQmlObject('import QtQuick 2.0; Timer { interval: ' + delayMs + '; repeat: false; onTriggered: {} }', mainControllerRoot, "RetryTimer");
                timer.onTriggered.connect(function() {
                    tryEmbedProcessWindow(processName, containerItem, maxRetries - 1, delayMs);
                    timer.destroy(); // 销毁定时器以释放资源
                });
                timer.start();
            }
        });
    }

    /**
    * @brief 通知子进程选定的IP
    */
    function notifyIpSelection(selectedIp) {
        if (mainController) {
            try {
                var result = mainController.SelectIpAndNotify(selectedIp)
                if (result) {
                    appendLog("已发送IP选择通知: " + selectedIp)
                } else {
                    appendLog("发送IP选择通知失败: " + selectedIp)
                }
            } catch (e) {
                appendLog("发送IP选择通知异常: " + e.message)
            }
        }
    }

}
