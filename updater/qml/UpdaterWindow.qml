import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15

ApplicationWindow {
    id: updaterRoot
    width: 400
    height: 200
    visible: true
    title: "软件更新器"
    
    Material.theme: Material.Light
    Material.accent: Material.Blue

    // 更新检查会在 Updater 构造函数中自动启动

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 15

        Text {
            text: updater ? updater.statusText : "正在准备更新..."
            Layout.alignment: Qt.AlignHCenter
            font.pixelSize: 16
            font.bold: true
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }

        ProgressBar {
            Layout.fillWidth: true
            value: updater ? updater.downloadProgress / 100.0 : 0
            visible: updater ? updater.showProgress : true
        }
        
        // 进度百分比显示
        Text {
            text: updater ? (updater.downloadProgress + "%") : "0%"
            Layout.alignment: Qt.AlignHCenter
            font.pixelSize: 14
            visible: updater ? updater.showProgress : true
        }
    }
    
    // 连接更新完成信号
    Connections {
        target: updater
        function onUpdateCompleted() {
            console.log("UpdaterWindow: 更新完成，自动创建桌面快捷方式");
            if (updater) {
                updater.createDesktopShortcut();
            }
            
            // 延迟退出，确保快捷方式创建完成和程序启动
            Qt.callLater(function() {
                console.log("UpdaterWindow: 所有操作完成，退出程序");
                Qt.quit();
            }, 3000); // 增加到3秒，确保程序有足够时间启动
        }
        
        function onUpdateFailed(errorMessage) {
            console.log("UpdaterWindow: 更新失败 - " + errorMessage);
            // 更新失败时也退出程序
            Qt.callLater(function() {
                Qt.quit();
            });
        }
    }
}