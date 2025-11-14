#ifndef MASTER_SRC_FILELOGSTORAGE_H_
#define MASTER_SRC_FILELOGSTORAGE_H_

#include "ILogStorage.h"
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>

/**
 * @brief 文件日志存储实现
 *
 * 该类实现了ILogStorage接口，将日志写入到本地文件系统。
 * 支持按日期分割文件、自动清理旧日志等功能。
 */
class FileLogStorage : public ILogStorage {
    Q_OBJECT

public:
    explicit FileLogStorage(const QJsonObject& config, QObject* parent = nullptr);
    ~FileLogStorage() override;

    bool initialize(const QJsonObject& config) override;
    bool start() override;
    void stop() override;
    bool writeLog(const LogEntry& entry) override;
    int writeLogs(const QList<LogEntry>& entries) override;
    bool writeLogAsync(const LogEntry& entry) override;
    QList<LogEntry> queryLogs(const LogQueryCondition& condition) override;
    QList<LogEntry> getLatestLogs(int count, const QList<LogLevel>& level_filter) override;
    int cleanupOldLogs(int days_to_keep) override;
    bool archiveLogs(const QDateTime& start_time, const QDateTime& end_time, const QString& archive_path) override;
    LogStatistics getStatistics(const LogQueryCondition& condition) override;
    bool isHealthy() const override;
    QJsonObject getStorageInfo() const override;
    bool flush() override;
    bool createIndex(const QString& field_name) override;
    QString getLastError() const override; // 添加 getLastError 声明
    QList<LogEntry> getProcessLogs(const QString& process_id, int count) override;

private:
    QString m_base_dir_;
    QString m_current_file_path_;
    int m_max_file_size_bytes_;
    int m_max_days_to_keep_;
    QFile m_log_file_;
    QTextStream m_text_stream_;
    mutable QMutex m_mutex_;
    bool m_initialized_;
    bool m_running_;
    QString m_last_error_; // 添加 m_last_error_ 成员变量

    void rotateLogFile();
    QString getLogFilePath(const QDateTime& datetime) const;
    QString getLogFileName(const QDateTime& datetime) const;
    LogLevel stringToLogLevel(const QString& level_str) const;
    QString logLevelToString(LogLevel level) const;
};

#endif // MASTER_SRC_FILELOGSTORAGE_H_
