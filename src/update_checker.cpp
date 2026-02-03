#include "update_checker.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QDebug>
#include <QRegularExpression>

const QString UpdateChecker::kCurrentVersion = "1.0.8"; // 与 main.cpp 中的版本一致
const QString UpdateChecker::kVersionCheckUrl = "https://jts-tools-master.oss-cn-shanghai.aliyuncs.com/version.json";

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent)
    , network_manager_(nullptr)
    , current_reply_(nullptr)
    , current_version_(kCurrentVersion)
    , has_new_version_(false)
{
    network_manager_ = new QNetworkAccessManager(this);
}

UpdateChecker::~UpdateChecker()
{
    if (current_reply_) {
        current_reply_->abort();
        current_reply_->deleteLater();
    }
}


// 启动自动更新检查
void UpdateChecker::startAutoUpdateCheck()
{
    qDebug() << "UpdateChecker: 开始自动检查更新，当前版本:" << current_version_;
    
    // 重置状态
    has_new_version_ = false;
    new_version_.clear();
    release_notes_.clear();
    download_url_.clear();
    
    // 如果有正在进行的请求，先取消
    if (current_reply_) {
        current_reply_->abort();
        current_reply_->deleteLater();
        current_reply_ = nullptr;
    }
    
    QNetworkRequest request;
    request.setUrl(QUrl(kVersionCheckUrl));
    request.setRawHeader("User-Agent", "VTA-AutoUpdateChecker/1.0");
    
    // 设置超时时间为10秒
    request.setTransferTimeout(10000);
    
    current_reply_ = network_manager_->get(request);
    
    // 连接信号
    connect(current_reply_, &QNetworkReply::finished, this, &UpdateChecker::OnVersionReply);
    connect(current_reply_, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &UpdateChecker::OnNetworkError);
}

void UpdateChecker::OnVersionReply()
{
    if (!current_reply_) {
        return;
    }
    
    QNetworkReply* reply = current_reply_;
    current_reply_ = nullptr;
    
    if (reply->error() != QNetworkReply::NoError) {
        QString error_msg = tr("网络请求失败: %1").arg(reply->errorString());
        qWarning() << "UpdateChecker:" << error_msg;
        emit updateCheckFailed(error_msg);
        reply->deleteLater();
        return;
    }
    
    QByteArray response_data = reply->readAll();
    reply->deleteLater();
    
    
    QJsonDocument json_doc = QJsonDocument::fromJson(response_data);
    if (json_doc.isNull() || !json_doc.isObject()) {
        QString error_msg = tr("服务器返回的数据格式不正确");
        qWarning() << "UpdateChecker:" << error_msg;
        emit updateCheckFailed(error_msg);
        return;
    }
    
    ParseVersionInfo(json_doc.object());
}

void UpdateChecker::OnNetworkError(QNetworkReply::NetworkError error)
{
    QString error_msg = tr("网络错误: %1").arg(static_cast<int>(error));
    qWarning() << "UpdateChecker:" << error_msg;
    emit updateCheckFailed(error_msg);
}

void UpdateChecker::ParseVersionInfo(const QJsonObject& json)
{
    qDebug() << "UpdateChecker: 解析版本信息:" << json;
    
    // 获取版本号
    QString new_version = json["version"].toString();
    if (new_version.isEmpty()) {
        QString error_msg = tr("服务器未提供版本信息");
        qWarning() << "UpdateChecker:" << error_msg;
        emit updateCheckFailed(error_msg);
        return;
    }
    
    // 移除可能的 "v" 前缀
    if (new_version.startsWith('v')) {
        new_version = new_version.mid(1);
    }
    
    qDebug() << "UpdateChecker: 服务器版本:" << new_version << ", 当前版本:" << current_version_;
    
    // 比较版本号
    if (new_version <= current_version_) {
        qDebug() << "UpdateChecker: 当前已是最新版本";
        emit updateCheckCompleted(false);
        return;
    }
    
    // 获取下载链接
    QString download_url = json["download_url"].toString();
    if (download_url.isEmpty()) {
        QString error_msg = tr("服务器未提供下载链接");
        qWarning() << "UpdateChecker:" << error_msg;
        emit updateCheckFailed(error_msg);
        return;
    }
    download_url_ = download_url;

    // 获取版本说明
    QString release_notes = json["release_notes"].toString();
    if (release_notes.length() > 1000) {
        release_notes = release_notes.left(1000) + "...";
    }
    
    // 转换 Markdown 为 HTML
    release_notes = ConvertMarkdownToHtml(release_notes);
    
    // 保存信息
    has_new_version_ = true;
    new_version_ = new_version;
    release_notes_ = release_notes;
    current_version_ = kCurrentVersion;
    
    qDebug() << "UpdateChecker: 发现新版本" << new_version_;
    
    // 发出信号
    emit newVersionFound(new_version_, release_notes_, download_url_, current_version_);
    emit updateCheckCompleted(true);
}

QString UpdateChecker::ConvertMarkdownToHtml(const QString& markdown)
{
    QString html = markdown;
    
    // 去除多余的空行和空白
    html = html.trimmed();
    html.replace(QRegularExpression("(\\n\\s*){2,}"), "\n");
    
    // 标题转换
    html.replace(QRegularExpression("^# (.+)$", QRegularExpression::MultilineOption), "<h2>\\1</h2>");
    html.replace(QRegularExpression("^## (.+)$", QRegularExpression::MultilineOption), "<h3>\\1</h3>");
    html.replace(QRegularExpression("^### (.+)$", QRegularExpression::MultilineOption), "<h4>\\1</h4>");
    
    // 粗体转换
    html.replace(QRegularExpression("\\*\\*(.+?)\\*\\*"), "<strong>\\1</strong>");
    
    // 斜体转换
    html.replace(QRegularExpression("\\*(.+?)\\*"), "<em>\\1</em>");
    
    // 代码块转换
    html.replace(QRegularExpression("```([\\s\\S]+?)```"), "<pre><code>\\1</code></pre>");
    
    // 行内代码转换
    html.replace(QRegularExpression("`(.+?)`"), "<code>\\1</code>");
    
    // 列表转换
    html.replace(QRegularExpression("^- (.+)$", QRegularExpression::MultilineOption), "<li style='margin-bottom: 5px;'>\\1</li>");
    html.replace(QRegularExpression("(<li>.+</li>)"), "<ul style='margin-left: 20px; margin-bottom: 10px;'>\\1</ul>");
    
    return html;
}
