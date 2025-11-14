#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDir>
#include <QStandardPaths>
#include <QTextStream>
#include <QDateTime>
#include <QException>
#include <QCoreApplication>
#include <QDebug>
#include <iostream>
#include "updater.h"
#include <QVariant>
#include <QQuickWindow>

void customMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString txt;
    switch (type) {
    case QtDebugMsg:
        txt = QString("Debug: %1").arg(msg);
        break;
    case QtWarningMsg:
        txt = QString("Warning: %1").arg(msg);
        break;
    case QtCriticalMsg:
        txt = QString("Critical: %1").arg(msg);
        break;
    case QtFatalMsg:
        txt = QString("Fatal: %1").arg(msg);
        abort();
    }
    
    // 输出到 stderr，这样可以在父进程中捕获
    std::cerr << txt.toStdString() << " (" << context.file << ":" << context.line << ", " << context.function << ")" << std::endl;
    
    // 同时写入日志文件
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
    QDir().mkpath(logDir);
    QString logFile = logDir + "/updater2_log.txt";
    
    QFile file(logFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        QTextStream stream(&file);
        stream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") 
               << " " << txt << " (" << context.file << ":" << context.line << ")" << Qt::endl;
    }
}

void globalExceptionHandler()
{
    try {
        throw;
    } catch (const QException &e) {
        qCritical() << "未捕获的Qt异常:" << e.what();
    } catch (const std::exception &e) {
        qCritical() << "未捕获的标准异常:" << e.what();
    } catch (...) {
        qCritical() << "未捕获的未知异常";
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 安装全局异常处理器
    std::set_terminate(globalExceptionHandler);
    
    // 安装自定义消息输出处理器
    qInstallMessageHandler(customMessageOutput);
    
    // 设置应用程序信息
    app.setApplicationName("Log_analyzer Updater");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Log_analyzer");
    
    // 设置不在最后一个窗口关闭时退出
    app.setQuitOnLastWindowClosed(false);
    
    // 强制使用 Basic 样式以支持自定义
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    
    qDebug() << "Updater 进程启动，PID:" << QCoreApplication::applicationPid();
    
    // 创建 QML 引擎
    QQmlApplicationEngine engine;

    // 创建更新器实例
    Updater updater;
    
    // 将 updater 对象暴露给 QML
    engine.rootContext()->setContextProperty("updater", &updater);

    // 加载 QML 界面
    const QUrl url(QStringLiteral("qrc:/qml/UpdaterWindow.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    // 确保 QML 加载完毕且窗口已显示后，从 C++ 端触发更新检查
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &updater, 
        [&updater](QObject *obj, const QUrl &objUrl) {
            if (obj && objUrl == QUrl(QStringLiteral("qrc:/qml/UpdaterWindow.qml"))) {
                updater.checkForUpdates(); // 调用 checkForUpdates 方法
            }
        });

    // 连接更新完成信号到应用退出
    QObject::connect(&updater, &Updater::updateCompleted, &app, []() {
        qDebug() << "Update completed, exiting application";
        QCoreApplication::quit();
    });
    
    QObject::connect(&updater, &Updater::updateFailed, &app, [](const QString &error) {
        qWarning() << "Update failed:" << error;
        // 不立即退出，让用户看到错误信息
    });
    
    qDebug() << "Updater 进程退出码:" << app.exec();
    
    return 0;
} 