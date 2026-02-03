#include "DataStore.h"
#include "FolderDialogHelper.h"
#include "MainController.h"
#include "ProjectConfig.h"
#include "update_checker.h"
#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTextStream>

void cleanupLogFile(const QString &logFilePath,
                    qint64 maxSizeBytes = 10 * 1024 * 1024) {
  QFileInfo fileInfo(logFilePath);
  if (fileInfo.exists() && fileInfo.size() > maxSizeBytes) {
    QFile logFile(logFilePath);
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Truncate |
                     QIODevice::Text)) {
      QTextStream stream(&logFile);
      stream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")
             << " [Info] 日志文件已清理（超过大小限制 "
             << (maxSizeBytes / 1024 / 1024) << "MB）" << Qt::endl;
      logFile.close();
    }
  }
}

// 自定义消息处理程序
void customMessageOutput(QtMsgType type, const QMessageLogContext &context,
                         const QString &msg) {
  // 确保日志目录存在
  QDir logDir(QCoreApplication::applicationDirPath() + "/logs");
  if (!logDir.exists()) {
    logDir.mkpath(".");
  }

  // 创建日志文件
  QString logFilePath = logDir.filePath("Master_log.txt");
  QFile logFile(logFilePath);

  cleanupLogFile(logFilePath);

  if (logFile.open(QIODevice::WriteOnly | QIODevice::Append |
                   QIODevice::Text)) {
    QTextStream stream(&logFile);
    stream << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz")
           << " ";

    switch (type) {
    case QtDebugMsg:
      stream << "[Debug] ";
      break;
    case QtInfoMsg:
      stream << "[Info] ";
      break;
    case QtWarningMsg:
      stream << "[Warning] ";
      break;
    case QtCriticalMsg:
      stream << "[Critical] ";
      break;
    case QtFatalMsg:
      stream << "[Fatal] ";
      break;
    }

    stream << msg << Qt::endl;
    logFile.close();
  }

  // 同时输出到标准错误流
  fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
  fflush(stderr);
}

int main(int argc, char *argv[]) {
  qInstallMessageHandler(customMessageOutput);
  QApplication app(argc, argv);

  // 设置全局样式为Material
  QQuickStyle::setStyle("Material");

  qDebug() << "正在启动Master主控系统...";

  // 获取MainController单例实例
  MainController &mainController = MainController::GetInstance();
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
  FolderDialogHelper *folderDialogHelper = new FolderDialogHelper(&app);
  engine.rootContext()->setContextProperty("folderDialogHelper",
                                           folderDialogHelper);
  qDebug() << "FolderDialogHelper已注册并暴露给QML";

  engine.rootContext()->setContextProperty("updateCheckerInstance",
                                           mainController.GetUpdateChecker());
  qDebug() << "UpdateChecker类型已注册到QML系统";
  // 将MainController实例暴露给QML
  engine.rootContext()->setContextProperty("mainControllerInstance",
                                           &mainController);
  qDebug() << "MainController已暴露给QML上下文";

  // 连接对象创建失败信号
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
      []() {
        qCritical() << "QML对象创建失败，应用程序退出";
        QCoreApplication::exit(-1);
      },
      Qt::QueuedConnection);

  // 加载QML界面 - 使用模块导入方式
  engine.loadFromModule("Master", "Main");

  if (engine.rootObjects().isEmpty()) {
    qCritical() << "QML界面加载失败";
    return -1;
  }
  qDebug() << "QML界面加载成功";

  // 连接应用程序退出信号到MainController的停止子进程槽函数
  QObject::connect(&app, &QApplication::aboutToQuit, &mainController,
                   [&mainController]() { mainController.Stop(); });
  qDebug() << "应用程序退出信号已连接到MainController::Stop";

  return app.exec();
}
