import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
// import Qt5Compat.GraphicalEffects

Rectangle {
    id: updateCard
    width: 500
    height: 500
    radius: 10
    color: "#FFFFFF"
    border.color: "#E8E8E8"
    border.width: 1
    enabled: true
    
    // 阴影效果
    layer.enabled: true
    // layer.effect: DropShadow {
    //     transparentBorder: true
    //     horizontalOffset: 0
    //     verticalOffset: 4
    //     radius: 16
    //     samples: 33
    //     color: "#1A000000"
    // }
    
    // 属性
    property string newVersion: ""
    property string releaseNotes: ""
    property string currentVersion: ""
    
    // 信号
    signal updateClicked()
    signal closeClicked()
    signal laterClicked()
    
    // 显示动画
    NumberAnimation on opacity {
        id: showAnimation
        from: 0
        to: 1
        duration: 300
        easing.type: Easing.OutCubic
    }
    
    // 关闭动画
    NumberAnimation {
        id: hideAnimation
        target: updateCard
        property: "opacity"
        from: 1
        to: 0
        duration: 250
        easing.type: Easing.InCubic
        onFinished: updateCard.visible = false
    }
    
    function show() {
        visible = true
        showAnimation.start()
    }
    
    function hide() {
        hideAnimation.start()
    }
    
    Column {
        id: content
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: 20
        }
        spacing: 16
        
        // 标题行
        Row {
            width: parent.width
            spacing: 12
            
            // 更新图标
            Rectangle {
                width: 32
                height: 32
                radius: 16
                color: "#4CAF50"
                
                Text {
                    anchors.centerIn: parent
                    text: "↑"
                    color: "white"
                    font.pixelSize: 20
                    font.bold: true
                }
            }
            
            Column {
                width: parent.width - 44 - 32  // 减去图标和关闭按钮的宽度
                spacing: 4
                
                Text {
                    text: "发现新版本"
                    font.pixelSize: 18
                    font.bold: true
                    color: "#333333"
                }
                
                Text {
                    text: qsTr("当前版本: %1 → 新版本: %2").arg(currentVersion).arg(newVersion)
                    font.pixelSize: 14
                    color: "#666666"
                }
            }
            
            // 关闭按钮
            Rectangle {
                id: closeButton
                width: 32
                height: 32
                radius: 16
                color: closeMouseArea.hovered ? "#F5F5F5" : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: "×"
                    font.pixelSize: 18
                    color: "#999999"
                }

                MouseArea {
                    id: closeMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: updateCard.closeClicked()
                }
            }
        }
        
        // 更新说明
        Flickable {
            id: releaseNotesFlickable
            width: parent.width
            height: 340  // 固定高度
            contentWidth: width
            contentHeight: releaseNotesText.height
            clip: true
            
            // 启用垂直滚动
            flickableDirection: Flickable.VerticalFlick
            
            // 启用交互
            interactive: true
            
            // 滚动条
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AlwaysOn
            }
            
            Rectangle {
                width: parent.width
                height: releaseNotesText.height
                color: "#F8F9FA"
                radius: 8
                border.color: "#E9ECEF"
                border.width: 1
                
                Text {
                    id: releaseNotesText
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                        margins: 12
                    }
                    text: releaseNotes || "暂无更新说明"  // 添加默认文本
                    font.pixelSize: 16
                    color: "#555555"
                    wrapMode: Text.WordWrap
                    textFormat: Text.RichText  // 确保支持 HTML 格式
                }
            }
        }
        
        // 按钮行
        Row {
            width: parent.width
            spacing: 12
            
            Rectangle {
                id: updateButton
                width: 100
                height: 36
                radius: 6
                color: updateMouseArea.pressed ? "#45A049" : 
                       updateMouseArea.hovered ? "#5CBF60" : "#4CAF50"

                Text {
                    text: "立即更新"
                    font.pixelSize: 13
                    font.bold: true
                    color: "white"
                    anchors.centerIn: parent
                }
                
                MouseArea {
                    id: updateMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: updateCard.updateClicked()
                }
            }
            
            Rectangle {
                id: laterButton
                width: 100
                height: 36
                radius: 6
                border.color: "#CCCCCC"
                border.width: 1
                color: laterMouseArea.pressed ? "#E0E0E0" : 
                       laterMouseArea.hovered ? "#F0F0F0" : "#F5F5F5"

                Text {
                    text: "稍后提醒"
                    font.pixelSize: 13
                    color: "#666666"
                    anchors.centerIn: parent
                }

                MouseArea {
                    id: laterMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: updateCard.laterClicked()
                }
            }
            
            // 占位符，让按钮靠左对齐
            Item {
                width: parent.width - updateButton.width - laterButton.width - 12
                height: 1
            }
        }
    }
} 
