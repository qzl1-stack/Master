import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

/**
 * @brief 关于对话框（Window 版本）
 *
 * 显示应用程序的版本信息、Git信息和系统信息
 * 使用 Window 封装，无原生边框，居中显示在屏幕中央
 */
Window {
    id: aboutDialog
    title: "关于 JT Studio"
    width: 500
    height: 400
    modality: Qt.ApplicationModal
    flags: Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    visible: false
    color: "transparent"

    // 显示时居中于屏幕
    function centerOnScreen() {
        x = Screen.width / 2 - width / 2;
        y = Screen.height / 2 - height / 2;
    }

    onVisibleChanged: {
        if (visible) {
            centerOnScreen();
        }
    }

    // ==================== 窗口主体 ====================
    Rectangle {
        anchors.fill: parent
        color: "#1e1e1e"           // 深色背景，与主窗口风格一致
        radius: 8
        antialiasing: true

        // 轻描边
        layer.enabled: false
        border.color: "#3a3a3a"
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // ==================== 自定义标题栏 ====================
            Rectangle {
                id: aboutTitleBar
                Layout.fillWidth: true
                height: 40
                color: "transparent"
                radius: 8

                // 下半圆角遮掉（让标题栏底部为直角）
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: parent.height / 2
                    color: parent.color
                }

                RowLayout {
                    anchors.fill: parent
                    spacing: 0

                    // 标题文字
                    Text {
                        Layout.fillWidth: true
                        Layout.leftMargin: 16
                        text: "关于 JT Studio"
                        font.pixelSize: 13
                        color: "white"
                        opacity: 0.9
                        verticalAlignment: Text.AlignVCenter
                    }

                    // 关闭按钮
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
                        onClicked: aboutDialog.visible = false
                    }
                }

                // 拖动支持
                DragHandler {
                    target: null
                    onActiveChanged: if (active) {
                        aboutDialog.startSystemMove();
                    }
                }
            }

            // 分隔线
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#3a3a3a"
            }

            // ==================== 内容区 ====================
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 24
                spacing: 12

                // 应用程序标题
                Text {
                    text: "JT Studio"
                    font {
                        pixelSize: 24
                        bold: true
                        family: "Arial"
                    }
                    color: "#ffffff"
                }

                // 分隔线
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#3a3a3a"
                }

                // 版本信息
                Text {
                    text: "Version: " + appInfo.version
                    font {
                        pixelSize: 12
                        family: "Consolas"
                    }
                    color: "#aaaaaa"
                }

                // Qt版本信息
                Text {
                    text: "Qt Version: " + appInfo.buildInfo
                    font {
                        pixelSize: 12
                        family: "Consolas"
                    }
                    color: "#aaaaaa"
                }

                // Git Commit信息
                Text {
                    text: "Commit: " + appInfo.gitCommit
                    font {
                        pixelSize: 12
                        family: "Consolas"
                    }
                    color: "#aaaaaa"
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                }

                // Git日期
                Text {
                    text: "Date: " + appInfo.gitDate
                    font {
                        pixelSize: 12
                        family: "Consolas"
                    }
                    color: "#aaaaaa"
                }

                // 操作系统信息
                Text {
                    text: appInfo.osInfo
                    font {
                        pixelSize: 12
                        family: "Consolas"
                    }
                    color: "#aaaaaa"
                }

                // 弹性空间
                Item {
                    Layout.fillHeight: true
                }

                // 底部版权
                Text {
                    text: "© 2026 JT Studio. All rights reserved."
                    font.pixelSize: 10
                    color: "#666666"
                    Layout.alignment: Qt.AlignHCenter
                }

                // OK 按钮
                Button {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 80
                    Layout.preferredHeight: 32
                    text: "确定"
                    background: Rectangle {
                        color: parent.hovered ? "#005fa3" : "#0078d4"
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: aboutDialog.visible = false
                }
            }
        }
    }
}
