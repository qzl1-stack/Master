import QtQuick 6.0
import QtQuick.Controls 6.0
import QtQuick.Layouts 6.0
import QtQuick.Controls.Basic 6.0
import Master 1.0 // 导入 Master 模块以使用 FolderDialogHelper


Rectangle {
    id: workspaceSelectorRoot
    color: "#1e1e1e"
    width: parent.width
    height: parent.height

    // 属性定义
    property var mainController: null
    property var recentWorkspaces: []

    // 信号定义
    signal workspaceSelected(string workspacePath)

    // 组件加载时初始化
    Component.onCompleted: {
        loadRecentWorkspaces()
    }

    // 主布局
    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(600, parent.width * 0.8)
        spacing: 30

        // 标题区域 - 专业现代化设计
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 20

            // 应用名称
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "JT Studio"
                color: "#ffffff"
                font.pixelSize: 32
                font.bold: true
                font.family: "Arial"
            }

            // 公司名称（中文）
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "嘉腾机器人集成工作台"
                color: "#cccccc"
                font.pixelSize: 16
                font.bold: true
                font.family: "Source Han Sans, Microsoft YaHei, SimHei"
            }


            // 分隔线
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 200
                height: 1
                color: "#3e3e42"
                Layout.topMargin: 10
            }
        }

        // 最近使用的工作区
        GroupBox {
            Layout.fillWidth: true
            Layout.preferredHeight: 250

            background: Rectangle {
                color: "#2d2d30"
                border.color: "transparent"
                border.width: 1
                radius: 8
            }

            label: Text {
                text: "最近使用的工作区"
                color: "#cccccc"
                font.pixelSize: 13
                font.bold: true
                font.family: "Source Han Sans, Microsoft YaHei, SimHei"
                leftPadding: 10
                topPadding: 10
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 0
                spacing: 0

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ListView {
                        id: recentWorkspacesListView
                        model: recentWorkspaces
                        spacing: 2

                        delegate: Rectangle {
                            id: workspaceItemDelegate
                            width: ListView.view.width
                            height: 45
                            color: workspaceMouseArea.containsMouse ? "#37373d" : "transparent"
                            radius: 5

                            MouseArea {
                                id: workspaceMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor

                                onClicked: {
                                    selectWorkspace(modelData.path)
                                }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 12

                                // 目录信息
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Text {
                                        text: modelData.name || getDirectoryName(modelData.path)
                                        color: "#cccccc"
                                        font.pixelSize: 14
                                        font.bold: true
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        text: modelData.path
                                        color: "#999999"
                                        font.pixelSize: 11
                                        Layout.fillWidth: true
                                        elide: Text.ElideMiddle
                                    }
                                }

                                // 最后使用时间
                                Text {
                                    text: formatLastUsed(modelData.lastUsed)
                                    color: "#666666"
                                    font.pixelSize: 10
                                    Layout.alignment: Qt.AlignTop | Qt.AlignRight
                                }

                                // 删除图标 (始终可见但灰显)
                                Rectangle {
                                    width: 28
                                    height: 28
                                    color: "transparent"
                                    radius: 4
                                    Layout.rightMargin: 4
                                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter

                                    Text {
                                        anchors.centerIn: parent
                                        text: "×"
                                        color: "#cccccc"
                                        font.pixelSize: 18
                                        font.bold: true
                                        visible: workspaceMouseArea.containsMouse
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        
                                        onClicked: {
                                            removeWorkspaceFromHistory(index)
                                        }
                                    }

                                    Behavior on color {
                                        ColorAnimation {
                                            duration: 150
                                        }
                                    }
                                }
                            }
                        }

                        // 空状态显示
                        Rectangle {
                            anchors.centerIn: parent
                            width: 200
                            height: 100
                            color: "transparent"
                            visible: recentWorkspaces.length === 0

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 10

                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: "📂"
                                    font.pixelSize: 32
                                    color: "#666666"
                                }

                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: "暂无最近使用的工作区"
                                    color: "#666666"
                                    font.pixelSize: 12
                                }
                            }
                        }
                    }
                }
            }
        }

        // 操作按钮区域
        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            spacing: 15

            // 浏览文件夹按钮
            Button {
                text: "浏览文件夹"
                Layout.preferredWidth: 100
                Layout.preferredHeight: 40

                background: Rectangle {
                    color: parent.hovered ? "#4a4a4a" : "#3d3d3d"
                    radius: 6
                    border.color: parent.hovered ? "#5a5a5a" : "#3d3d3d"
                    border.width: 1
                }

                contentItem: RowLayout {
                    spacing: 8
                    anchors.centerIn: parent

                    Canvas {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16

                        onPaint: {
                            var ctx = getContext("2d");
                            ctx.reset();
                            ctx.strokeStyle = "#ffffff";
                            ctx.fillStyle = "#ffffff";
                            ctx.lineWidth = 1.5;
                            ctx.lineCap = "round";
                            ctx.lineJoin = "round";

                            // 绘制文件夹图标
                            // 文件夹顶部标签
                            ctx.beginPath();
                            ctx.moveTo(2, 4);
                            ctx.lineTo(6, 4);
                            ctx.lineTo(8, 6);
                            ctx.lineTo(2, 6);
                            ctx.stroke();

                            // 文件夹主体
                            ctx.beginPath();
                            ctx.moveTo(2, 6);
                            ctx.lineTo(2, 14);
                            ctx.lineTo(14, 14);
                            ctx.lineTo(14, 6);
                            ctx.lineTo(8, 6);
                            ctx.stroke();
                        }
                    }

                    Text {
                        text: parent.parent.text
                        color: "white"
                        font.pixelSize: 13
                        font.bold: true
                        font.family: "Source Han Sans, Microsoft YaHei, SimHei"
                    }
                }

                onClicked: {
                    folderDialogHelper.openDialog("选择工作目录")
                }
            }

            // 手动输入按钮
            Button {
                text: "手动输入"
                Layout.preferredWidth: 100
                Layout.preferredHeight: 40

                background: Rectangle {
                    color: parent.hovered ? "#4a4a4a" : "#3d3d3d"
                    radius: 6
                    border.color: parent.hovered ? "#5a5a5a" : "#3d3d3d"
                    border.width: 1
                }

                contentItem: RowLayout {
                    spacing: 8
                    anchors.centerIn: parent

                    Canvas {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16

                        onPaint: {
                            var ctx = getContext("2d");
                            ctx.reset();
                            ctx.strokeStyle = "#ffffff";
                            ctx.fillStyle = "#ffffff";
                            ctx.lineWidth = 1.5;
                            ctx.lineCap = "round";
                            ctx.lineJoin = "round";

                            // 绘制编辑/输入图标（类似铅笔）
                            // 笔的杆
                            ctx.beginPath();
                            ctx.moveTo(10, 3);
                            ctx.lineTo(3, 10);
                            ctx.lineTo(5, 12);
                            ctx.lineTo(12, 5);
                            ctx.closePath();
                            ctx.stroke();

                            // 笔的尖端
                            ctx.beginPath();
                            ctx.moveTo(3, 10);
                            ctx.lineTo(2, 13);
                            ctx.lineTo(5, 12);
                            ctx.closePath();
                            ctx.fill();
                        }
                    }

                    Text {
                        text: parent.parent.text
                        color: "white"
                        font.pixelSize: 13
                        font.bold: true
                        font.family: "Source Han Sans, Microsoft YaHei, SimHei"
                    }
                }

                onClicked: {
                    manualInputDialog.visible = true // 使用 visible 属性控制显示
                }
            }
        }
    }

    // 底部信息容器 - 固定在页面最底部
    ColumnLayout {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 20
        spacing: 5

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "嘉腾长沙创新中心"
            color: '#ffffff'
            font.pixelSize: 16
            font.bold: true
            font.family: "Source Han Sans, Microsoft YaHei, SimHei"
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "Jaten Changsha Innovation Center"
            color: "#999999"
            font.pixelSize: 12
            font.family: "Arial"
        }
    }

    // 文件夹选择对话框助手 (使用 C++ 实现的 FolderDialogHelper)
    FolderDialogHelper {
        id: folderDialogHelper
        onFolderSelected: function(folderPath) {
            selectWorkspace(folderPath)
        }
    }

    // 手动输入对话框
    Window {
        id: manualInputDialog
        modality: Qt.ApplicationModal
        flags: Qt.Dialog | Qt.WindowStaysOnTopHint
        title: "手动输入工作目录"
        width: 400
        height: 180
        visible: false
        x: (workspaceSelectorRoot.width - width) / 2 + workspaceSelectorRoot.x
        y: (workspaceSelectorRoot.height - height) / 2 + workspaceSelectorRoot.y

        Rectangle {
            anchors.fill: parent
            color: "#2d2d30"
            border.color: "#3e3e42"
            border.width: 1
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 15

                Text {
                    text: "请输入工作目录路径:"
                    color: "#cccccc"
                    font.pixelSize: 12
                }

                TextField {
                    id: manualPathInput
                    Layout.fillWidth: true
                    placeholderText: "例如: D:/MyWorkspace"
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
                            manualPathInput.clear()
                            manualInputDialog.close()
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
                        text: "确认"
                        enabled: manualPathInput.text.trim().length > 0

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

                        onClicked: {
                            var inputPath = manualPathInput.text.trim()
                            if (inputPath) {
                                selectWorkspace(inputPath)
                                manualPathInput.clear()
                                manualInputDialog.close()
                            }
                        }
                    }
                }
            }
        }
    }

    // JavaScript 函数
    function loadRecentWorkspaces() {
        if (mainController) {
            try {
                var workspaceHistory = mainController.GetWorkspaceHistory()
                recentWorkspaces = workspaceHistory || []
            } catch (e) {
                console.log("加载工作区历史失败: " + e.message)
                recentWorkspaces = []
            }
        }
    }

    function selectWorkspace(workspacePath) {
        if (!workspacePath) return

        // 添加到历史记录
        addToWorkspaceHistory(workspacePath)

        // 发出工作区选择信号
        workspaceSelected(workspacePath)
    }

    function addToWorkspaceHistory(workspacePath) {
        if (!mainController) return

        try {
            mainController.AddToWorkspaceHistory(workspacePath)
            loadRecentWorkspaces() // 重新加载历史记录
        } catch (e) {
            console.log("添加工作区历史失败: " + e.message)
        }
    }

    function removeWorkspaceFromHistory(index) {
        if (!mainController || index < 0 || index >= recentWorkspaces.length) return

        try {
            var workspacePath = recentWorkspaces[index].path
            mainController.RemoveFromWorkspaceHistory(workspacePath)
            loadRecentWorkspaces() // 重新加载历史记录
        } catch (e) {
            console.log("删除工作区历史失败: " + e.message)
        }
    }

    function getDirectoryName(path) {
        if (!path) return ""
        var parts = path.split(/[\/\\]/)
        return parts[parts.length - 1] || path
    }

    function formatLastUsed(timestamp) {
        if (!timestamp) return ""
        
        var now = new Date()
        var lastUsed = new Date(timestamp)
        var diffMs = now - lastUsed
        var diffDays = Math.floor(diffMs / (1000 * 60 * 60 * 24))

        if (diffDays === 0) {
            return "今天"
        } else if (diffDays === 1) {
            return "昨天"
        } else if (diffDays < 7) {
            return diffDays + "天前"
        } else {
            return lastUsed.toLocaleDateString()
        }
    }
}
