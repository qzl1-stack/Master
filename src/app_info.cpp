#include "app_info.h"
#include <QSysInfo>
#include <QVersionNumber>

AppInfo::AppInfo(QObject *parent)
    : QObject(parent)
{
}

QString AppInfo::GetVersion() const
{
    return QString::fromLatin1(kAppVersion);
}

QString AppInfo::GetGitCommit() const
{
    return QString::fromLatin1(kGitCommit);
}

QString AppInfo::GetGitDate() const
{
    return QString::fromLatin1(kGitDate);
}

QString AppInfo::GetOsInfo() const
{
    QString osInfo = QString("OS: %1 %2")
        .arg(QSysInfo::prettyProductName())
        .arg(QSysInfo::currentCpuArchitecture());
    
    return osInfo;
}

QString AppInfo::GetBuildInfo() const
{
    QString buildInfo = QString("Build with Qt %1 ")
        .arg(qVersion());
    
    return buildInfo;
}
