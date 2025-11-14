#ifndef UPDATE_CHECKER_H_
#define UPDATE_CHECKER_H_

// 文件功能：更新检查器类声明，负责启动时自动检查更新

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QJsonObject>
#include <QDebug> // Added for qDebug

class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    explicit UpdateChecker(QObject* parent = nullptr);
    ~UpdateChecker();
    
    // 启动自动更新检查
    Q_INVOKABLE void startAutoUpdateCheck();  // 修改方法名为小写开头
    
    // 启动更新
    Q_INVOKABLE void startUpdate() {
        if (!download_url_.isEmpty()) {
            // 调用下载更新的方法
            qDebug() << "开始下载更新：" << download_url_;
        }
    }
    
    // 获取新版本号
    Q_INVOKABLE QString newVersion() const { return new_version_; }
    
    // 获取当前版本号
    Q_INVOKABLE QString currentVersion() const { return current_version_; }
    
    // 获取发行说明
    Q_INVOKABLE QString releaseNotes() const { return release_notes_; }
    
    // 获取检查结果
    bool HasNewVersion() const { return has_new_version_; }
    QString GetNewVersion() const { return new_version_; }
    QString GetReleaseNotes() const { return release_notes_; }
    QString GetDownloadUrl() const { return download_url_; }

signals:
    // 检查完成信号
    void UpdateCheckCompleted(bool has_update);
    // 检查失败信号  
    void UpdateCheckFailed(const QString& error);
    // 新版本发现信号
    void newVersionFound(const QString& version, const QString& notes, const QString& download_url, const QString& current_version);

private slots:
    void OnVersionReply();
    void OnNetworkError(QNetworkReply::NetworkError error);


private:
    static std::unique_ptr<UpdateChecker> instance_; 
    void ParseVersionInfo(const QJsonObject& json);
    QString ConvertMarkdownToHtml(const QString& markdown);
    
    QNetworkAccessManager* network_manager_;
    QNetworkReply* current_reply_;
    
    // 版本信息
    QString current_version_;
    bool has_new_version_;
    QString new_version_;
    QString release_notes_;
    QString download_url_;
    
    // 常量
    static const QString kCurrentVersion;
    static const QString kVersionCheckUrl;
};

#endif // UPDATE_CHECKER_H_ 