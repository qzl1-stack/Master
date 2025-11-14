#ifndef UPDATER_H
#define UPDATER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QQmlApplicationEngine>

class Updater : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString statusText READ statusText WRITE setStatusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString titleText READ titleText WRITE setTitleText NOTIFY titleTextChanged)
    Q_PROPERTY(QString newVersion READ newVersion WRITE setNewVersion NOTIFY newVersionChanged)
    Q_PROPERTY(QString releaseNotes READ releaseNotes WRITE setReleaseNotes NOTIFY releaseNotesChanged)
    Q_PROPERTY(int downloadProgress READ downloadProgress WRITE setDownloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(bool showProgress READ showProgress WRITE setShowProgress NOTIFY showProgressChanged)
    Q_PROPERTY(bool showUpdateButton READ showUpdateButton WRITE setShowUpdateButton NOTIFY showUpdateButtonChanged)
    Q_PROPERTY(bool showReleaseNotes READ showReleaseNotes WRITE setShowReleaseNotes NOTIFY showReleaseNotesChanged)
    Q_PROPERTY(QString cancelButtonText READ cancelButtonText WRITE setCancelButtonText NOTIFY cancelButtonTextChanged)
    Q_PROPERTY(bool showCreateShortcut READ showCreateShortcut WRITE setShowCreateShortcut NOTIFY showCreateShortcutChanged)
    Q_PROPERTY(bool createShortcutChecked READ createShortcutChecked WRITE setCreateShortcutChecked NOTIFY createShortcutCheckedChanged)

public:
    explicit Updater(QObject* parent = nullptr);
    ~Updater();
    
    // Q_PROPERTY 的 Getter 方法
    QString statusText() const { return m_statusText; }
    QString titleText() const { return m_titleText; }
    QString newVersion() const { return m_newVersion; }
    QString releaseNotes() const { return m_releaseNotes; }
    int downloadProgress() const { return m_downloadProgress; }
    bool showProgress() const { return m_showProgress; }
    bool showUpdateButton() const { return m_showUpdateButton; }
    bool showReleaseNotes() const { return m_showReleaseNotes; }
    QString cancelButtonText() const { return m_cancelButtonText; }
    bool showCreateShortcut() const { return m_showCreateShortcut; }
    bool createShortcutChecked() const { return m_createShortcutChecked; }

    // Q_PROPERTY 的 Setter 方法 (在 .cpp 中实现)
    void setStatusText(const QString &text);
    void setTitleText(const QString &text);
    void setNewVersion(const QString &version);
    void setReleaseNotes(const QString &notes);
    void setDownloadProgress(int progress);
    void setShowProgress(bool show);
    void setShowUpdateButton(bool show);
    void setShowReleaseNotes(bool show);
    void setCancelButtonText(const QString &text);
    void setShowCreateShortcut(bool show);
    void setCreateShortcutChecked(bool checked);

    // Q_INVOKABLE 方法
    Q_INVOKABLE void startUpdate();
    Q_INVOKABLE void cancelUpdate();
    Q_INVOKABLE void createDesktopShortcut();
    Q_INVOKABLE void checkForUpdates();

signals:
    void statusTextChanged();
    void titleTextChanged();
    void newVersionChanged();
    void releaseNotesChanged();
    void downloadProgressChanged();
    void showProgressChanged();
    void showUpdateButtonChanged();
    void showReleaseNotesChanged();
    void cancelButtonTextChanged();
    void showCreateShortcutChanged();
    void createShortcutCheckedChanged();
    void updateCompleted();
    void updateFailed(const QString& error);
    void newVersionFound(const QString& newVersion, const QString& releaseNotes, const QString& downloadUrl, const QString& currentVersion); // 添加新版本发现信号

private slots:
    void OnVersionReply(QNetworkReply *reply);
    void OnDownloadFinished();
    void OnDownloadProgress(qint64 bytes_received, qint64 bytes_total);

private:
    void ParseVersionInfo(const QJsonObject &json);
    QString ConvertMarkdownToHtml(const QString& markdown);
    void StartDownload(const QString &url); // 恢复 StartDownload 的私有声明
    void CloseMainApp();
    void InstallUpdate();

    QNetworkAccessManager *network_manager_;
    QNetworkReply *current_reply_;
    QString download_url_;
    QString file_path_;

    QString m_statusText;
    QString m_titleText;
    QString m_newVersion;
    QString m_releaseNotes;
    int m_downloadProgress;
    bool m_showProgress;
    bool m_showUpdateButton;
    bool m_showReleaseNotes;
    QString m_cancelButtonText;
    bool m_showCreateShortcut;
    bool m_createShortcutChecked;
    bool has_new_version_; // 添加新的成员变量
    QString m_currentVersion; // 添加新的成员变量

    static const QString kCurrentVersion;
    static const QString kDownloadBaseUrl;
    static const QString kAppName;
};

#endif // UPDATER_H 