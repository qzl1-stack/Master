import QtQuick 6.0
import QtQuick.Controls 6.0
import QtQuick.Layouts 6.0

/**
 * @brief PluginDetailView 插件详情展示页面
 * 
 * 用于展示插件的详细信息，包括名称、版本、作者、详细描述等
 * 支持插件的安装和卸载操作，并显示安装进度
 */
Rectangle {
    id: pluginDetailRoot
    color: "#1e1e1e"
    
    // 属性定义
    property string pluginId: ""
    property var pluginData: null
    property var mainController: null
    property bool isInstalling: false
    property int installProgress: 0
    property var loaderRef: null  // Loader 引用
    
    // 组件加载时获取插件详情
    Component.onCompleted: {
        if (mainController && mainController.pluginManager && pluginId) {
            loadPluginDetail();
        }
    }
    
    // 加载插件详情
    function loadPluginDetail() {
        console.log("[PluginDetailView] 加载插件详情:", pluginId);
        pluginData = mainController.pluginManager.getPluginDetail(pluginId);
        console.log("[PluginDetailView] 插件数据:", JSON.stringify(pluginData));
    }
    
    // 安装插件
    function installPluginAction() {
        if (!mainController || !mainController.pluginManager) {
            console.warn("[PluginDetailView] MainController或PluginManager未初始化");
            return;
        }
        
        console.log("[PluginDetailView] 开始安装插件:", pluginId);
        isInstalling = true;
        mainController.pluginManager.installPlugin(pluginId);
    }
    
    // 卸载插件
    function uninstallPluginAction() {
        if (!mainController || !mainController.pluginManager) {
            console.warn("[PluginDetailView] MainController或PluginManager未初始化");
            return;
        }
        
        console.log("[PluginDetailView] 开始卸载插件:", pluginId);
        mainController.pluginManager.uninstallPlugin(pluginId);
    }
    
    // 监听安装进度信号
    Connections {
        target: mainController ? mainController.pluginManager : null
        
        function onInstallProgress(plugin_id, progress) {
            if (plugin_id === pluginId) {
                installProgress = progress;
                console.log("[PluginDetailView] 安装进度:", progress + "%");
            }
        }
        
        function onInstallCompleted(plugin_id, success, error_message) {
            if (plugin_id === pluginId) {
                isInstalling = false;
                if (success) {
                    console.log("[PluginDetailView] 安装成功");
                    // 重新加载插件详情
                    loadPluginDetail();
                } else {
                    console.warn("[PluginDetailView] 安装失败:", error_message);
                    errorMessageText.text = error_message;
                    errorMessageRect.visible = true;
                }
            }
        }
        
        function onUninstallCompleted(plugin_id, success) {
            if (plugin_id === pluginId) {
                if (success) {
                    console.log("[PluginDetailView] 卸载成功");
                    // 重新加载插件详情
                    loadPluginDetail();
                } else {
                    console.warn("[PluginDetailView] 卸载失败");
                }
            }
        }
        
        function onPluginListUpdated() {
            // 插件列表更新时，重新加载详情
            loadPluginDetail();
        }
    }
    
    ScrollView {
        anchors.fill: parent
        anchors.margins: 20
        clip: true
        
        ColumnLayout {
            width: pluginDetailRoot.width - 40
            spacing: 20
            
            // 错误消息提示
            Rectangle {
                id: errorMessageRect
                Layout.fillWidth: true
                Layout.preferredHeight: 50
                color: "#5a2a2a"
                border.color: "#ff4444"
                border.width: 1
                radius: 4
                visible: false
                
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10
                    
                    Text {
                        id: errorMessageText
                        color: "#ff6666"
                        font.pixelSize: 13
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    
                    Rectangle {
                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 24
                        color: "transparent"
                        
                        Canvas {
                            anchors.fill: parent
                            onPaint: {
                                var ctx = getContext("2d");
                                ctx.reset();
                                ctx.strokeStyle = "#ff6666";
                                ctx.lineWidth = 2;
                                ctx.beginPath();
                                ctx.moveTo(6, 6);
                                ctx.lineTo(18, 18);
                                ctx.moveTo(18, 6);
                                ctx.lineTo(6, 18);
                                ctx.stroke();
                            }
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            onClicked: errorMessageRect.visible = false
                        }
                    }
                }
            }
            
            // 顶部：插件基本信息
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 120
                color: "#252526"
                border.color: "#3e3e42"
                border.width: 1
                radius: 4
                
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 20
                    
                    // 插件图标
                    Rectangle {
                        Layout.preferredWidth: 80
                        Layout.preferredHeight: 80
                        color: "#1e1e1e"
                        border.color: "#007acc"
                        border.width: 2
                        radius: 8
                        
                        Canvas {
                            anchors.fill: parent
                            anchors.margins: 15
                            onPaint: {
                                var ctx = getContext("2d");
                                ctx.reset();
                                ctx.strokeStyle = "#007acc";
                                ctx.fillStyle = "#007acc";
                                ctx.lineWidth = 3;
                                ctx.lineCap = "round";
                                
                                // 绘制拼图图标
                                ctx.beginPath();
                                ctx.moveTo(10, 20);
                                ctx.lineTo(10, 40);
                                ctx.arc(15, 40, 5, Math.PI, 0, false);
                                ctx.lineTo(30, 40);
                                ctx.lineTo(30, 25);
                                ctx.arc(30, 20, 5, 0, Math.PI, true);
                                ctx.lineTo(15, 20);
                                ctx.arc(10, 20, 5, 0, Math.PI * 2, false);
                                ctx.stroke();
                            }
                        }
                    }
                    
                    // 插件信息
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 8
                        
                        Text {
                            text: pluginData ? pluginData.name : "未知插件"
                            color: "#ffffff"
                            font.pixelSize: 20
                            font.bold: true
                        }
                        
                        Text {
                            text: "版本: " + (pluginData ? pluginData.version : "")
                            color: "#cccccc"
                            font.pixelSize: 13
                        }
                        
                        Text {
                            text: "作者: " + (pluginData ? pluginData.author : "")
                            color: "#cccccc"
                            font.pixelSize: 13
                        }
                        
                        Text {
                            text: "分类: " + (pluginData ? pluginData.category : "")
                            color: "#007acc"
                            font.pixelSize: 12
                        }
                    }
                    
                    // 状态标签
                    Rectangle {
                        Layout.preferredWidth: 80
                        Layout.preferredHeight: 28
                        color: pluginData && pluginData.status === 3 ? "#2a5a2a" : "#5a5a2a"
                        border.color: pluginData && pluginData.status === 3 ? "#44ff44" : "#ffaa44"
                        border.width: 1
                        radius: 4
                        
                        Text {
                            anchors.centerIn: parent
                            text: pluginData ? pluginData.status_text : ""
                            color: pluginData && pluginData.status === 3 ? "#88ff88" : "#ffcc88"
                            font.pixelSize: 12
                            font.bold: true
                        }
                    }
                }
            }
            
            // 简短描述
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                color: "#252526"
                border.color: "#3e3e42"
                border.width: 1
                radius: 4
                
                Text {
                    anchors.fill: parent
                    anchors.margins: 15
                    text: pluginData ? pluginData.description : ""
                    color: "#cccccc"
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                    verticalAlignment: Text.AlignVCenter
                }
            }
            
            // 详细描述
            Rectangle {
                Layout.fillWidth: true
                Layout.minimumHeight: 200
                color: "#252526"
                border.color: "#3e3e42"
                border.width: 1
                radius: 4
                
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 10
                    
                    Text {
                        text: "详细介绍"
                        color: "#ffffff"
                        font.pixelSize: 16
                        font.bold: true
                    }
                    
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: "#3e3e42"
                    }
                    
                    Text {
                        Layout.fillWidth: true
                        text: pluginData ? pluginData.detailed_description : ""
                        color: "#cccccc"
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                        lineHeight: 1.4
                    }
                }
            }
            
            // 技术信息
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 150
                color: "#252526"
                border.color: "#3e3e42"
                border.width: 1
                radius: 4
                
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 10
                    
                    Text {
                        text: "技术信息"
                        color: "#ffffff"
                        font.pixelSize: 16
                        font.bold: true
                    }
                    
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: "#3e3e42"
                    }
                    
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 20
                        rowSpacing: 8
                        
                        Text {
                            text: "可执行文件:"
                            color: "#999999"
                            font.pixelSize: 12
                        }
                        
                        Text {
                            text: pluginData ? pluginData.executable : ""
                            color: "#cccccc"
                            font.pixelSize: 12
                        }
                        
                        Text {
                            text: "下载大小:"
                            color: "#999999"
                            font.pixelSize: 12
                        }
                        
                        Text {
                            text: pluginData ? (Math.round(pluginData.download_size / 1024 / 1024 * 100) / 100) + " MB" : ""
                            color: "#cccccc"
                            font.pixelSize: 12
                        }
                        
                        Text {
                            text: "所需版本:"
                            color: "#999999"
                            font.pixelSize: 12
                        }
                        
                        Text {
                            text: pluginData ? pluginData.required_version : ""
                            color: "#cccccc"
                            font.pixelSize: 12
                        }
                    }
                }
            }
            
            // 安装进度条
            Rectangle {
                id: progressContainer
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                color: "#252526"
                border.color: "#3e3e42"
                border.width: 1
                radius: 4
                visible: isInstalling
                
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 8
                    
                    Text {
                        text: "安装进度: " + installProgress + "%"
                        color: "#cccccc"
                        font.pixelSize: 13
                    }
                    
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 20
                        color: "#1e1e1e"
                        border.color: "#3e3e42"
                        border.width: 1
                        radius: 3
                        
                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: parent.width * (installProgress / 100)
                            color: "#007acc"
                            radius: 3
                            
                            Behavior on width {
                                NumberAnimation {
                                    duration: 200
                                    easing.type: Easing.InOutQuad
                                }
                            }
                        }
                    }
                }
            }
            
            // 操作按钮
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                color: "transparent"
                
                RowLayout {
                    anchors.centerIn: parent
                    spacing: 20
                    
                    // 安装/已安装按钮
                    Rectangle {
                        id: installButton
                        Layout.preferredWidth: 150
                        Layout.preferredHeight: 40
                        color: {
                            if (!pluginData || isInstalling) {
                                return "#3e3e42";
                            }
                            if (pluginData.status === 3) { // 已安装
                                return "#2a5a2a";
                            }
                            if (installButtonMouseArea.containsMouse) {
                                return "#0098ff";
                            }
                            return "#007acc";
                        }
                        border.color: pluginData && pluginData.status === 3 ? "#44ff44" : "#007acc"
                        border.width: 2
                        radius: 6
                        
                        Text {
                            anchors.centerIn: parent
                            text: {
                                if (!pluginData) return "加载中...";
                                if (isInstalling) return "安装中...";
                                if (pluginData.status === 3) return "已安装";
                                return "立即安装";
                            }
                            color: "#ffffff"
                            font.pixelSize: 14
                            font.bold: true
                        }
                        
                        MouseArea {
                            id: installButtonMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            enabled: pluginData && pluginData.status !== 3 && !isInstalling
                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            
                            onClicked: {
                                installPluginAction();
                            }
                        }
                        
                        Behavior on color {
                            ColorAnimation {
                                duration: 200
                            }
                        }
                    }
                    
                    // 卸载按钮（仅已安装时显示）
                    Rectangle {
                        id: uninstallButton
                        Layout.preferredWidth: 150
                        Layout.preferredHeight: 40
                        color: uninstallButtonMouseArea.containsMouse ? "#ff5555" : "#aa3333"
                        border.color: "#ff4444"
                        border.width: 2
                        radius: 6
                        visible: pluginData && pluginData.status === 3
                        
                        Text {
                            anchors.centerIn: parent
                            text: "卸载插件"
                            color: "#ffffff"
                            font.pixelSize: 14
                            font.bold: true
                        }
                        
                        MouseArea {
                            id: uninstallButtonMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            
                            onClicked: {
                                uninstallPluginAction();
                            }
                        }
                        
                        Behavior on color {
                            ColorAnimation {
                                duration: 200
                            }
                        }
                    }
                }
            }
            
            // 底部填充空间
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 20
            }
        }
    }
}

