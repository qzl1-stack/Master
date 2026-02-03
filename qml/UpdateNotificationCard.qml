import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: updateCard
    width: 320
    height: 85
    radius: 10
    color: "#1E1E1E"
    border.color: "#3A3A3A"
    border.width: 1
    enabled: true

    // 阴影效果
    layer.enabled: true

    // 属性
    property string newVersion: ""
    property string releaseNotes: ""
    property string currentVersion: ""

    // 信号
    signal updateClicked
    signal closeClicked
    signal laterClicked

    // 显示动画 - 从底部滑入
    ParallelAnimation {
        id: showAnimation
        NumberAnimation {
            target: updateCard
            property: "opacity"
            from: 0
            to: 1
            duration: 300
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: updateCard
            property: "y"
            from: updateCard.y + 20
            to: updateCard.y
            duration: 300
            easing.type: Easing.OutCubic
        }
    }

    // 关闭动画 - 向下滑出
    ParallelAnimation {
        id: hideAnimation
        NumberAnimation {
            target: updateCard
            property: "opacity"
            from: 1
            to: 0
            duration: 250
            easing.type: Easing.InCubic
        }
        NumberAnimation {
            target: updateCard
            property: "y"
            from: updateCard.y
            to: updateCard.y + 20
            duration: 250
            easing.type: Easing.InCubic
        }
        onFinished: updateCard.visible = false
    }

    function show() {
        visible = true;
        showAnimation.start();
    }

    function hide() {
        hideAnimation.start();
    }

    // 主内容区域
    RowLayout {
        anchors {
            fill: parent
            margins: 12
        }
        spacing: 10

        // 左侧：图标和信息
        Row {
            Layout.fillWidth: true
            spacing: 10

            // 现代化更新图标
            Rectangle {
                width: 36
                height: 36
                radius: 6
                color: "#2A2A2A"
                border.color: "#3D7EFF"
                border.width: 1.5
                anchors.verticalCenter: parent.verticalCenter

                // 下载图标（简约风格）
                Canvas {
                    id: downloadIcon
                    anchors.centerIn: parent
                    width: 20
                    height: 20

                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.clearRect(0, 0, width, height);
                        ctx.strokeStyle = "#3D7EFF";
                        ctx.fillStyle = "#3D7EFF";
                        ctx.lineWidth = 1.5;
                        ctx.lineCap = "round";
                        ctx.lineJoin = "round";

                        // 向下箭头
                        ctx.beginPath();
                        ctx.moveTo(10, 3);
                        ctx.lineTo(10, 12);
                        ctx.stroke();

                        // 箭头头部
                        ctx.beginPath();
                        ctx.moveTo(6, 9);
                        ctx.lineTo(10, 13);
                        ctx.lineTo(14, 9);
                        ctx.stroke();

                        // 底部横线
                        ctx.beginPath();
                        ctx.moveTo(4, 17);
                        ctx.lineTo(16, 17);
                        ctx.stroke();
                    }
                }
            }

            // 版本信息
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3

                Text {
                    text: "发现新版本"
                    font.pixelSize: 13
                    font.bold: true
                    color: "#E8E8E8"
                    font.family: "Microsoft YaHei"
                }

                // Text {
                //     text: qsTr("%1 → %2").arg(currentVersion).arg(newVersion)
                //     font.pixelSize: 11
                //     color: "#9A9A9A"
                //     font.family: "Microsoft YaHei"
                // }
            }
        }

        // 右侧：操作按钮
        Row {
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            spacing: 8

            // 立即更新按钮（渐变蓝绿色）
            Rectangle {
                id: updateButton
                width: 75
                height: 28
                radius: 5

                gradient: Gradient {
                    GradientStop {
                        position: 0.0
                        color: updateMouseArea.pressed ? "#2B5FCC" : updateMouseArea.hovered ? "#4A7FFF" : "#3D7EFF"
                    }
                    GradientStop {
                        position: 1.0
                        color: updateMouseArea.pressed ? "#1E4599" : updateMouseArea.hovered ? "#3D70E6" : "#2E5FD9"
                    }
                }

                Behavior on opacity {
                    NumberAnimation {
                        duration: 150
                    }
                }

                Text {
                    text: "立即更新"
                    font.pixelSize: 11
                    font.bold: true
                    color: "#FFFFFF"
                    anchors.centerIn: parent
                    font.family: "Microsoft YaHei"
                }

                MouseArea {
                    id: updateMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: updateCard.updateClicked()
                }
            }

            // 稍后提醒按钮
            Rectangle {
                id: laterButton
                width: 60
                height: 28
                radius: 5
                border.color: "#3A3A3A"
                border.width: 1
                color: laterMouseArea.pressed ? "#3A3A3A" : laterMouseArea.hovered ? "#2D2D2D" : "#252525"

                Behavior on color {
                    ColorAnimation {
                        duration: 150
                    }
                }

                Text {
                    text: "稍后"
                    font.pixelSize: 11
                    color: "#9A9A9A"
                    anchors.centerIn: parent
                    font.family: "Microsoft YaHei"
                }

                MouseArea {
                    id: laterMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: updateCard.laterClicked()
                }
            }
        }
    }
}
