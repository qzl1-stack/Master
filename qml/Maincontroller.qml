import QtQuick 6.0
import QtQuick.Controls 6.0
import QtQuick.Layouts 6.0
import QtQuick.Window 6.0
import Qt5Compat.GraphicalEffects

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
    width: parent.width
    height: parent.height

    // ==================== 属性定义 ====================
    property var mainController: null // MainController C++ 实例引用
    property bool systemInitialized: false
    property bool systemStarted: false
    property string currentSystemStatus: "空闲"
    property var processStatusList: []
    property var ipList: []
    property var pluginList: [] // 工具列表
    property string currentWorkspacePath: ""
    property string selectedProcess: "" // 当前选中的进程
    property bool secondarySidebarCollapsed: false // 次级侧边栏是否收起
    property int secondarySidebarWidth: 180 // 次级侧边栏展开宽度
    property string sideBarCurrentTab: "ip_list" // 侧边栏当前标签页: "ip_list" 或 "plugins"

    onCurrentWorkspacePathChanged: {
        if (currentWorkspacePath) {
            console.log("[Maincontroller] 工作目录已更改: " + currentWorkspacePath + "，将异步更新配置。");
            Qt.callLater(updateWorkspaceConfiguration);
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

        // ==================== 图标栏 ====================
        Rectangle {
            id: iconBar
            Layout.preferredWidth: 56
            Layout.fillHeight: true
            color: "#252526"
            // border.color: "#3e3e42"
            // border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: 8
                spacing: 8
            
             Rectangle {
                    Layout.preferredWidth: 56
                    Layout.preferredHeight: 56
                    color: {
                        if (selectedProcess === "插件商店") {
                            return "#37373d"
                        } else if (pluginStoreMouseArea.containsMouse) {
                            return "#2a2d2e"
                        } else {
                            return "transparent"
                        }
                    }

                    // 左侧选中指示器
                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 2
                        color: "#007acc"
                        visible: selectedProcess === "插件商店"
                    }

                    // Canvas图标容器
                    Item {
                        anchors.centerIn: parent
                        width: 32
                        height: 32

                        Canvas {
                            id: pluginStoreIcon
                            anchors.fill: parent
                            
                            property bool isHovered: pluginStoreMouseArea.containsMouse
                            property bool isSelected: selectedProcess === "插件商店"
                            
                            onIsHoveredChanged: requestPaint()
                            onIsSelectedChanged: requestPaint()
                            
                            onPaint: {
                                var ctx = getContext("2d");
                                ctx.reset();
                                ctx.clearRect(0, 0, width, height);
                                
                                // 设置基础颜色
                                var baseColor = isHovered || isSelected ? "#007acc" : "#ffffff";
                                var accentColor = isHovered || isSelected ? "#4fc1ff" : "#e8e8e8";
                                
                                ctx.lineWidth = 1.5;
                                ctx.lineCap = "round";
                                ctx.lineJoin = "round";
                                
                                // 绘制VSCode风格插件商店图标
                                drawVSCodePluginIcon(ctx, baseColor, accentColor);
                            }
                            
                            // 绘制VSCode风格的插件商店图标：网格+星号
                            function drawVSCodePluginIcon(ctx, baseColor, accentColor) {
                                // 绘制3个彩色方块（代表不同的插件）- 左上、右上、下面
                                var blockSize = 5;
                                var spacing = 2;
                                
                                // 左上方块 - 蓝色系
                                ctx.fillStyle = baseColor;
                                ctx.fillRect(6, 6, blockSize, blockSize);
                                
                                // 右上方块 - 绿色系
                                ctx.fillStyle = accentColor;
                                ctx.globalAlpha = 0.8;
                                ctx.fillRect(6 + blockSize + spacing, 6, blockSize, blockSize);
                                ctx.globalAlpha = 1;
                                
                                // 左下方块 - 紫色系
                                ctx.fillStyle = accentColor;
                                ctx.globalAlpha = 0.6;
                                ctx.fillRect(6, 6 + blockSize + spacing, blockSize, blockSize);
                                ctx.globalAlpha = 1;
                                
                                // 右下方块（主要块）- 更大更突出
                                ctx.fillStyle = baseColor;
                                ctx.globalAlpha = 0.9;
                                ctx.fillRect(6 + blockSize + spacing, 6 + blockSize + spacing, blockSize + 1, blockSize + 1);
                                ctx.globalAlpha = 1;
                                
                                // 添加星号标记在右下方块的右上角（表示推荐）
                                ctx.strokeStyle = baseColor;
                                ctx.fillStyle = baseColor;
                                var starX = 6 + blockSize + spacing + blockSize + 1 + 2;
                                var starY = 6 - 2;
                                drawStar(ctx, starX, starY, 2);
                            }
                            
                            // 绘制小星号
                            function drawStar(ctx, cx, cy, r) {
                                var points = 5;
                                var outerRadius = r;
                                var innerRadius = r * 0.4;
                                
                                ctx.beginPath();
                                for (var i = 0; i < points * 2; i++) {
                                    var radius = i % 2 === 0 ? outerRadius : innerRadius;
                                    var angle = (i * Math.PI) / points - Math.PI / 2;
                                    var x = cx + Math.cos(angle) * radius;
                                    var y = cy + Math.sin(angle) * radius;
                                    if (i === 0) ctx.moveTo(x, y);
                                    else ctx.lineTo(x, y);
                                }
                                ctx.closePath();
                                ctx.fill();
                            }
                        }
                    }

                    MouseArea {
                        id: pluginStoreMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        
                        onClicked: {
                            console.log("[QML] 点击插件商店");
                            selectedProcess = "插件商店";
                            secondarySidebarCollapsed = false;
                            sideBarCurrentTab = "plugins";
                            // 加载工具列表
                            loadPluginsFromUrl();
                        }
                        
                    }

                    Behavior on color {
                        ColorAnimation {
                            duration: 200
                        }
                    }
                }
                // 进程图标列表
                Repeater {
                    model: processStatusList
                    
                    Rectangle {
                        Layout.preferredWidth: 56
                        Layout.preferredHeight: 56
                        color: {
                            if (selectedProcess === modelData.name) {
                                return "#37373d"
                            } else if (iconMouseArea.containsMouse) {
                                return "#2a2d2e"
                            } else {
                                return "transparent"
                            }
                        }

                        // 左侧选中指示器
                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 2
                            color: "#007acc"
                            visible: selectedProcess === modelData.name
                        }

                        // 状态指示器（小圆点）
                        Rectangle {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: 4
                            width: 8
                            height: 8
                            radius: 4
                            color: getProcessStatusColor(modelData.status)
                            border.color: "#252526"
                            border.width: 1
                            z: 10
                        }

                        // Canvas图标容器
                        Item {
                            anchors.centerIn: parent
                            width: 32
                            height: 32

                            Canvas {
                                id: processIcon
                                anchors.fill: parent
                                
                                property bool isHovered: iconMouseArea.containsMouse
                                property bool isSelected: selectedProcess === modelData.name
                                
                                onIsHoveredChanged: requestPaint()
                                onIsSelectedChanged: requestPaint()
                                
                                onPaint: {
                                    var ctx = getContext("2d");
                                    ctx.reset();
                                    ctx.clearRect(0, 0, width, height);
                                    
                                    // 设置颜色
                                    var iconColor = isHovered || isSelected ? "#007acc" : "#ffffff";
                                    ctx.strokeStyle = iconColor;
                                    ctx.fillStyle = iconColor;
                                    ctx.lineWidth = 2;
                                    ctx.lineCap = "round";
                                    ctx.lineJoin = "round";
                                    
                                    // 根据进程名称绘制不同的图标
                                    if (modelData.name === "文件传输" || modelData.name.includes("文件") || modelData.name.includes("传输")) {
                                        drawFileTransferIcon(ctx);
                                    } else if (modelData.name === "AGV分析" || modelData.name.includes("AGV") || modelData.name.includes("分析")) {
                                        drawAGVAnalysisIcon(ctx);
                                    } else {
                                        // 默认图标：简单的方块
                                        drawDefaultIcon(ctx);
                                    }
                                }
                                
                                // 文件传输图标：文件夹+箭头
                                function drawFileTransferIcon(ctx) {
                                    // 绘制文件夹
                                    ctx.beginPath();
                                    // 文件夹底部
                                    ctx.moveTo(6, 12);
                                    ctx.lineTo(6, 26);
                                    ctx.lineTo(26, 26);
                                    ctx.lineTo(26, 12);
                                    // 文件夹标签
                                    ctx.moveTo(6, 12);
                                    ctx.lineTo(6, 8);
                                    ctx.lineTo(14, 8);
                                    ctx.lineTo(16, 12);
                                    ctx.stroke();
                                    
                                    // 绘制上传箭头
                                    ctx.beginPath();
                                    // 箭头线
                                    ctx.moveTo(16, 22);
                                    ctx.lineTo(16, 14);
                                    // 箭头头部
                                    ctx.moveTo(13, 17);
                                    ctx.lineTo(16, 14);
                                    ctx.lineTo(19, 17);
                                    ctx.stroke();
                                }
                                
                                // AGV分析图标：柱状图
                                function drawAGVAnalysisIcon(ctx) {
                                    // 绘制坐标轴
                                    ctx.beginPath();
                                    ctx.moveTo(6, 26);
                                    ctx.lineTo(6, 6);
                                    ctx.moveTo(6, 26);
                                    ctx.lineTo(26, 26);
                                    ctx.stroke();
                                    
                                    // 绘制柱状图
                                    ctx.fillRect(10, 18, 4, 8);
                                    ctx.fillRect(16, 14, 4, 12);
                                    ctx.fillRect(22, 10, 4, 16);
                                }
                                
                                // 默认图标：方块
                                function drawDefaultIcon(ctx) {
                                    ctx.strokeRect(8, 8, 16, 16);
                                }
                            }
                        }

                        MouseArea {
                            id: iconMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            
                            onClicked: {
                                console.log("[QML] 图标栏点击进程:", modelData.name);
                                selectedProcess = modelData.name;
                                secondarySidebarCollapsed = false;
                                sideBarCurrentTab = "ip_list";

                                // 调用进程启动逻辑
                                openProcessTab(modelData);
                                
                                // 检查进程状态，只有在未运行时才启动
                                if (modelData.status !== "运行中" && modelData.status !== "启动中") {
                                    console.log("[QML] 图标栏点击，立即启动进程:", modelData.name);
                                    startProcessById(modelData.name);
                                } else {
                                    console.log("[QML] 图标栏点击，进程已在运行或启动中，跳过启动:", modelData.name);
                                }
                            }
                            
                        }

                        Behavior on color {
                            ColorAnimation {
                                duration: 200
                            }
                        }
                    }
                }

                // 底部填充空间
                Item {
                    Layout.fillHeight: true
                }
            }
        }

        // ==================== 次级侧边栏 ====================
        Rectangle {
            id: sideBar
            Layout.preferredWidth: secondarySidebarCollapsed ? 0 : secondarySidebarWidth
            Layout.fillHeight: true
            color: "#2c2c2c"
            border.color: secondarySidebarCollapsed ? "transparent" : "#3e3e42"
            border.width: 1
            clip: true

            // 添加收缩动画（仅在 secondarySidebarCollapsed 改变时动画，不在拖动时动画）
            property bool isDragging: false
            
            Behavior on Layout.preferredWidth {
                enabled: !sideBar.isDragging
                NumberAnimation {
                    duration: 200
                    easing.type: Easing.InOutQuad
                }
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // IP列表视图
                Rectangle {
                    color: "#2c2c2c"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: sideBarCurrentTab === "ip_list"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 35

                            Text {
                                text: "IP地址列表"
                                color: "#cccccc"
                                font.pixelSize: 12
                                font.bold: true
                                Layout.fillWidth: true
                                verticalAlignment: Text.AlignVCenter
                            }

                            RowLayout {
                                spacing: 8
                                Layout.preferredHeight: 35

                                // 添加单个IP按钮
                                Rectangle {
                                    id: addSingleIpBtn
                                    Layout.preferredWidth: 32
                                    Layout.preferredHeight: 32
                                    radius: 6
                                    color: addSingleIpMouseArea.containsMouse ? "#0078d4" : "#3e3e42"

                                    Canvas {
                                        anchors.fill: parent
                                        onPaint: {
                                            var ctx = getContext("2d");
                                            ctx.reset();

                                            // 绘制加号
                                            ctx.strokeStyle = "#ffffff";
                                            ctx.lineWidth = 2;
                                            ctx.lineCap = "round";

                                            // 水平线
                                            ctx.beginPath();
                                            ctx.moveTo(10, 16);
                                            ctx.lineTo(22, 16);
                                            ctx.stroke();

                                            // 竖直线
                                            ctx.beginPath();
                                            ctx.moveTo(16, 10);
                                            ctx.lineTo(16, 22);
                                            ctx.stroke();
                                        }
                                    }

                                    MouseArea {
                                        id: addSingleIpMouseArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: newIpDialog.visible = true
                                    }

                                    Behavior on color {
                                        ColorAnimation {
                                            duration: 200
                                        }
                                    }
                                }

                                // 批量添加IP按钮
                                Rectangle {
                                    id: batchAddIpBtn
                                    Layout.preferredWidth: 32
                                    Layout.preferredHeight: 32
                                    radius: 6
                                    color: batchAddIpMouseArea.containsMouse ? "#0078d4" : "#3e3e42"

                                    Canvas {
                                        anchors.fill: parent
                                        onPaint: {
                                            var ctx = getContext("2d");
                                            ctx.reset();

                                            // 绘制多个方块表示批量
                                            ctx.fillStyle = "#ffffff";
                                            ctx.globalAlpha = 0.8;

                                            // 第一个方块
                                            ctx.fillRect(8, 8, 7, 7);
                                            // 第二个方块
                                            ctx.fillRect(17, 8, 7, 7);
                                            // 第三个方块
                                            ctx.fillRect(8, 17, 7, 7);
                                            // 第四个方块
                                            ctx.fillRect(17, 17, 7, 7);
                                        }
                                    }

                                    MouseArea {
                                        id: batchAddIpMouseArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: batchAddIpDialog.visible = true
                                    }

                                    Behavior on color {
                                        ColorAnimation {
                                            duration: 200
                                        }
                                    }
                                }
                                }
                        }

                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            ListView {
                                id: sidebarIpListView
                                clip: true
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

                // 工具列表视图
                Rectangle {
                    color: "#2c2c2c"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: sideBarCurrentTab === "plugins"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 35

                            Text {
                                text: "工具列表"
                                color: "#cccccc"
                                font.pixelSize: 12
                                font.bold: true
                                Layout.fillWidth: true
                                verticalAlignment: Text.AlignVCenter
                            }

                            // 刷新按钮
                            Rectangle {
                                id: refreshPluginsBtn
                                Layout.preferredWidth: 32
                                Layout.preferredHeight: 32
                                radius: 6
                                color: refreshPluginsBtnMouseArea.containsMouse ? "#0078d4" : "#3e3e42"

                                Canvas {
                                    anchors.fill: parent
                                    onPaint: {
                                        var ctx = getContext("2d");
                                        ctx.reset();

                                        ctx.strokeStyle = "#ffffff";
                                        ctx.lineWidth = 2;
                                        ctx.lineCap = "round";
                                        ctx.lineJoin = "round";

                                        // 绘制刷新图标
                                        var cx = 16, cy = 16;
                                        var r = 5;
                                        
                                        // 绘制圆形
                                        ctx.beginPath();
                                        ctx.arc(cx, cy, r, 0.5, Math.PI * 2 - 0.5);
                                        ctx.stroke();

                                        // 绘制箭头
                                        ctx.beginPath();
                                        ctx.moveTo(cx + r - 1, cy - r);
                                        ctx.lineTo(cx + r + 2, cy - r);
                                        ctx.lineTo(cx + r, cy - r - 3);
                                        ctx.stroke();
                                    }
                                }

                                MouseArea {
                                    id: refreshPluginsBtnMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: loadPluginsFromUrl()
                                }

                                Behavior on color {
                                    ColorAnimation {
                                        duration: 200
                                    }
                                }
                            }
                        }

                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            ListView {
                                id: sidebarPluginListView
                                clip: true
                                model: pluginList
                                delegate: sidebarPluginDelegate
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
                hoverEnabled: true

                property int lastMouseX: 0
                property int startWidth: 0

                onPressed: {
                    lastMouseX = mouseX;
                    startWidth = secondarySidebarCollapsed ? 0 : sideBar.Layout.preferredWidth;
                    sideBar.isDragging = true;
                }

                onReleased: {
                    sideBar.isDragging = false;
                    
                    // 松手时检查是否需要收起或记忆宽度
                    var currentWidth = sideBar.Layout.preferredWidth;
                    if (currentWidth < 150) {
                        secondarySidebarCollapsed = true;
                    } else {
                        secondarySidebarCollapsed = false;
                        secondarySidebarWidth = currentWidth;
                    }
                }

                onMouseXChanged: {
                    if (pressed && sideBar.isDragging) {
                        var delta = mouseX - lastMouseX;
                        var newWidth = startWidth + delta;

                        // 限制宽度范围在0-500之间
                        if (newWidth < 0) {
                            newWidth = 0;
                        } else if (newWidth > 500) {
                            newWidth = 500;
                        }

                        // 直接更新 Layout.preferredWidth，不触发动画
                        sideBar.Layout.preferredWidth = newWidth;
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
                            // border.color: "#3e3e42"
                            // border.width: 1

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
                                    if (openTabs.length === 0)
                                        return -1;
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
        height: logPanelExpanded ? 200 : 24
        color: "#1e1e1e"
        border.color: "#3e3e42"
        border.width: 1

        Behavior on height {
            NumberAnimation {
                duration: 200
                easing.type: Easing.OutQuad
            }
        }

        // 日志面板标题栏 - 现代化设计
        Rectangle {
            id: logPanelHeader
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: logPanelExpanded ? 40 : 24
            color: "#2d2d30"

            Behavior on height {
                NumberAnimation {
                    duration: 200
                    easing.type: Easing.OutQuad
                }
            }

            // 顶部分割线
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: "#3e3e42"
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: logPanelExpanded ? 10 : 4
                spacing: logPanelExpanded ? 8 : 0

                // 第一行：标题和操作按钮
                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 8

                    // 日志图标和标题
                    RowLayout {
                        spacing: 6
                        Layout.fillHeight: true

                        Rectangle {
                            width: 4
                            height: 16
                            radius: 2
                            color: "#0e639c"
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Text {
                            text: "实时日志"
                            color: "#e0e0e0"
                            font.pixelSize: logPanelExpanded ? 13 : 11
                            font.bold: true
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Text {
                            text: "(" + logMessages.length + ")"
                            color: "#858585"
                            font.pixelSize: 10
                            Layout.alignment: Qt.AlignVCenter
                            visible: logPanelExpanded
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                    }

                    // 操作按钮组
                    RowLayout {
                        spacing: 4
                        Layout.preferredHeight: 24

                        // 折叠展开按钮
                        Button {
                            text: logPanelExpanded ? "▼" : "▲"
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            font.pixelSize: 12

                            background: Rectangle {
                                color: parent.hovered ? "#3e3e42" : "transparent"
                                radius: 4
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "#cccccc"
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: logPanelExpanded = !logPanelExpanded
                        }
                    }
                }

                // 第二行：搜索栏（仅在展开时显示）
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: logPanelExpanded ? 32 : 0
                    color: "#1e1e1e"
                    radius: 4
                    border.color: "#3e3e42"
                    border.width: 1
                    visible: logPanelExpanded
                    clip: true

                    Behavior on Layout.preferredHeight {
                        NumberAnimation {
                            duration: 200
                            easing.type: Easing.OutQuad
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 4
                        spacing: 4

                        Text {
                            text: "🔍"
                            font.pixelSize: 12
                            color: "#858585"
                        }

                        TextField {
                            id: logSearchInput
                            Layout.fillWidth: true
                            color: "#cccccc"
                            font.pixelSize: 11
                            font.family: "Consolas, Monaco, monospace"
                            selectByMouse: true

                            placeholderText: "搜索日志内容..."

                            background: Rectangle {
                                color: "transparent"
                                border.color: "#3e3e42"
                                border.width: 0
                            }
                        }

                        // 清除搜索按钮
                        Button {
                            text: "✕"
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                            visible: logSearchInput.text.length > 0

                            background: Rectangle {
                                color: parent.hovered ? "#3e3e42" : "transparent"
                                radius: 3
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "#858585"
                                font.pixelSize: 10
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: logSearchInput.text = ""
                        }
                    }
                }
            }
        }

        // 日志内容区域 - 现代化设计
        Rectangle {
            id: logContentWrapper
            anchors.top: logPanelHeader.bottom
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            color: "#1e1e1e"
            visible: logPanelExpanded

            Behavior on opacity {
                NumberAnimation {
                    duration: 200
                }
            }

            ScrollView {
                anchors.fill: parent
                anchors.margins: 5

                background: Rectangle {
                    color: "#1e1e1e"
                }

                TextArea {
                    id: logTextArea
                    text: {
                        if (logSearchInput.text.length === 0) {
                            return logMessages.join('\n');
                        } else {
                            // 简单的搜索过滤
                            var searchTerm = logSearchInput.text.toLowerCase();
                            return logMessages.filter(function (msg) {
                                return msg.toLowerCase().indexOf(searchTerm) >= 0;
                            }).join('\n');
                        }
                    }
                    color: "#cccccc"
                    font.family: "Consolas, Monaco, monospace"
                    font.pixelSize: 11
                    readOnly: true
                    selectByMouse: true

                    background: Rectangle {
                        color: "transparent"
                    }

                    // 平滑滚动到底部
                    onTextChanged: {
                        cursorPosition = length;
                    }

                    // 右键菜单：复制选中文本
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.RightButton
                        onClicked: {
                            if (parent.selectedText.length > 0) {
                                // 已选中文本，显示菜单
                                contextMenu.popup();
                            }
                        }
                    }
                }
            }

            // 右键菜单
            Menu {
                id: contextMenu

                MenuItem {
                    text: "复制"
                    onTriggered: {
                        // 复制选中文本到剪贴板
                    }
                }

                MenuItem {
                    text: "全选"
                    onTriggered: {
                        logTextArea.selectAll();
                    }
                }

                MenuSeparator {}

                MenuItem {
                    text: "清空日志"
                    onTriggered: {
                        logMessages = [];
                    }
                }
            }

            // 空日志提示
            Text {
                anchors.centerIn: parent
                text: "暂无日志"
                color: "#666666"
                font.pixelSize: 14
                visible: logMessages.length === 0
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    // ==================== 属性定义（新增） ====================
    property bool logPanelExpanded: false
    property var openTabs: []
    property int currentTabIndex: -1
    property var logMessages: []
    property string pendingEmbedProcess: ""

    // 侧边栏进程列表项委托
    Component {
        id: sidebarProcessDelegate

        Rectangle {
            width: ListView.view ? ListView.view.width : 0
            height: 32
            color: (ListView.view && ListView.isCurrentItem) ? "#094771" : (processMouseArea.containsMouse ? "#37373d" : "transparent")
            radius: 3

            MouseArea {
                id: processMouseArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    // sidebarProcessListView.currentIndex = index; // 已移除进程列表视图
                    var processName = modelData.name;
                    openProcessTab(modelData);

                    // 检查进程状态，只有在未运行时才启动
                    if (modelData.status !== "运行中" && modelData.status !== "启动中") {
                        console.log("[QML] 侧边栏点击，立即启动进程:", processName);
                        startProcessById(processName);
                    } else {
                        console.log("[QML] 侧边栏点击，进程已在运行或启动中，跳过启动:", processName);
                    }
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
                    font.pixelSize: 14
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }
        }
    }

    // 侧边栏IP列表项委托
    Component {
        id: sidebarIpDelegate

        Rectangle {
            width: ListView.view ? ListView.view.width : 0
            height: 32
            color: (ListView.view && ListView.isCurrentItem) ? "#094771" : (ipMouseArea.containsMouse ? "#37373d" : "transparent")
            radius: 3

            MouseArea {
                id: ipMouseArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    sidebarIpListView.currentIndex = index;
                    notifyIpSelection(modelData);
                }
                onDoubleClicked: {
                    // 双击编辑IP
                    editIpDialog.currentIp = modelData;
                    editIpDialog.currentIndex = index;
                    editIpDialog.visible = true;
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 6 // 调整边距以匹配整体风格
                    spacing: 6

                    // Rectangle { // IP状态指示器
                    //     width: 6
                    //     height: 6
                    //     radius: 3
                    //     color: "#4CAF50" // 在线状态
                    // }

                    Text { // IP地址文本
                        text: modelData
                        color: "#cccccc"
                        font.pixelSize: 14
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    Button {
                        visible: ipMouseArea.containsMouse
                        text: "×"
                        Layout.preferredWidth: 30
                        Layout.preferredHeight: 20
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        background: Rectangle {
                            anchors.fill: parent
                            color: "transparent"
                            radius: 3
                        }
                        contentItem: Text {
                            anchors.fill: parent
                            text: parent.text
                            color: parent.hovered ? "#f44336" : "#cccccc" // 悬停时改变文本颜色
                            font.pixelSize: 20
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            removeIpAddress(index);
                        }
                    }
                }
            }
        }
    }

    // 工具列表Delegate
    Component {
        id: sidebarPluginDelegate

        Rectangle {
            width: ListView.view ? ListView.view.width : 0
            height: 70
            color: (ListView.view && ListView.isCurrentItem) ? "#094771" : (pluginMouseArea.containsMouse ? "#37373d" : "transparent")
            radius: 3

            MouseArea {
                id: pluginMouseArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    sidebarPluginListView.currentIndex = index;
                    openPluginDetailTab(modelData);
                }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    // 插件图标
                    Rectangle {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        radius: 4
                        color: "#3e3e42"

                        Text {
                            anchors.centerIn: parent
                            text: modelData.name ? modelData.name.charAt(0).toUpperCase() : "P"
                            color: "#ffffff"
                            font.bold: true
                            font.pixelSize: 14
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Text {
                                text: modelData.name || "未知插件"
                                color: "#e0e0e0"
                                font.pixelSize: 12
                                font.bold: true
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            // 类别标签
                            Rectangle {
                                Layout.preferredHeight: 16
                                Layout.preferredWidth: categoryText.width + 6
                                radius: 3
                                color: "#3e4444"

                                Text {
                                    id: categoryText
                                    anchors.centerIn: parent
                                    text: modelData.category || "通用"
                                    color: "#70d8ff"
                                    font.pixelSize: 9
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Text {
                                text: (modelData.version || "v1.0.0")
                                color: "#888888"
                                font.pixelSize: 10
                            }

                            Text {
                                text: modelData.author || "Unknown"
                                color: "#888888"
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            // 文件大小
                            Text {
                                text: formatFileSize(modelData.download_size || 0)
                                color: "#666666"
                                font.pixelSize: 9
                            }
                        }
                    }
                }

                Text {
                    text: modelData.description || "暂无描述"
                    color: "#a0a0a0"
                    font.pixelSize: 10
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    maximumLineCount: 1
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
                onClicked: {
                    currentTabIndex = index;
                    pendingEmbedProcess = modelData.title;
                }
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
                    // visible: tabMouseArea.containsMouse || openTabs.length > 1
                    text: "×"
                    Layout.preferredWidth: 25
                    Layout.preferredHeight: 25
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    background: Rectangle {
                        anchors.fill: parent
                        color: "transparent"
                        radius: 3
                    }
                    contentItem: Text {
                        anchors.fill: parent
                        text: parent.text
                        color: parent.hovered ? "#f44336" : "#cccccc" // 悬停时改变文本颜色
                        font.pixelSize: 20
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        closeTab(index);
                        mouse.accepted = true;
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
                id: loader
                anchors.fill: parent
                sourceComponent: {
                    if (modelData.type === "process")
                        return processDetailComponent;
                    if (modelData.type === "plugin_detail")
                        return pluginDetailComponent;
                }

                property var tabData: modelData || {}
                onLoaded: {
                    if (loader.item) {
                        if (modelData.type === "process") {
                            loader.item.loaderRef = loader.item;
                            loader.item.startEmbeddingTaskRef = startEmbeddingTask;
                        } else if (modelData.type === "plugin_detail") {
                            loader.item.pluginId = tabData.data.id || "";
                            loader.item.pluginData = tabData.data || null;
                            loader.item.mainController = mainControllerRoot.mainController;
                            loader.item.loaderRef = loader.item;
                        }
                    }
                }
            }
        }
    }

    // 进程详情组件 - 嵌入子进程窗口
    Component {
        id: processDetailComponent
        Rectangle {
            id: processDetailRoot // 为根组件添加ID以便于访问其内部元素
            property var loaderRef: null    // 用于存储 Loader.item，即自身引用（当前仅兼容保留）
            property string processName: ""  // 当前详情页对应的进程名
            property var startEmbeddingTaskRef: null // 从外部传入的 startEmbeddingTask 函数引用
            color: "#1e1e1e"
            border.color: "#3e3e42"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // 嵌入窗口容器
                Rectangle {
                    id: embeddedWindowContainer
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#000000"
                    border.color: "#3e3e42"
                    border.width: 1
                    z: -1

                    property var windowId: 0
                    property bool hasTriggeredEmbed: false // 标记是否已触发嵌入，避免重复触发

                    Connections {
                        target: mainControllerRoot // 根 Rectangle 的 id
                        function onPendingEmbedProcessChanged() {
                            var processName = tabData.data ? tabData.data.name : "";
                            if (!processName)
                                return;
                            if (mainControllerRoot.pendingEmbedProcess === processName) {
                                console.log("[QML] pendingEmbedProcess 变更匹配当前进程，重新尝试自动嵌入:", processName);
                                embeddedWindowContainer.hasTriggeredEmbed = false; // 允许再次触发
                                embeddedWindowContainer.tryAutoEmbed(); // 复用原来的自动嵌入逻辑
                            }
                        }
                    }

                    // 防抖定时器，避免频繁更新窗口几何
                    Timer {
                        id: geometryUpdateTimer
                        interval: 100 // 100ms 防抖延迟
                        repeat: false
                        onTriggered: {
                            var processName = tabData.data ? tabData.data.name : "";
                            if (processName && mainController) {
                                mainController.UpdateEmbeddedWindowGeometry(processName, embeddedWindowContainer);
                            }
                        }
                    }

                    Component.onCompleted: {
                        console.log("[QML] embeddedWindowContainer Component.onCompleted，容器尺寸:", width, "x", height);
                        // 嵌入逻辑将在 onWidthChanged 和 onHeightChanged 中触发
                    }

                    // 监听宽度变化，当宽高都 > 0 时触发嵌入或更新
                    onWidthChanged: {
                        if (hasTriggeredEmbed) {
                            // 如果已经嵌入，则更新窗口几何
                            updateEmbeddedWindowGeometry();
                        } else {
                            // 否则尝试嵌入
                            tryAutoEmbed();
                        }
                    }

                    // 监听高度变化，当宽高都 > 0 时触发嵌入或更新
                    onHeightChanged: {
                        if (hasTriggeredEmbed) {
                            // 如果已经嵌入，则更新窗口几何
                            updateEmbeddedWindowGeometry();
                        } else {
                            // 否则尝试嵌入
                            tryAutoEmbed();
                        }
                    }

                    // 自动嵌入函数
                    function tryAutoEmbed() {
                        // 如果已经触发过嵌入，则不再触发
                        if (hasTriggeredEmbed) {
                            console.debug("[QML] 已经触发过嵌入，不再触发");
                            return;
                        }

                        // 如果容器尺寸仍为 0，不执行嵌入
                        if (width <= 0 || height <= 0) {
                            console.debug("[QML] 容器尺寸无效，等待布局完成，当前尺寸:", width, "x", height);
                            return;
                        }

                        // 获取进程名
                        var processName = tabData.data ? tabData.data.name : "";
                        if (!processName) {
                            console.warn("[QML] 无有效进程名，暂不嵌入");
                            return;
                        }

                        // 验证函数引用
                        if (!processDetailRoot.startEmbeddingTaskRef || typeof processDetailRoot.startEmbeddingTaskRef !== "function") {
                            console.error("[QML] startEmbeddingTaskRef 无效，无法嵌入");
                            return;
                        }

                        // 标记已触发，避免重复调用
                        hasTriggeredEmbed = true;

                        console.log("[QML] 容器尺寸有效，自动开始嵌入:", processName, "容器尺寸:", width, "x", height);
                        processDetailRoot.startEmbeddingTaskRef(processName, embeddedWindowContainer);
                    }

                    // 更新嵌入窗口几何的函数（使用防抖）
                    function updateEmbeddedWindowGeometry() {
                        // 只在有效尺寸时更新
                        if (width <= 0 || height <= 0) {
                            return;
                        }

                        // 获取进程名
                        var processName = tabData.data ? tabData.data.name : "";
                        if (!processName) {
                            return;
                        }

                        // 重启定时器，实现防抖效果
                        geometryUpdateTimer.restart();
                    }
                }
            }
        }
    }

    // 插件详情组件
    Component {
        id: pluginDetailComponent
        PluginDetailView {
            property var loaderRef: null
        }
    }

    // ==================== 对话框 ====================

    // 新增IP对话框
    Window {
        id: newIpDialog
        modality: Qt.ApplicationModal
        flags: Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        title: "添加新IP地址"
        width: 380
        height: 220
        visible: false
        x: (mainControllerRoot.width - width) / 2
        y: (mainControllerRoot.height - height) / 2
        color: "transparent"

        Rectangle {
            anchors.fill: parent
            color: "#ffffff"
            radius: 4
            border.color: "#d0d0d0"
            border.width: 1

            // 阴影效果
            layer.enabled: true
            layer.effect: DropShadow {
                transparentBorder: true
                horizontalOffset: 0
                verticalOffset: 2
                radius: 8
                samples: 17
                color: "#40000000"
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // 标题栏
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    color: "#f5f5f5"
                    radius: 4

                    // 只让顶部有圆角
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 4
                        color: parent.color
                    }

                    // 底部分隔线
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 1
                        color: "#e0e0e0"
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        text: "添加新IP地址"
                        color: "#333333"
                        font.pixelSize: 14
                        font.bold: true
                    }
                }

                // 内容区域
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: 20

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 12

                        Text {
                            text: "请输入IP地址:"
                            color: "#333333"
                            font.pixelSize: 13
                            font.bold: true
                        }

                        Text {
                            text: "IP地址:"
                            color: "#666666"
                            font.pixelSize: 12
                        }

                        TextField {
                            id: newIpInputField
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            placeholderText: "例如: 192.168.1.100"
                            color: "#333333"
                            placeholderTextColor: "#999999"
                            font.pixelSize: 13
                            leftPadding: 10
                            background: Rectangle {
                                color: "#ffffff"
                                border.color: newIpInputField.activeFocus ? "#0078d4" : "#c0c0c0"
                                border.width: 1
                                radius: 3
                            }
                            onAccepted: {
                                if (newIpAddButton.enabled) {
                                    newIpAddButton.clicked();
                                }
                            }
                        }

                        Item {
                            Layout.fillHeight: true
                        }

                        // 按钮区域
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignRight
                            spacing: 10

                            Button {
                                text: "取消"
                                implicitWidth: 70
                                implicitHeight: 30
                                onClicked: {
                                    newIpInputField.clear();
                                    newIpDialog.visible = false;
                                }
                                background: Rectangle {
                                    color: parent.hovered ? "#e8e8e8" : "#f0f0f0"
                                    border.color: "#c0c0c0"
                                    border.width: 1
                                    radius: 3
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: "#333333"
                                    font.pixelSize: 12
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            Button {
                                id: newIpAddButton
                                text: "确定"
                                implicitWidth: 70
                                implicitHeight: 30
                                enabled: newIpInputField.text.trim().length > 0 && isValidIp(newIpInputField.text.trim())
                                onClicked: {
                                    var newIp = newIpInputField.text.trim();
                                    if (newIp && ipList.indexOf(newIp) === -1) {
                                        addIpAddress(newIp);
                                        newIpInputField.clear();
                                        newIpDialog.visible = false;
                                    }
                                }
                                background: Rectangle {
                                    color: parent.enabled ? (parent.hovered ? "#e8e8e8" : "#f0f0f0") : "#f5f5f5"
                                    border.color: parent.enabled ? "#c0c0c0" : "#d0d0d0"
                                    border.width: 1
                                    radius: 3
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: parent.enabled ? "#333333" : "#999999"
                                    font.pixelSize: 12
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // 批量添加IP对话框
    Window {
        id: batchAddIpDialog
        title: "批量添加IP地址"
        width: 380
        height: 320
        modality: Qt.ApplicationModal
        flags: Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        visible: false
        color: "transparent"

        // 居中显示
        x: (mainControllerRoot.width - width) / 2
        y: (mainControllerRoot.height - height) / 2

        property string startIp: ""
        property string endIp: ""
        property string subnetWarning: ""

        onVisibleChanged: {
            if (visible) {
                startIpInputField.text = "";
                endIpInputField.text = "";
                subnetWarningText.text = "";
                subnetWarningText.visible = false;
            }
        }

        Rectangle {
            anchors.fill: parent
            color: "#ffffff"
            radius: 4
            border.color: "#d0d0d0"
            border.width: 1

            // 阴影效果
            layer.enabled: true
            layer.effect: DropShadow {
                transparentBorder: true
                horizontalOffset: 0
                verticalOffset: 2
                radius: 8
                samples: 17
                color: "#40000000"
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // 标题栏
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    color: "#f5f5f5"
                    radius: 4

                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 4
                        color: parent.color
                    }

                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 1
                        color: "#e0e0e0"
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        text: "批量添加IP地址"
                        color: "#333333"
                        font.pixelSize: 14
                        font.bold: true
                    }
                }

                // 内容区域
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: 20

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10

                        Text {
                            text: "请输入IP地址范围:"
                            color: "#333333"
                            font.pixelSize: 13
                            font.bold: true
                        }

                        Text {
                            text: "起始IP地址:"
                            color: "#666666"
                            font.pixelSize: 12
                        }

                        TextField {
                            id: startIpInputField
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            placeholderText: "例如: 192.168.1.1"
                            color: "#333333"
                            placeholderTextColor: "#999999"
                            font.pixelSize: 13
                            leftPadding: 10
                            background: Rectangle {
                                color: "#ffffff"
                                border.color: startIpInputField.activeFocus ? "#0078d4" : (subnetWarningText.visible && !isValidIp(startIpInputField.text) ? "#d83b01" : "#c0c0c0")
                                border.width: 1
                                radius: 3
                            }
                            onTextChanged: {
                                batchAddIpDialog.startIp = text.trim();
                                validateIpRange();
                            }
                            onAccepted: {
                                if (batchIpAddButton.enabled) {
                                    batchIpAddButton.clicked();
                                }
                            }
                        }

                        Text {
                            text: "结束IP地址:"
                            color: "#666666"
                            font.pixelSize: 12
                        }

                        TextField {
                            id: endIpInputField
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            placeholderText: "例如: 192.168.1.50"
                            color: "#333333"
                            placeholderTextColor: "#999999"
                            font.pixelSize: 13
                            leftPadding: 10
                            background: Rectangle {
                                color: "#ffffff"
                                border.color: endIpInputField.activeFocus ? "#0078d4" : (subnetWarningText.visible && !isValidIp(endIpInputField.text) ? "#d83b01" : "#c0c0c0")
                                border.width: 1
                                radius: 3
                            }
                            onTextChanged: {
                                batchAddIpDialog.endIp = text.trim();
                                validateIpRange();
                            }
                            onAccepted: {
                                if (batchIpAddButton.enabled) {
                                    batchIpAddButton.clicked();
                                }
                            }
                        }

                        Text {
                            id: subnetWarningText
                            text: batchAddIpDialog.subnetWarning
                            color: "#d83b01"
                            font.pixelSize: 11
                            visible: batchAddIpDialog.subnetWarning.length > 0
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        Text {
                            id: ipCountText
                            text: {
                                var startIp = startIpInputField.text.trim();
                                var endIp = endIpInputField.text.trim();
                                if (startIp && endIp && isValidIp(startIp) && isValidIp(endIp) && isSameSubnet(startIp, endIp)) {
                                    var count = getIpRangeCount(startIp, endIp);
                                    return "将添加 " + count + " 个IP地址";
                                }
                                return "";
                            }
                            color: "#107c10"
                            font.pixelSize: 11
                            visible: text.length > 0
                            Layout.fillWidth: true
                        }

                        Item {
                            Layout.fillHeight: true
                        }

                        // 按钮区域
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignRight
                            spacing: 10

                            Button {
                                text: "取消"
                                implicitWidth: 70
                                implicitHeight: 30
                                onClicked: {
                                    startIpInputField.text = "";
                                    endIpInputField.text = "";
                                    batchAddIpDialog.visible = false;
                                }
                                background: Rectangle {
                                    color: parent.hovered ? "#e8e8e8" : "#f0f0f0"
                                    border.color: "#c0c0c0"
                                    border.width: 1
                                    radius: 3
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: "#333333"
                                    font.pixelSize: 12
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            Button {
                                id: batchIpAddButton
                                text: "确定"
                                implicitWidth: 70
                                implicitHeight: 30
                                enabled: {
                                    var startIp = startIpInputField.text.trim();
                                    var endIp = endIpInputField.text.trim();
                                    return startIp && endIp && isValidIp(startIp) && isValidIp(endIp) && isSameSubnet(startIp, endIp) && getIpRangeCount(startIp, endIp) > 0;
                                }
                                onClicked: {
                                    var startIp = startIpInputField.text.trim();
                                    var endIp = endIpInputField.text.trim();
                                    if (isValidIp(startIp) && isValidIp(endIp) && isSameSubnet(startIp, endIp)) {
                                        batchAddIpAddresses(startIp, endIp);
                                        startIpInputField.text = "";
                                        endIpInputField.text = "";
                                        batchAddIpDialog.visible = false;
                                    }
                                }
                                background: Rectangle {
                                    color: parent.enabled ? (parent.hovered ? "#e8e8e8" : "#f0f0f0") : "#f5f5f5"
                                    border.color: parent.enabled ? "#c0c0c0" : "#d0d0d0"
                                    border.width: 1
                                    radius: 3
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: parent.enabled ? "#333333" : "#999999"
                                    font.pixelSize: 12
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // 编辑IP对话框
    Window {
        id: editIpDialog
        title: "编辑IP地址"
        width: 380
        height: 220
        modality: Qt.ApplicationModal
        flags: Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        visible: false
        color: "transparent"

        // 居中显示
        x: (mainControllerRoot.width - width) / 2
        y: (mainControllerRoot.height - height) / 2

        property string currentIp: ""
        property int currentIndex: -1

        onVisibleChanged: {
            if (visible) {
                editIpInputField.text = editIpDialog.currentIp;
                editIpInputField.selectAll();
                editIpInputField.focus = true;
            }
        }

        Rectangle {
            anchors.fill: parent
            color: "#ffffff"
            radius: 4
            border.color: "#d0d0d0"
            border.width: 1

            // 阴影效果
            layer.enabled: true
            layer.effect: DropShadow {
                transparentBorder: true
                horizontalOffset: 0
                verticalOffset: 2
                radius: 8
                samples: 17
                color: "#40000000"
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // 标题栏
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    color: "#f5f5f5"
                    radius: 4

                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 4
                        color: parent.color
                    }

                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 1
                        color: "#e0e0e0"
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        text: "编辑IP地址"
                        color: "#333333"
                        font.pixelSize: 14
                        font.bold: true
                    }
                }

                // 内容区域
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: 20

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 12

                        Text {
                            text: "修改IP地址:"
                            color: "#333333"
                            font.pixelSize: 13
                            font.bold: true
                        }

                        Text {
                            text: "IP地址:"
                            color: "#666666"
                            font.pixelSize: 12
                        }

                        TextField {
                            id: editIpInputField
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            color: "#333333"
                            placeholderText: "例如: 192.168.1.100"
                            placeholderTextColor: "#999999"
                            font.pixelSize: 13
                            leftPadding: 10
                            background: Rectangle {
                                color: "#ffffff"
                                border.color: editIpInputField.activeFocus ? "#0078d4" : "#c0c0c0"
                                border.width: 1
                                radius: 3
                            }
                            Keys.onPressed: function (event) {
                                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                    if (editIpInputField.text.trim().length > 0 && editIpDialog.currentIndex >= 0) {
                                        var newIp = editIpInputField.text.trim();
                                        updateIpAddress(editIpDialog.currentIndex, newIp);
                                        editIpDialog.visible = false;
                                    }
                                } else if (event.key === Qt.Key_Escape) {
                                    editIpDialog.visible = false;
                                }
                            }
                        }

                        Item {
                            Layout.fillHeight: true
                        }

                        // 按钮区域
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignRight
                            spacing: 10

                            Button {
                                text: "取消"
                                implicitWidth: 70
                                implicitHeight: 30
                                onClicked: editIpDialog.visible = false
                                background: Rectangle {
                                    color: parent.hovered ? "#e8e8e8" : "#f0f0f0"
                                    border.color: "#c0c0c0"
                                    border.width: 1
                                    radius: 3
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: "#333333"
                                    font.pixelSize: 12
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            Button {
                                text: "确定"
                                implicitWidth: 70
                                implicitHeight: 30
                                enabled: editIpInputField.text.trim().length > 0 && isValidIp(editIpInputField.text.trim())
                                onClicked: {
                                    var newIp = editIpInputField.text.trim();
                                    var ipIndex = editIpDialog.currentIndex;
                                    if (newIp && ipIndex >= 0) {
                                        updateIpAddress(ipIndex, newIp);
                                        editIpDialog.visible = false;
                                    }
                                }
                                background: Rectangle {
                                    color: parent.enabled ? (parent.hovered ? "#e8e8e8" : "#f0f0f0") : "#f5f5f5"
                                    border.color: parent.enabled ? "#c0c0c0" : "#d0d0d0"
                                    border.width: 1
                                    radius: 3
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: parent.enabled ? "#333333" : "#999999"
                                    font.pixelSize: 12
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
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
            // updateProcessList()
            var statusJson = mainController.GetAllProcessInfo();
            parseProcessStatus(statusJson);
            // 更新IP列表
            updateIpList();
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
        case 0:
            return "未运行";
        case 1:
            return "启动中";
        case 2:
            return "运行中";
        case 3:
            return "停止中";
        case 4:
            return "错误";
        case 5:
            return "崩溃";
        default:
            return "未知";
        }
    }

    /**
    * @brief 更新进程列表和状态
    */
    function updateProcessList() {
        // 1. 保存当前选中项索引
        // var oldIndex = sidebarProcessListView.currentIndex;

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

        // 标记是否有变化
        var hasChanges = false;

        // 只更新已有对象的 status 字段
        for (var key in statusJson) {
            if (statusJson.hasOwnProperty(key)) {
                var processData = statusJson[key];
                var idx = processIndexMap[key];
                if (typeof idx === "number") {
                    var newStatus = mapProcessStatus(processData.status);
                    // 只在状态真正改变时才更新
                    if (processStatusList[idx].status !== newStatus) {
                        processStatusList[idx].status = newStatus;
                        hasChanges = true;
                    }
                } else {
                    // 如果是新进程，极少见。这里可以根据需求选择是否添加
                    processStatusList.push({
                        name: key,
                        status: mapProcessStatus(processData.status)
                    });
                    hasChanges = true;
                }
            }
        }

        // 只在有变化时触发 UI 更新
        if (hasChanges) {
            processStatusList = processStatusList.slice();
        }
    }

    /**
    * @brief 删除选中的IP地址
    */
    function removeSelectedIp() {
        if (ipListView.currentIndex >= 0) {
            var removedIp = ipList[ipListView.currentIndex];
            ipList.splice(ipListView.currentIndex, 1);
            ipList = ipList.slice(); // 触发属性更新
            appendLog("删除IP地址: " + removedIp);

            // 更新配置到后端
            updateIpConfiguration();
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
            };

            try {
                var result = mainController.HotUpdateConfiguration(configUpdate);
                if (result) {
                    appendLog("IP配置热更新成功");
                } else {
                    appendLog("IP配置热更新失败");
                }
            } catch (e) {
                appendLog("IP配置更新异常: " + e.message);
            }
        }
    }

    function updateWorkspaceConfiguration() {
        if (mainController) {
            // 构建配置更新对象
            var configUpdate = {
                "work_directory": currentWorkspacePath
            };

            try {
                var result = mainController.HotUpdateConfiguration(configUpdate);
                if (result) {
                    appendLog("工作目录配置热更新成功");
                } else {
                    appendLog("工作目录配置热更新失败");
                }
            } catch (e) {
                appendLog("工作目录配置更新异常: " + e.message);
            }
        }
    }

    /**
    * @brief 获取进程状态颜色
    */
    function getProcessStatusColor(status) {
        switch (status) {
        case "运行中":
            return "#4CAF50";
        case "已停止":
            return "#f44336";
        case "启动中":
            return "#FF9800";
        case "错误":
            return "#f44336";
        default:
            return "#9E9E9E";
        }
    }

    /**
    * @brief 添加日志信息（适配新的日志系统）
    */
    function appendLog(message) {
        var timestamp = new Date().toLocaleTimeString();
        var logEntry = "[" + timestamp + "] " + message;

        // 添加到日志消息数组
        logMessages.unshift(logEntry);

        // 限制日志数量，避免内存过度使用
        if (logMessages.length > 500) {
            logMessages = logMessages.slice(0, 400);
        }

        // 触发属性更新
        logMessages = logMessages.slice();
    }

    // ==================== VSCode风格界面控制函数 ====================

    /**
    * @brief 打开进程详情标签页
    */
    function openProcessTab(processData) {
        if (!processData)
            return;

        // 如果当前不在主内容视图（例如在设置页面），则先返回
        if (stackLayout.depth > 1) {
            stackLayout.pop(null); // 弹出所有页面，直到返回根页面
        }

        // 检查是否已经打开了该进程的标签页
        var existingIndex = -1;
        for (var i = 0; i < openTabs.length; i++) {
            if (openTabs[i].type === "process" && openTabs[i].data && openTabs[i].data.name === processData.name) {
                existingIndex = i;
                break;
            }
        }

        if (existingIndex >= 0) {
            // 如果已存在，切换到该标签页
            currentTabIndex = existingIndex;
        } else {
            // 创建新的进程标签页
            var newTab = {
                type: "process",
                title: processData.name || "未知进程",
                data: processData
            };
            openTabs.push(newTab);
            openTabs = openTabs.slice(); // 触发属性更新
            currentTabIndex = openTabs.length - 1;
        }

        // 记录需要嵌入的进程，供容器初始化后使用
        pendingEmbedProcess = processData.name || "";
        console.log("[QML] openProcessTab 设置待嵌入进程:", pendingEmbedProcess);

        appendLog("打开进程详情: " + (processData.name || "未知进程"));
    }

    /**
    * @brief 打开插件详情标签页
    */
    function openPluginDetailTab(pluginData) {
        if (!pluginData)
            return;

        if (stackLayout.depth > 1) {
            stackLayout.pop(null);
        }

        var existingIndex = -1;
        for (var i = 0; i < openTabs.length; i++) {
            if (openTabs[i].type === "plugin_detail" && openTabs[i].data && openTabs[i].data.id === pluginData.id) {
                existingIndex = i;
                break;
            }
        }

        if (existingIndex >= 0) {
            currentTabIndex = existingIndex;
        } else {
            var newTab = {
                type: "plugin_detail",
                title: pluginData.name || "未知插件",
                data: pluginData
            };
            openTabs.push(newTab);
            openTabs = openTabs.slice();
            currentTabIndex = openTabs.length - 1;
        }

        appendLog("打开插件详情: " + (pluginData.name || "未知插件"));
    }

    /**
    * @brief 关闭指定标签页
    */
    function closeTab(tabIndex) {
        if (tabIndex < 0 || tabIndex >= openTabs.length)
            return;
        var closedTab = openTabs[tabIndex];

        // 如果关闭的是进程标签页：隐藏已嵌入的子窗口，但不影响其他UI/不停止进程
        if (closedTab && closedTab.type === "process" && closedTab.data && closedTab.data.name && mainController) {
            mainController.SetEmbeddedProcessWindowVisible(closedTab.data.name, false);
        }

        openTabs.splice(tabIndex, 1);
        openTabs = openTabs.slice(); // 触发属性更新

        // 调整当前标签页索引
        if (currentTabIndex >= tabIndex) {
            currentTabIndex = Math.max(0, currentTabIndex - 1);
        }

        // 如果没有标签页了，重置索引
        if (openTabs.length === 0) {
            currentTabIndex = -1;
        }

        appendLog("关闭标签页: " + (closedTab.title || "未命名"));
    }

    /**
    * @brief 启动指定进程
    */
    function startProcessById(processName) {
        if (mainController && processName) {
            var result = mainController.StartSubProcess(processName);
            if (result) {
                appendLog("启动进程成功: " + processName);
                // 更新打开的标签页中的进程数据
                updateProcessTabsData();
            } else {
                appendLog("启动进程失败: " + processName);
            }
        }
    }

    /**
    * @brief 停止指定进程
    */
    function stopProcessById(processName) {
        if (mainController && processName) {
            var result = mainController.StopSubProcess(processName);
            if (result) {
                appendLog("停止进程成功: " + processName);
                updateProcessList();

                // 更新打开的标签页中的进程数据
                updateProcessTabsData();
            } else {
                appendLog("停止进程失败: " + processName);
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
                        openTabs[i].data = processStatusList[j];
                        break;
                    }
                }
            }
        }

        // 触发属性更新
        openTabs = openTabs.slice();
    }

    /**
    * @brief 添加IP地址（新版本）
    */
    function addIpAddress(newIp) {
        if (!newIp)
            return;
        newIp = newIp.trim();
        if (newIp && ipList.indexOf(newIp) === -1) {
            ipList.push(newIp);
            ipList = ipList.slice(); // 触发属性更新
            appendLog("添加IP地址: " + newIp);

            // 更新配置到后端
            updateIpConfiguration();
        }
    }

    /**
    * @brief 删除IP地址（新版本）
    */
    function removeIpAddress(index) {
        if (index >= 0 && index < ipList.length) {
            var removedIp = ipList[index];
            ipList.splice(index, 1);
            ipList = ipList.slice(); // 触发属性更新
            appendLog("删除IP地址: " + removedIp);

            // 更新配置到后端
            updateIpConfiguration();
        }
    }

    /**
     * @brief 更新IP地址
     */
    function updateIpAddress(index, newIp) {
        if (index >= 0 && index < ipList.length && newIp) {
            var oldIp = ipList[index];
            if(oldIp === newIp)
                return;
            ipList[index] = newIp;
            ipList = ipList.slice(); // 触发属性更新
            appendLog("IP地址已更新: " + oldIp + " -> " + newIp);

            // 更新相关标签页
            for (var i = 0; i < openTabs.length; i++) {
                if (openTabs[i].type === "ip" && openTabs[i].data === oldIp) {
                    openTabs[i].data = newIp;
                    openTabs[i].title = newIp;
                    break;
                }
            }
            openTabs = openTabs.slice(); // 触发属性更新

            // 更新配置到后端
            updateIpConfiguration();
        }
    }

    /**
     * @brief 验证IP地址格式
     * @param ip IP地址字符串
     * @return 是否为有效IP地址
     */
    function isValidIp(ip) {
        if (!ip || typeof ip !== "string")
            return false;

        var ipRegex = /^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/;
        var match = ip.trim().match(ipRegex);

        if (!match)
            return false;

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
        if (!isValidIp(ip1) || !isValidIp(ip2))
            return false;

        var parts1 = ip1.trim().split('.');
        var parts2 = ip2.trim().split('.');

        // 检查前三个段是否相同
        return parts1[0] === parts2[0] && parts1[1] === parts2[1] && parts1[2] === parts2[2];
    }

    /**
     * @brief 计算IP范围内的IP数量
     * @param startIp 起始IP地址
     * @param endIp 结束IP地址
     * @return IP数量（包含起始和结束IP）
     */
    function getIpRangeCount(startIp, endIp) {
        if (!isValidIp(startIp) || !isValidIp(endIp))
            return 0;
        if (!isSameSubnet(startIp, endIp))
            return 0;

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
        if (!isValidIp(startIp) || !isValidIp(endIp))
            return [];
        if (!isSameSubnet(startIp, endIp))
            return [];

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
        var startIp = startIpInputField ? startIpInputField.text.trim() : "";
        var endIp = endIpInputField ? endIpInputField.text.trim() : "";

        if (!startIp && !endIp) {
            batchAddIpDialog.subnetWarning = "";
            return;
        }

        if (!startIp) {
            batchAddIpDialog.subnetWarning = "请输入起始IP地址";
            return;
        }

        if (!endIp) {
            batchAddIpDialog.subnetWarning = "请输入结束IP地址";
            return;
        }

        if (!isValidIp(startIp)) {
            batchAddIpDialog.subnetWarning = "起始IP地址格式不正确";
            return;
        }

        if (!isValidIp(endIp)) {
            batchAddIpDialog.subnetWarning = "结束IP地址格式不正确";
            return;
        }

        if (!isSameSubnet(startIp, endIp)) {
            batchAddIpDialog.subnetWarning = "警告：两个IP地址不在同一网段，请确保前三个段相同（如：192.168.1.x）";
            return;
        }

        // 检查起始IP是否小于结束IP
        var startParts = startIp.split('.');
        var endParts = endIp.split('.');
        var startLast = parseInt(startParts[3], 10);
        var endLast = parseInt(endParts[3], 10);

        if (startLast > endLast) {
            batchAddIpDialog.subnetWarning = "起始IP地址应小于结束IP地址";
            return;
        }

        // 验证通过
        batchAddIpDialog.subnetWarning = "";
    }

    /**
     * @brief 批量添加IP地址
     * @param startIp 起始IP地址
     * @param endIp 结束IP地址
     */
    function batchAddIpAddresses(startIp, endIp) {
        if (!isValidIp(startIp) || !isValidIp(endIp)) {
            appendLog("批量添加IP失败：IP地址格式不正确");
            return;
        }

        if (!isSameSubnet(startIp, endIp)) {
            appendLog("批量添加IP失败：两个IP地址不在同一网段");
            return;
        }

        var ipRange = generateIpRange(startIp, endIp);
        if (ipRange.length === 0) {
            appendLog("批量添加IP失败：无法生成IP范围");
            return;
        }

        var addedCount = 0;
        var skippedCount = 0;

        for (var i = 0; i < ipRange.length; i++) {
            var ip = ipRange[i];
            // 检查IP是否已存在
            if (ipList.indexOf(ip) === -1) {
                ipList.push(ip);
                addedCount++;
            } else {
                skippedCount++;
            }
        }

        // 触发属性更新
        ipList = ipList.slice();

        // 更新配置到后端
        updateIpConfiguration();

        // 记录日志
        if (addedCount > 0) {
            appendLog("批量添加IP地址：成功添加 " + addedCount + " 个IP地址（" + startIp + " - " + endIp + "）");
        }
        if (skippedCount > 0) {
            appendLog("批量添加IP地址：跳过 " + skippedCount + " 个已存在的IP地址");
        }
    }

    // ==================== 窗口嵌入辅助函数 ====================

    function startEmbeddingTask(processName, containerItem) {
        if (!mainController) {
            console.warn("[QML] mainController 未定义，无法启动嵌入任务。");
            return;
        }

        // 验证容器项
        if (!containerItem) {
            console.error("[QML] containerItem 为 null，无法启动嵌入任务:", processName);
            return;
        }

        console.log("[QML] 验证容器项有效性 - 容器:", containerItem, "宽高:", containerItem.width, "x", containerItem.height);

        // 立即设置"嵌入中"状态，以确保 onDestruction 能够正确取消
        mainController.startEmbeddingProcess(processName);

        tryEmbedProcessWindow(processName, containerItem);
    }

    function tryEmbedProcessWindow(processName, containerItem) {

        // 验证容器项在每次重试时仍然有效
        if (!containerItem) {
            console.error("[QML] 容器项在重试时变为无效，停止嵌入:", processName);
            mainController.finishEmbeddingProcess(processName);
            return;
        }

        console.debug("[QML] 尝试嵌入窗口:", processName, "到容器:", containerItem.objectName || "未命名");

        // 使用 Qt.callLater 确保在 Qt 事件循环中执行，并避免阻塞UI
        Qt.callLater(function () {
            if (!containerItem) {
                console.warn("[QML] 容器在回调中变为无效，停止重试:", processName);
                mainController.finishEmbeddingProcess(processName);
                return;
            }

            var success = mainController.EmbedProcessWindow(processName, containerItem);
            if (success) {
                console.info("[QML] 成功嵌入窗口:", processName);
                mainController.finishEmbeddingProcess(processName);
            }
        });
    }

    /**
    * @brief 通知子进程选定的IP
    */
    /**
    * @brief 格式化文件大小为易读的形式
    * @param bytes 字节数
    * @return 格式化的文件大小字符串
    */
    function formatFileSize(bytes) {
        if (bytes === 0) return "0 B";
        var k = 1024;
        var sizes = ["B", "KB", "MB", "GB"];
        var i = Math.floor(Math.log(bytes) / Math.log(k));
        return (bytes / Math.pow(k, i)).toFixed(2) + " " + sizes[i];
    }

    /**
    * @brief 从URL加载工具列表
    */
    function loadPluginsFromUrl() {
        var xhr = new XMLHttpRequest();
        xhr.timeout = 5000; // 5秒超时
        
        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                if (xhr.status === 200) {
                    try {
                        var data = JSON.parse(xhr.responseText);
                        console.log("[QML] 工具列表加载成功，共 " + data.plugins.length + " 个插件");
                        
                        // 将数据转换为QML可用的格式
                        var pluginArray = [];
                        for (var i = 0; i < data.plugins.length; i++) {
                            pluginArray.push({
                                id: data.plugins[i].id,
                                name: data.plugins[i].name,
                                version: data.plugins[i].version,
                                author: data.plugins[i].author,
                                description: data.plugins[i].description,
                                detailed_description: data.plugins[i].detailed_description,
                                category: data.plugins[i].category,
                                download_url: data.plugins[i].download_url,
                                download_size: data.plugins[i].download_size,
                                executable: data.plugins[i].executable,
                                required_version: data.plugins[i].required_version,
                                icon_type: data.plugins[i].icon_type || "default",
                                dependencies: data.plugins[i].dependencies || [],
                                screenshots: data.plugins[i].screenshots || []
                            });
                        }
                        pluginList = pluginArray;
                        appendLog("工具列表加载成功，共 " + pluginArray.length + " 个插件");
                    } catch (e) {
                        console.log("[QML] 插件JSON解析错误: " + e.message);
                        appendLog("工具列表解析失败: " + e.message);
                    }
                } else {
                    console.log("[QML] 加载工具列表失败，状态码: " + xhr.status);
                    appendLog("加载工具列表失败，状态码: " + xhr.status);
                }
            }
        };
        
        xhr.onerror = function() {
            console.log("[QML] 加载工具列表网络错误");
            appendLog("加载工具列表网络错误");
        };
        
        xhr.ontimeout = function() {
            console.log("[QML] 加载工具列表超时");
            appendLog("加载工具列表超时");
        };
        
        try {
            console.log("[QML] 开始加载工具列表...");
            appendLog("正在加载工具列表...");
            xhr.open("GET", "https://jts-tools-extensions.oss-cn-chengdu.aliyuncs.com/plugins.json", true);
            xhr.send();
        } catch (e) {
            console.log("[QML] 加载工具列表异常: " + e.message);
            appendLog("加载工具列表异常: " + e.message);
        }
    }

    function notifyIpSelection(selectedIp) {
        if (mainController) {
            try {
                var result = mainController.SelectIpAndNotify(selectedIp);
                if (result) {
                    appendLog("已发送IP选择通知: " + selectedIp);
                } else {
                    appendLog("发送IP选择通知失败: " + selectedIp);
                }
            } catch (e) {
                appendLog("发送IP选择通知异常: " + e.message);
            }
        }
    }
}
