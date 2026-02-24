#ifndef APP_INFO_H_
#define APP_INFO_H_

#include <QString>
#include <QObject>

/**
 * @brief AppInfo 应用程序信息管理类
 * 
 * 负责管理应用程序的版本、Git信息、系统信息等
 * 通过Q_PROPERTY向QML暴露这些信息
 */
class AppInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString version READ GetVersion CONSTANT)
    Q_PROPERTY(QString gitCommit READ GetGitCommit CONSTANT)
    Q_PROPERTY(QString gitDate READ GetGitDate CONSTANT)
    Q_PROPERTY(QString osInfo READ GetOsInfo CONSTANT)
    Q_PROPERTY(QString buildInfo READ GetBuildInfo CONSTANT)

public:
    explicit AppInfo(QObject *parent = nullptr);
    ~AppInfo() = default;

    // 获取应用程序版本
    QString GetVersion() const;

    // 获取Git commit哈希值
    QString GetGitCommit() const;

    // 获取Git commit日期
    QString GetGitDate() const;

    // 获取操作系统信息
    QString GetOsInfo() const;

    // 获取构建信息
    QString GetBuildInfo() const;

private:
    // 版本号常量
    static constexpr const char* kAppVersion = "1.0.8";
    static constexpr const char* kGitCommit = "932f235fecf24b9021dad1f47bd9e35f269ab12e";
    static constexpr const char* kGitDate = "2026-02-08T07:42:24.999Z";
};

#endif // APP_INFO_H_
