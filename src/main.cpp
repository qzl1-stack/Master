#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDebug>
#include <QQuickStyle>
#include "MainController.h"
#include "ProjectConfig.h"
#include "DataStore.h"
#include "update_checker.h"
#include "FolderDialogHelper.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>

void customMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // 确保日志目录存在
    QDir logDir(QCoreApplication::applicationDirPath() + "/Master_logs");
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }

    // 创建日志文件
    QString logFilePath = logDir.filePath("app_log.txt");
    QFile logFile(logFilePath);
    
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&logFile);
        
        QString logString;
        QTextStream logStream(&logString);

        switch (type) {
        case QtDebugMsg:
            logStream << "[Debug] ";
            break;
        case QtInfoMsg:
            logStream << "[Info] ";
            break;
        case QtWarningMsg:
            logStream << "[Warning] ";
            break;
        case QtCriticalMsg:
            logStream << "[Critical] ";
            break;
        case QtFatalMsg:
            logStream << "[Fatal] ";
            break;
        }

        logStream << msg ;
        
        stream << logString << Qt::endl; // 写入到文件
        logFile.close();
        
        // 同时输出到标准错误流
        fprintf(stderr, "%s\n", logString.toLocal8Bit().constData());
        fflush(stderr);
    }

}   

int main(int argc, char *argv[])
{
    qInstallMessageHandler(customMessageOutput);
    QApplication app(argc, argv);

    // 设置全局样式为Material
    QQuickStyle::setStyle("Material");

    qDebug() << "正在启动Master主控系统...";
    
    // 获取MainController单例实例
    MainController& mainController = MainController::GetInstance();
    qDebug() << "MainController实例已获取";

    // 创建QML引擎
    QQmlApplicationEngine engine;
    
    // 添加QML导入路径，确保能找到所有QML模块
    engine.addImportPath(QCoreApplication::applicationDirPath() + "/qml");

    // 注册UpdateChecker类型到QML系统
    qmlRegisterType<UpdateChecker>("Master", 1, 0, "UpdateChecker");
    
    // 注册FolderDialogHelper类型到QML系统
    qmlRegisterType<FolderDialogHelper>("Master", 1, 0, "FolderDialogHelper");

    // 创建文件夹选择对话框助手实例并暴露给QML
    FolderDialogHelper* folderDialogHelper = new FolderDialogHelper(&app);
    engine.rootContext()->setContextProperty("folderDialogHelper", folderDialogHelper);
    qDebug() << "FolderDialogHelper已注册并暴露给QML";

    engine.rootContext()->setContextProperty("updateCheckerInstance", mainController.GetUpdateChecker());
    qDebug() << "UpdateChecker类型已注册到QML系统";
    // 将MainController实例暴露给QML
    engine.rootContext()->setContextProperty("mainControllerInstance", &mainController);
    qDebug() << "MainController已暴露给QML上下文";
    
    // 连接对象创建失败信号
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { 
            qCritical() << "QML对象创建失败，应用程序退出";
            QCoreApplication::exit(-1); 
        },
        Qt::QueuedConnection);
    
    // 加载QML界面 - 使用模块导入方式
    engine.loadFromModule("Master", "Main");

    if (engine.rootObjects().isEmpty())
    {
        qCritical() << "QML界面加载失败";
        return -1;
    }
    qDebug() << "QML界面加载成功";


    // 连接应用程序退出信号到MainController的停止子进程槽函数
    QObject::connect(&app, &QApplication::aboutToQuit, &mainController, [&mainController]() {
        mainController.Stop();
    });
    qDebug() << "应用程序退出信号已连接到MainController::Stop";

    return app.exec();
}
