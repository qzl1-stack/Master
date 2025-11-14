import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: settingsView
    color: "#1e1e1e"

    property var mainController: null // MainController C++ 实例引用

    ScrollView {
        anchors.fill: parent
        anchors.margins: 20

        ColumnLayout {
            width: parent.width
            spacing: 20

            Text {
                text: "设置 & 系统统计"
                font.pixelSize: 24
                font.bold: true
                color: "#cccccc"
            }

            // 系统统计信息
            GroupBox {
                title: "系统统计信息"
                Layout.fillWidth: true
                
                background: Rectangle {
                    color: "#2d2d30"
                    border.color: "#3e3e42"
                    border.width: 1
                    radius: 5
                }
                
                label: Text {
                    text: parent.title
                    color: "#cccccc"
                    font.pixelSize: 14
                    font.bold: true
                }
                
                GridLayout {
                    columns: 2
                    rowSpacing: 10
                    columnSpacing: 20
                    anchors.fill: parent
                    anchors.margins: 10

                    Text { text: "系统运行时长:"; color: "#cccccc"; font.pixelSize: 12 }
                    Text { text: getUptime(); color: "#ffffff"; font.pixelSize: 12; font.bold: true }
                    
                    Text { text: "已处理消息数:"; color: "#cccccc"; font.pixelSize: 12 }
                    Text { text: getProcessedMessages(); color: "#ffffff"; font.pixelSize: 12; font.bold: true }

                    Text { text: "已执行命令数:"; color: "#cccccc"; font.pixelSize: 12 }
                    Text { text: getExecutedCommands(); color: "#ffffff"; font.pixelSize: 12; font.bold: true }

                    Text { text: "配置更新次数:"; color: "#cccccc"; font.pixelSize: 12 }
                    Text { text: getConfigUpdates(); color: "#ffffff"; font.pixelSize: 12; font.bold: true }

                    Text { text: "进程重启次数:"; color: "#cccccc"; font.pixelSize: 12 }
                    Text { text: getProcessRestarts(); color: "#ffffff"; font.pixelSize: 12; font.bold: true }

                    Text { text: "系统健康状态:"; color: "#cccccc"; font.pixelSize: 12 }
                    Text { text: getSystemHealth() ? "健康" : "异常"; color: getSystemHealth() ? "#4CAF50" : "#f44336"; font.pixelSize: 12; font.bold: true }
                }
            }
        }
    }

    // ==================== 状态获取函数 ====================
    
    function getUptime() {
        if (!mainController || !mainController.IsSystemStarted()) return "未运行"
        // 实际应从后端获取更精确的运行时长
        return "N/A"
    }

    function getProcessedMessages() {
        if (!mainController) return "0"
        var stats = mainController.GetSystemStatistics()
        return stats.total_processed_messages || "0"
    }

    function getExecutedCommands() {
        if (!mainController) return "0"
        var stats = mainController.GetSystemStatistics()
        return stats.total_executed_commands || "0"
    }

    function getConfigUpdates() {
        if (!mainController) return "0"
        var stats = mainController.GetSystemStatistics()
        return stats.total_config_updates || "0"
    }

    function getProcessRestarts() {
        if (!mainController) return "0"
        var stats = mainController.GetSystemStatistics()
        return stats.total_process_restarts || "0"
    }

    function getSystemHealth() {
        if (!mainController) return false
        return mainController.IsSystemHealthy()
    }
}
