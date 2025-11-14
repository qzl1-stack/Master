#include "updater.h"
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QApplication>  
#include <QNetworkProxy>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QThread>
#include <QStandardPaths>
#include <QProcessEnvironment>
#include <QTextStream>
#include <QStringConverter>
#include <stdexcept>
#include <QTimer>
#include <QFileInfo>
#include <QDebug>
#include <QRegularExpression>

// 确保 moc 文件在最后包含
#include "moc_updater.cpp"

// 恢复 const QString 定义
const QString Updater::kCurrentVersion = "1.0.0"; // 版本号，与主程序一致
const QString Updater::kDownloadBaseUrl = "https://jts-tools-master.oss-cn-shanghai.aliyuncs.com/"; // 下载链接基础路径
const QString Updater::kAppName = "appMaster.exe";

Updater::Updater(QObject *parent)
    : QObject(parent)
    , network_manager_(new QNetworkAccessManager(this))
    , m_statusText(tr("正在连接服务器检查更新，请稍候..."))
    , m_titleText(tr("检查软件更新"))
    , m_newVersion("")
    , m_releaseNotes("")
    , m_downloadProgress(0)
    , m_showProgress(false)
    , m_showUpdateButton(false)
    , m_showReleaseNotes(false)
    , m_cancelButtonText(tr("取消"))
    , m_showCreateShortcut(false)
    , m_createShortcutChecked(true)
{
    // 配置网络访问
    QNetworkProxy proxy = QNetworkProxy::applicationProxy();
    if (proxy.type() != QNetworkProxy::NoProxy) {
        qDebug() << "使用系统代理:" << proxy.hostName() << ":" << proxy.port();
        network_manager_->setProxy(proxy);
    } else {
        qDebug() << "未使用代理";
    }
    
    // 输出 SSL 库信息
    qDebug() << "SSL 支持:" << QSslSocket::supportsSsl()
             << "SSL 库版本:" << QSslSocket::sslLibraryVersionString();
    
    // 延迟启动更新检查，确保 QML 界面已经加载
    QTimer::singleShot(100, this, &Updater::checkForUpdates);
}

Updater::~Updater()
{
    // network_manager_ 由 parent 机制自动删除，无需手动 delete
}

// Property setters with change notifications
void Updater::setStatusText(const QString &text)
{
    if (m_statusText != text) {
        m_statusText = text;
        emit statusTextChanged();
    }
}

void Updater::setTitleText(const QString &text)
{
    if (m_titleText != text) {
        m_titleText = text;
        emit titleTextChanged();
    }
}

void Updater::setNewVersion(const QString &version)
{
    if (m_newVersion != version) {
        m_newVersion = version;
        emit newVersionChanged();
    }
}

void Updater::setReleaseNotes(const QString &notes)
{
    if (m_releaseNotes != notes) {
        m_releaseNotes = notes;
        emit releaseNotesChanged();
    }
}

void Updater::setDownloadProgress(int progress)
{
    if (m_downloadProgress != progress) {
        m_downloadProgress = progress;
        emit downloadProgressChanged();
    }
}

void Updater::setShowProgress(bool show)
{
    if (m_showProgress != show) {
        m_showProgress = show;
        emit showProgressChanged();
    }
}

void Updater::setShowUpdateButton(bool show)
{
    if (m_showUpdateButton != show) {
        m_showUpdateButton = show;
        emit showUpdateButtonChanged();
    }
}

void Updater::setShowReleaseNotes(bool show)
{
    if (m_showReleaseNotes != show) {
        m_showReleaseNotes = show;
        emit showReleaseNotesChanged();
    }
}

void Updater::setCancelButtonText(const QString &text)
{
    if (m_cancelButtonText != text) {
        m_cancelButtonText = text;
        emit cancelButtonTextChanged();
    }
}

void Updater::setShowCreateShortcut(bool show)
{
    if (m_showCreateShortcut != show) {
        m_showCreateShortcut = show;
        emit showCreateShortcutChanged();
    }
}

void Updater::setCreateShortcutChecked(bool checked)
{
    if (m_createShortcutChecked != checked) {
        m_createShortcutChecked = checked;
        emit createShortcutCheckedChanged();
    }
}

// Public Q_INVOKABLE methods
void Updater::startUpdate()
{
    if (!download_url_.isEmpty()) {
        StartDownload(download_url_); // 调用 StartDownload 私有方法
    }
}

void Updater::cancelUpdate()
{
    qDebug() << "用户取消更新";
    QCoreApplication::quit();
}

void Updater::createDesktopShortcut()
{
    qDebug() << "开始创建桌面快捷方式";
    
    QString appPath = QCoreApplication::applicationDirPath() + "/" + kAppName;
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QString shortcutName = "Master.lnk";
    QString shortcutPath = desktopPath + "/" + shortcutName;
    
    qDebug() << "应用程序路径:" << appPath;
    qDebug() << "桌面路径:" << desktopPath;
    qDebug() << "快捷方式路径:" << shortcutPath;
    
    // 检查应用程序是否存在
    if (!QFile::exists(appPath)) {
        qWarning() << "应用程序不存在:" << appPath;
        setStatusText(tr("创建快捷方式失败：找不到应用程序"));
        return;
    }
    
    // 创建VBScript来创建快捷方式
    QString vbsPath = QDir::tempPath() + "/create_shortcut.vbs";
    QFile vbsFile(vbsPath);
    
    if (!vbsFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "无法创建VBS脚本:" << vbsPath;
        setStatusText(tr("创建快捷方式失败：无法创建脚本"));
        return;
    }
    
    QTextStream vbsOut(&vbsFile);
    vbsOut.setEncoding(QStringConverter::System);
    
    // VBScript内容
    vbsOut << "Set WshShell = CreateObject(\"WScript.Shell\")\n";
    vbsOut << "Set oShellLink = WshShell.CreateShortcut(\"" << QDir::toNativeSeparators(shortcutPath) << "\")\n";
    vbsOut << "oShellLink.TargetPath = \"" << QDir::toNativeSeparators(appPath) << "\"\n";
    vbsOut << "oShellLink.WorkingDirectory = \"" << QDir::toNativeSeparators(QCoreApplication::applicationDirPath()) << "\"\n";
    vbsOut << "oShellLink.Description = \"Master主控\"\n";
    vbsOut << "oShellLink.Save\n";
    
    vbsFile.close();
    
    qDebug() << "VBS脚本已创建:" << vbsPath;
    
    // 执行VBS脚本
    QProcess vbsProcess;
    vbsProcess.start("cscript.exe", QStringList() << "//NoLogo" << vbsPath);
    
    if (vbsProcess.waitForStarted(3000) && vbsProcess.waitForFinished(5000)) {
        QString output = QString::fromLocal8Bit(vbsProcess.readAllStandardOutput());
        QString errorOutput = QString::fromLocal8Bit(vbsProcess.readAllStandardError());
        
        qDebug() << "VBS脚本执行完成，退出码:" << vbsProcess.exitCode();
        qDebug() << "标准输出:" << output;
        qDebug() << "错误输出:" << errorOutput;
        
        if (vbsProcess.exitCode() == 0 && QFile::exists(shortcutPath)) {
            qDebug() << "桌面快捷方式创建成功:" << shortcutPath;
            setStatusText(tr("桌面快捷方式创建成功，正在启动程序..."));
            
            // 启动应用程序
            QProcess::startDetached(appPath);
            qDebug() << "应用程序已启动:" << appPath;
            setStatusText(tr("桌面快捷方式创建成功，程序已启动"));
        } else {
            qWarning() << "桌面快捷方式创建失败";
            setStatusText(tr("桌面快捷方式创建失败"));
        }
    } else {
        qWarning() << "VBS脚本执行失败";
        setStatusText(tr("创建快捷方式失败：脚本执行失败"));
    }
    
    // 清理临时VBS文件
    QFile::remove(vbsPath);
    
    qDebug() << "创建桌面快捷方式操作完成";
}

void Updater::checkForUpdates()
{
    qDebug() << "开始检查更新，当前版本:" << kCurrentVersion;
    
    // 诊断网络环境
    QNetworkProxy proxy = QNetworkProxy::applicationProxy();
    
    setTitleText(tr("检查软件更新"));
    setStatusText(tr("正在连接服务器检查更新，请稍候..."));
    setShowProgress(false);
    setShowUpdateButton(false);
    setShowReleaseNotes(false);
    setCancelButtonText(tr("取消"));
    
    // 使用 GitHub API 获取最新 Release
    QString url = QString("https://jts-tools-master.oss-cn-shanghai.aliyuncs.com/version.json");
    qDebug() << "请求 阿里云OSS API URL:" << url;
    
    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setRawHeader("User-Agent", "Master-Updater");
    
    // 配置 SSL
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyPeer);
    request.setSslConfiguration(sslConfig);
    
    QNetworkReply *reply = network_manager_->get(request);
    
    connect(reply, &QNetworkReply::errorOccurred, this, [this, reply](QNetworkReply::NetworkError error) {
        qWarning() << "网络请求错误:" << error;
        qWarning() << "错误描述:" << reply->errorString();
        
        // 详细的错误诊断
        switch(error) {
            case QNetworkReply::ConnectionRefusedError:
                qWarning() << "连接被拒绝，可能是网络或防火墙问题";
                break;
            case QNetworkReply::HostNotFoundError:
                qWarning() << "主机未找到，请检查网络连接";
                break;
            case QNetworkReply::TimeoutError:
                qWarning() << "网络请求超时";
                break;
            case QNetworkReply::ProxyConnectionRefusedError:
                qWarning() << "代理服务器连接被拒绝";
                break;
            case QNetworkReply::SslHandshakeFailedError:
                qWarning() << "SSL握手失败";
                break;
            default:
                qWarning() << "未知网络错误";
        }
        
        setTitleText(tr("检查更新失败"));
        setStatusText(tr("无法连接到更新服务器: %1").arg(reply->errorString()));
        qDebug() << "检查更新失败:" << reply->errorString();
        setCancelButtonText(tr("关闭"));
        
        // 网络请求失败时延迟退出
        QTimer::singleShot(3000, this, [this]() {
            qDebug() << "=== 网络请求失败，即将退出 updater 进程 ===";
            QCoreApplication::quit();
        });
    });
    
    connect(reply, &QNetworkReply::sslErrors, this, [this, reply](const QList<QSslError> &errors) {
        for (const QSslError &error : errors) {
            qWarning() << "SSL错误:" << error.errorString();
        }
        
        // 忽略 SSL 错误（仅用于调试，生产环境不建议）
        reply->ignoreSslErrors();
    });
    
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        OnVersionReply(reply);
    });
}

void Updater::OnVersionReply(QNetworkReply *reply)
{
    if (reply->error()) {
        setTitleText(tr("检查更新失败"));
        setStatusText(tr("无法连接到更新服务器: %1").arg(reply->errorString()));
        setCancelButtonText(tr("关闭"));
        qDebug() << "Network Error:" << reply->errorString();
        qDebug() << "Response:" << reply->readAll();
        
        // 网络回复错误时延迟退出
        QTimer::singleShot(3000, this, [this]() {
            qDebug() << "=== 网络回复错误，即将退出 updater 进程 ===";
            QCoreApplication::quit();
        });
        return;
    }

    QByteArray responseData = reply->readAll();
    qDebug() << "API Response:" << responseData;

    QJsonDocument json_doc = QJsonDocument::fromJson(responseData);
    reply->deleteLater();

    if (json_doc.isNull()) {
        setTitleText(tr("检查更新失败"));
        setStatusText(tr("服务器返回的数据格式不正确，无法解析版本信息"));
        setCancelButtonText(tr("关闭"));
        qDebug() << "JSON Parse Error: Document is null";
        
        // JSON 解析失败时延迟退出
        QTimer::singleShot(3000, this, [this]() {
            qDebug() << "=== JSON 解析失败，即将退出 updater 进程 ===";
            QCoreApplication::quit();
        });
        return;
    }

    if (!json_doc.isObject()) {
        setTitleText(tr("检查更新失败"));
        setStatusText(tr("服务器返回的版本信息格式错误"));
        setCancelButtonText(tr("关闭"));
        qDebug() << "JSON Parse Error: Not an object";
        
        // JSON 格式错误时延迟退出
        QTimer::singleShot(3000, this, [this]() {
            qDebug() << "=== JSON 格式错误，即将退出 updater 进程 ===";
            QCoreApplication::quit();
        });
        return;
    }

    ParseVersionInfo(json_doc.object());
}


void Updater::ParseVersionInfo(const QJsonObject &json)
{
    qDebug() << "Full JSON Object:" << json;


    QString download_url = json["download_url"].toString();
    if (download_url.isEmpty()) {
        setTitleText(tr("检查更新失败"));
        setStatusText(tr("服务器未提供更新包下载链接"));
        setCancelButtonText(tr("关闭"));
        qDebug() << "download_url Error: No download_url found";
        
        // 下载链接为空时延迟退出
        QTimer::singleShot(3000, this, [this]() {
            qDebug() << "=== 服务器未提供下载链接，即将退出 updater 进程 ===";
            QCoreApplication::quit();
        });
        return;
    }
    download_url_ = download_url;


    // 直接开始下载，不进行版本比较
    qDebug() << "开始自动下载更新包";
    setTitleText(tr("正在下载更新"));
    setStatusText(tr("正在下载更新包，请稍候..."));
    setShowProgress(true);
    setShowUpdateButton(false);
    setShowReleaseNotes(false);
    setCancelButtonText(tr("取消"));
    
    // 直接开始下载
    StartDownload(download_url_);
}

void Updater::StartDownload(const QString &url)
{
    qDebug() << "开始下载更新包，URL:" << url;
    
    setTitleText(tr("正在下载更新"));
    setStatusText(tr("正在下载更新包，请耐心等待..."));
    setShowProgress(true);
    setShowUpdateButton(false);
    setCancelButtonText(tr("取消"));

    QNetworkRequest request;
    request.setUrl(QUrl(url));
    
    qDebug() << "下载请求 headers:";
    foreach(QByteArray headerName, request.rawHeaderList()) {
        qDebug() << "  " << headerName << ":" << request.rawHeader(headerName);
    }

    QNetworkReply *reply = network_manager_->get(request);
    
    connect(reply, &QNetworkReply::errorOccurred, this, [this, reply](QNetworkReply::NetworkError error) {
        qWarning() << "下载请求错误:" << error << "错误描述:" << reply->errorString();
        setTitleText(tr("下载失败"));
        setStatusText(tr("下载更新包时出现错误: %1").arg(reply->errorString()));
        qDebug() << "下载失败:" << reply->errorString();
        setShowProgress(false);
        setCancelButtonText(tr("关闭"));
        emit updateFailed(reply->errorString());
    });
    
    connect(reply, &QNetworkReply::downloadProgress, this, &Updater::OnDownloadProgress);
    connect(reply, &QNetworkReply::finished, this, &Updater::OnDownloadFinished);
}

void Updater::OnDownloadProgress(qint64 bytes_received, qint64 bytes_total)
{
    if (bytes_total > 0) {
        int progress = static_cast<int>((bytes_received * 100) / bytes_total);
        setDownloadProgress(progress);
        setStatusText(tr("正在下载更新包... %1 MB / %2 MB (%3%)")
            .arg(bytes_received / 1024.0 / 1024.0, 0, 'f', 2)
            .arg(bytes_total / 1024.0 / 1024.0, 0, 'f', 2)
            .arg(progress));
    } else {
        // 如果总大小未知，只显示已下载大小
        setStatusText(tr("正在下载更新包... %1 MB")
            .arg(bytes_received / 1024.0 / 1024.0, 0, 'f', 2));
    }
}

void Updater::OnDownloadFinished()
{
    qDebug() << "=== OnDownloadFinished 开始执行 === (PID:" << QCoreApplication::applicationPid() << ")";

    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        qCritical() << "OnDownloadFinished: 没有有效的 QNetworkReply 对象";
        emit updateFailed(tr("网络请求对象无效"));
        return;
    }

    QScopedPointer<QNetworkReply> reply_deleter(reply);

    if (reply->error() != QNetworkReply::NoError) {
        qCritical() << "下载失败:" << reply->errorString();
        setTitleText(tr("下载失败"));
        setStatusText(tr("下载更新包时出现错误: %1").arg(reply->errorString()));
        setShowProgress(false);
        setCancelButtonText(tr("关闭"));
        emit updateFailed(reply->errorString());
        
        // 下载失败时延迟退出
        QTimer::singleShot(3000, this, [this]() {
            qDebug() << "=== 下载失败，即将退出 updater 进程 ===";
            QCoreApplication::quit();
        });
        return;
    }

    // 保存下载的文件
    file_path_ = QDir::tempPath() + "/Master.zip";
    qDebug() << "下载成功，正在保存临时文件至:" << file_path_;

    QFile file(file_path_);
    if (!file.open(QIODevice::WriteOnly)) {
        qCritical() << "无法打开临时文件进行写入:" << file_path_ << "错误:" << file.errorString();
        setTitleText(tr("保存失败"));
        setStatusText(tr("无法保存更新文件到本地: %1").arg(file.errorString()));
        setShowProgress(false);
        setCancelButtonText(tr("关闭"));
        emit updateFailed(file.errorString());
        
        // 文件保存失败时延迟退出
        QTimer::singleShot(3000, this, [this]() {
            qDebug() << "=== 文件保存失败，即将退出 updater 进程 ===";
            QCoreApplication::quit();
        });
        return;
    }

    qint64 bytesWritten = file.write(reply->readAll());
    file.close();

    qDebug() << "文件保存成功，大小:" << QFileInfo(file_path_).size() << "bytes";
    qDebug() << "实际写入字节数:" << bytesWritten;

    setTitleText(tr("准备安装"));
    setStatusText(tr("下载完成，正在关闭主程序准备安装更新..."));
    setShowProgress(false);
    
    qDebug() << "OnDownloadFinished: 文件保存成功。即将调用 CloseMainApp()。";
    
    // 尝试关闭主程序
    try {
        CloseMainApp();
    } catch (const std::exception& e) {
        qCritical() << "CloseMainApp 执行时发生异常:" << e.what();
    } catch (...) {
        qCritical() << "CloseMainApp 发生未知异常";
    }
    
    // 使用 QTimer 延迟执行 InstallUpdate
    QTimer::singleShot(100, this, [this]() {
        try {
            InstallUpdate();
        } catch (const std::exception& e) {
            qCritical() << "InstallUpdate 执行时发生异常:" << e.what();
            setStatusText(tr("更新失败: %1").arg(e.what()));
            emit updateFailed(e.what());
        } catch (...) {
            qCritical() << "InstallUpdate 发生未知异常";
            setStatusText(tr("更新失败：未知错误"));
            emit updateFailed(tr("未知错误"));
        }
    });

    qDebug() << "=== OnDownloadFinished 执行完毕 ===";
}

void Updater::CloseMainApp()
{
    qDebug() << "=== CloseMainApp 开始执行 === (PID:" << QCoreApplication::applicationPid() << ")";
    
    // 禁用自动退出 - 使用 QApplication 而非 QCoreApplication
    QApplication* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    if (app) {
        app->setQuitOnLastWindowClosed(false);
    }
    
    qDebug() << "尝试关闭主应用程序:" << kAppName;
    
    QStringList killCommands = {
        "taskkill /F /IM " + kAppName,
        "wmic process where name='" + kAppName + "' delete"
    };

    bool processKilled = false;
    for (const QString& cmd : killCommands) {
        QProcess killProcess;
        killProcess.start("cmd.exe", QStringList() << "/c" << cmd);
        
        if (killProcess.waitForStarted(3000) && killProcess.waitForFinished(5000)) {
            QString output = QString::fromLocal8Bit(killProcess.readAllStandardOutput());
            QString errorOutput = QString::fromLocal8Bit(killProcess.readAllStandardError());
            
            qDebug() << "命令:" << cmd;
            qDebug() << "退出码:" << killProcess.exitCode();
            qDebug() << "标准输出:" << output;
            qDebug() << "错误输出:" << errorOutput;
            
            if (killProcess.exitCode() == 0) {
                processKilled = true;
                break;
            }
        }
    }

    if (!processKilled) {
        qWarning() << "无法使用标准方法关闭 Master.exe";
    }

    // 检查进程是否已经关闭
    QProcess checkProcess;
    checkProcess.start("tasklist", QStringList() << "/NH" << "/FI" << QString("IMAGENAME eq %1").arg(kAppName));
    
    if (checkProcess.waitForStarted(3000) && checkProcess.waitForFinished(3000)) {
        QString output = QString::fromLocal8Bit(checkProcess.readAllStandardOutput());
        qDebug() << "进程检查结果:" << output.trimmed();
        
        if (output.contains(kAppName, Qt::CaseInsensitive)) {
            qWarning() << "Master.exe 仍在运行";
        } else {
            qDebug() << "Master.exe 已成功关闭";
        }
    }

    // 强制继续执行
    qDebug() << "=== CloseMainApp 即将结束，强制继续更新流程 ===";
    
    // 添加一个小延时，确保日志能够被写入
    QThread::msleep(500);
}

void Updater::InstallUpdate()
{
    QString extract_path = QCoreApplication::applicationDirPath();
    
    qDebug() << "=== InstallUpdate 函数开始执行 ===";
    qDebug() << "开始解压文件:" << file_path_;
    qDebug() << "解压目标路径:" << extract_path;

    if (!QFile::exists(file_path_)) {
        qWarning() << "下载文件不存在:" << file_path_;
        setStatusText(tr("下载文件丢失"));
        emit updateFailed(tr("下载文件丢失"));
        
        // 文件不存在时延迟退出
        QTimer::singleShot(3000, this, [this]() {
            qDebug() << "=== 下载文件丢失，即将退出 updater 进程 ===";
            QCoreApplication::quit();
        });
        return;
    }

    QFileInfo fileInfo(file_path_);
    if (fileInfo.size() == 0) {
        qWarning() << "下载文件大小为0:" << file_path_;
        setStatusText(tr("下载文件为空"));
        emit updateFailed(tr("下载文件为空"));
        
        // 文件为空时延迟退出
        QTimer::singleShot(3000, this, [this]() {
            qDebug() << "=== 下载文件为空，即将退出 updater 进程 ===";
            QCoreApplication::quit();
        });
        return;
    }

    qDebug() << "文件检查通过，大小:" << fileInfo.size() << "bytes";
    setStatusText(tr("正在准备更新..."));
    
    // 创建批处理脚本来执行更新
    QString batchPath = QDir::tempPath() + "/Master_update.bat";
    qDebug() << "批处理脚本路径:" << batchPath;
    QFile batchFile(batchPath);
    
    if (!batchFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "无法创建批处理脚本:" << batchPath;
        setStatusText(tr("无法创建更新脚本"));
        emit updateFailed(tr("无法创建更新脚本"));
        
        // 无法创建批处理脚本时延迟退出
        QTimer::singleShot(3000, this, [this]() {
            qDebug() << "=== 无法创建批处理脚本，即将退出 updater 进程 ===";
            QCoreApplication::quit();
        });
        return;
    }

    QTextStream out(&batchFile);
    out.setEncoding(QStringConverter::System);
    
    // 批处理脚本内容
    out << "@echo off\n";
    out << "echo [Master-Updater] 批处理脚本开始执行\n";
    out << "echo [Master-Updater] 等待更新程序退出...\n";
    out << "timeout /t 1 /nobreak >nul\n"; // 减少到1秒，因为updater.exe会在2秒后自动退出
    out << "echo [Master-Updater] 开始解压更新文件...\n";
    
    QString tarCmd = QString("tar -xf \"%1\" -C \"%2\" --strip-components=1")
                        .arg(QDir::toNativeSeparators(file_path_))
                        .arg(QDir::toNativeSeparators(extract_path));
    out << "echo [Master-Updater] 执行命令: " << tarCmd << "\n";
    out << tarCmd << "\n";
    
    out << "if %errorlevel% neq 0 (\n";
    out << "    echo [Master-Updater] 解压失败，错误代码: %errorlevel%\n";
    out << "    echo [Master-Updater] 按任意键继续...\n";
    out << "    pause\n";
    out << "    exit /b 1\n";
    out << ")\n";
    
    out << "echo [Master-Updater] 解压完成，删除临时文件...\n";
    out << "del \"" << QDir::toNativeSeparators(file_path_) << "\"\n";
    
    out << "echo [Master-Updater] 启动新版本...\n";
    QString newAppPath = QDir::toNativeSeparators(extract_path + "/" + kAppName);
    out << "echo [Master-Updater] 新版本路径: " << newAppPath << "\n";
    out << "start \"\" \"" << newAppPath << "\"\n";
    
    out << "echo [Master-Updater] 等待程序启动...\n";
    out << "timeout /t 1 /nobreak >nul\n";
    out << "echo [Master-Updater] 程序启动完成\n";
    
    out << "echo [Master-Updater] 清理批处理脚本...\n";
    out << "timeout /t 2 /nobreak >nul\n"; // 等待2秒再删除自己
    out << "del \"" << QDir::toNativeSeparators(batchPath) << "\"\n";
    out << "echo [Master-Updater] 更新完成\n";
    out << "exit\n";
    
    batchFile.close();
    
    qDebug() << "批处理脚本已创建:" << batchPath;

    // 显示创建快捷方式选项，而不是立即退出
    setTitleText(tr("更新完成"));
    setStatusText(tr("软件更新完成！新版本已经启动。"));
    setShowCreateShortcut(true);
    setCancelButtonText(tr("完成"));
    
    QStringList args;
    args << "/k" << batchPath;
    
    qDebug() << "准备启动批处理脚本: cmd.exe" << args.join(" ");
    bool started = QProcess::startDetached("cmd.exe", args);
    
    if (started) {
        createDesktopShortcut();
        
        // 延迟退出 updater 进程，确保批处理脚本能够正常启动和程序启动
        QTimer::singleShot(3000, this, [this]() {
            qDebug() << "=== InstallUpdate 完成，即将退出 updater 进程 ===";
            QCoreApplication::quit();
        });
    } else {
        qWarning() << "无法启动批处理脚本";
        setStatusText(tr("启动更新脚本失败"));
        emit updateFailed(tr("无法启动更新脚本"));
        
        // 即使失败也要退出
        QTimer::singleShot(3000, this, [this]() {
            qDebug() << "=== 更新失败，即将退出 updater 进程 ===";
            QCoreApplication::quit();
        });
    }
    
    qDebug() << "=== InstallUpdate 函数执行结束 ===";
} 