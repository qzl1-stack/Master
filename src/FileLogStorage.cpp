#include "FileLogStorage.h"
#include <QDebug>
#include <QJsonDocument>

// 匿名命名空间用于自动注册 FileLogStorage
namespace {
    struct FileLogStorageRegistrar {
        FileLogStorageRegistrar() {
            LogStorageFactory::registerStorageType(
                LogStorageType::kFileStorage,
                [](const QJsonObject& config) -> std::unique_ptr<ILogStorage> {
                    return std::make_unique<FileLogStorage>(config);
                }
            );
            qDebug() << "[FileLogStorage] FileLogStorage 类型已注册到工厂";
        }
    };
    // 静态实例，确保在程序启动时执行注册
    static FileLogStorageRegistrar registrar;
}

FileLogStorage::FileLogStorage(const QJsonObject& config, QObject* parent)
    : ILogStorage(parent)
    , m_base_dir_(config["base_dir"].toString(""))
    , m_max_file_size_bytes_(config["max_file_size_bytes"].toInt(10 * 1024 * 1024)) // 默认10MB
    , m_max_days_to_keep_(config["max_days_to_keep"].toInt(30)) // 默认保留30天
    , m_initialized_(false)
    , m_running_(false)
    , m_last_error_("") // 初始化m_last_error_
{
    qDebug() << "[FileLogStorage] 构造函数调用";
    if (m_base_dir_.isEmpty()) {
        // 默认日志目录: 应用程序数据目录下的logs子目录
        m_base_dir_ = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
    }
}

FileLogStorage::~FileLogStorage() {
    qDebug() << "[FileLogStorage] 析构函数调用";
    stop();
}

bool FileLogStorage::initialize(const QJsonObject& config) {
    QMutexLocker locker(&m_mutex_);
    if (m_initialized_) {
        qWarning() << "[FileLogStorage] 已经初始化";
        return true;
    }

    QDir dir(m_base_dir_);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qCritical() << "[FileLogStorage] 无法创建日志目录:" << m_base_dir_;
            return false;
        }
    }

    m_initialized_ = true;
    qDebug() << "[FileLogStorage] 初始化完成，日志目录:" << m_base_dir_;
    return true;
}

bool FileLogStorage::start() {
    bool success = false;
    QString status_message;
    {
        QMutexLocker locker(&m_mutex_);
        if (!m_initialized_) {
            qWarning() << "[FileLogStorage] 未初始化，无法启动";
            status_message = "未初始化，无法启动";
        } else if (m_running_) {
            qWarning() << "[FileLogStorage] 已经启动";
            success = true;
            status_message = "已经启动";
        } else {
            // 首次启动时创建或打开日志文件
            rotateLogFile();
            if (!m_log_file_.isOpen()) {
                qCritical() << "[FileLogStorage] 无法打开日志文件:" << m_current_file_path_;
                status_message = "无法打开日志文件";
            } else {
                m_running_ = true;
                success = true;
                status_message = "成功打开日志文件";
            }
        }
    }

    // 在互斥锁释放后发出信号
    emit storageStateChanged(success, status_message);
    return success;
}

void FileLogStorage::stop() {
  {  
    QMutexLocker locker(&m_mutex_);
    if (!m_running_) {
        return;
    }

    if (m_log_file_.isOpen()) {
        m_text_stream_.flush();
        m_log_file_.close();
    }
    m_running_ = false;
    qDebug() << "[FileLogStorage] 停止";
  }
    emit storageStateChanged(false, "已停止");
}

bool FileLogStorage::writeLog(const LogEntry& entry) {
    QMutexLocker locker(&m_mutex_);
    if (!m_running_ || !m_log_file_.isOpen()) {
        qWarning() << "[FileLogStorage] 未运行或文件未打开，无法写入日志";
        return false;
    }

    // 检查是否需要轮转文件
    if (m_log_file_.size() >= m_max_file_size_bytes_ ||
        getLogFileName(QDateTime::currentDateTime()) != m_log_file_.fileName().section('/', -1)) {
        rotateLogFile();
        if (!m_log_file_.isOpen()) {
            qCritical() << "[FileLogStorage] 轮转后无法打开日志文件，写入失败";
            return false;
        }
    }

    // 格式化日志条目并写入
    QString log_string = QString("[%1][%2][%3][%4:%5][%6:%7] %8 %9\n")
                            .arg(entry.timestamp.toString(Qt::ISODate))
                            .arg(logLevelToString(entry.level))
                            .arg(entry.source_process)
                            .arg(entry.module_name)
                            .arg(entry.function_name)
                            .arg(entry.thread_id)
                            .arg(entry.session_id)
                            .arg(entry.message)
                            .arg(QJsonDocument(entry.context).toJson(QJsonDocument::Compact));
    
    m_text_stream_ << log_string;
    m_text_stream_.flush();

    emit logWritten(entry, true);
    return true;
}

int FileLogStorage::writeLogs(const QList<LogEntry>& entries) {
    QMutexLocker locker(&m_mutex_);
    if (!m_running_ || !m_log_file_.isOpen()) {
        qWarning() << "[FileLogStorage] 未运行或文件未打开，无法批量写入日志";
        return 0;
    }
    
    int success_count = 0;
    for (const auto& entry : entries) {
        // 检查是否需要轮转文件 (这里简化，实际可能需要更复杂的批量处理)
        if (m_log_file_.size() >= m_max_file_size_bytes_ ||
            getLogFileName(QDateTime::currentDateTime()) != m_log_file_.fileName().section('/', -1)) {
            rotateLogFile();
            if (!m_log_file_.isOpen()) {
                qCritical() << "[FileLogStorage] 轮转后无法打开日志文件，批量写入中断";
                break;
            }
        }
        
        QString log_string = QString("[%1][%2][%3][%4:%5][%6:%7] %8 %9\n")
                                .arg(entry.timestamp.toString(Qt::ISODate))
                                .arg(logLevelToString(entry.level))
                                .arg(entry.source_process)
                                .arg(entry.module_name)
                                .arg(entry.function_name)
                                .arg(entry.thread_id)
                                .arg(entry.session_id)
                                .arg(entry.message)
                                .arg(QJsonDocument(entry.context).toJson(QJsonDocument::Compact));
        
        m_text_stream_ << log_string;
        success_count++;
        emit logWritten(entry, true);
    }
    m_text_stream_.flush();
    return success_count;
}

bool FileLogStorage::writeLogAsync(const LogEntry& entry) {
    // 文件存储本身是同步的，这里可以考虑使用QThreadPool或QtConcurrent来异步写入
    // 为简化起见，目前直接调用同步写入
    return writeLog(entry);
}

QList<LogEntry> FileLogStorage::queryLogs(const LogQueryCondition& condition) {
    QMutexLocker locker(&m_mutex_);
    QList<LogEntry> results;
    QDir dir(m_base_dir_);
    QStringList files = dir.entryList(QDir::Files, QDir::Name);

    for (const QString& file_name : files) {
        QString file_path = dir.filePath(file_name);
        QFile file(file_path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine();
                // 简单的解析日志行，实际可能需要更复杂的正则表达式
                // 格式: [timestamp][level][process_id][module:function][thread:session] message {context}
                if (line.startsWith("[") && line.contains("][")) {
                    LogEntry entry;
                    entry.timestamp = QDateTime::fromString(line.section(']', 0, 0).mid(1), Qt::ISODate);
                    entry.level = stringToLogLevel(line.section(']', 1, 1).section('[', 1, 1));
                    entry.source_process = line.section(']', 2, 2).section('[', 1, 1);
                    entry.message = line.section(']', 6, 6).section(']', 1).trimmed(); // 简化处理，获取消息部分

                    // 过滤条件
                    bool match_level = condition.levels.isEmpty() || condition.levels.contains(entry.level);
                    bool match_process = condition.process_ids.isEmpty() || condition.process_ids.contains(entry.source_process);
                    bool match_time = (!condition.start_time.isValid() || entry.timestamp >= condition.start_time) &&
                                      (!condition.end_time.isValid() || entry.timestamp <= condition.end_time);
                    bool match_keyword = condition.keyword.isEmpty() || entry.message.contains(condition.keyword, Qt::CaseInsensitive);

                    if (match_level && match_process && match_time && match_keyword) {
                        results.append(entry);
                    }
                }
            }
            file.close();
        }
    }

    // 根据时间戳排序
    std::sort(results.begin(), results.end(), 
              [](const LogEntry& a, const LogEntry& b) { return a.timestamp < b.timestamp; });

    return results;
}

QList<LogEntry> FileLogStorage::getLatestLogs(int count, const QList<LogLevel>& level_filter) {
    QMutexLocker locker(&m_mutex_);
    QList<LogEntry> all_logs;
    QDir dir(m_base_dir_);
    QStringList files = dir.entryList(QDir::Files, QDir::Name);

    for (const QString& file_name : files) {
        QString file_path = dir.filePath(file_name);
        QFile file(file_path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine();
                if (line.startsWith("[") && line.contains("][")) {
                    LogEntry entry;
                    entry.timestamp = QDateTime::fromString(line.section(']', 0, 0).mid(1), Qt::ISODate);
                    entry.level = stringToLogLevel(line.section(']', 1, 1).section('[', 1, 1));
                    entry.source_process = line.section(']', 2, 2).section('[', 1, 1);
                    entry.message = line.section(']', 6, 6).section(']', 1).trimmed();

                    bool match_level = level_filter.isEmpty() || level_filter.contains(entry.level);
                    if (match_level) {
                        all_logs.append(entry);
                    }
                }
            }
            file.close();
        }
    }

    // 按时间戳降序排序
    std::sort(all_logs.begin(), all_logs.end(), 
              [](const LogEntry& a, const LogEntry& b) { return a.timestamp > b.timestamp; });

    // 返回最新的count条
    if (all_logs.size() > count) {
        return all_logs.mid(0, count);
    }
    return all_logs;
}

int FileLogStorage::cleanupOldLogs(int days_to_keep) {
    QMutexLocker locker(&m_mutex_);
    int cleaned_count = 0;
    QDir dir(m_base_dir_);
    QStringList files = dir.entryList(QDir::Files, QDir::Name);

    QDateTime cutoff_date = QDateTime::currentDateTime().addDays(-days_to_keep);

    for (const QString& file_name : files) {
        QString file_path = dir.filePath(file_name);
        QFileInfo file_info(file_path);
        if (file_info.lastModified() < cutoff_date) {
            if (QFile::remove(file_path)) {
                cleaned_count++;
                qDebug() << "[FileLogStorage] 清理旧日志文件:" << file_name;
            } else {
                qWarning() << "[FileLogStorage] 无法清理旧日志文件:" << file_name;
            }
        }
    }
    return cleaned_count;
}

bool FileLogStorage::archiveLogs(const QDateTime& start_time, 
                                 const QDateTime& end_time, 
                                 const QString& archive_path) {
    QMutexLocker locker(&m_mutex_);
    QFile archive_file(archive_path);
    if (!archive_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        qCritical() << "[FileLogStorage] 无法打开归档文件:" << archive_path;
        return false;
    }

    QTextStream out(&archive_file);
    QDir dir(m_base_dir_);
    QStringList files = dir.entryList(QDir::Files, QDir::Name);

    for (const QString& file_name : files) {
        QString file_path = dir.filePath(file_name);
        QFile log_file(file_path);
        if (log_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&log_file);
            while (!in.atEnd()) {
                QString line = in.readLine();
                // 假设日志行包含时间戳，进行简单的时间过滤
                QDateTime log_time = QDateTime::fromString(line.section(']', 0, 0).mid(1), Qt::ISODate);
                if (log_time.isValid() && log_time >= start_time && log_time <= end_time) {
                    out << line << "\n";
                }
            }
            log_file.close();
        }
    }

    archive_file.close();
    qDebug() << "[FileLogStorage] 日志已归档到:" << archive_path;
    return true;
}

LogStatistics FileLogStorage::getStatistics(const LogQueryCondition& condition) {
    QMutexLocker locker(&m_mutex_);
    LogStatistics stats;
    stats.total_count = 0;
    stats.total_size_bytes = 0;
    stats.earliest_time = QDateTime();
    stats.latest_time = QDateTime();

    QDir dir(m_base_dir_);
    QStringList files = dir.entryList(QDir::Files, QDir::Name);

    for (const QString& file_name : files) {
        QString file_path = dir.filePath(file_name);
        QFile file(file_path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            stats.total_size_bytes += file.size();
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine();
                // 简单的解析日志行，进行统计
                if (line.startsWith("[") && line.contains("][")) {
                    LogEntry entry;
                    entry.timestamp = QDateTime::fromString(line.section(']', 0, 0).mid(1), Qt::ISODate);
                    entry.level = stringToLogLevel(line.section(']', 1, 1).section('[', 1, 1));
                    entry.source_process = line.section(']', 2, 2).section('[', 1, 1);

                    // 过滤条件
                    bool match_level = condition.levels.isEmpty() || condition.levels.contains(entry.level);
                    bool match_process = condition.process_ids.isEmpty() || condition.process_ids.contains(entry.source_process);
                    bool match_time = (!condition.start_time.isValid() || entry.timestamp >= condition.start_time) &&
                                      (!condition.end_time.isValid() || entry.timestamp <= condition.end_time);
                    bool match_keyword = condition.keyword.isEmpty() || entry.message.contains(condition.keyword, Qt::CaseInsensitive);

                    if (match_level && match_process && match_time && match_keyword) {
                        stats.total_count++;
                        stats.level_counts[entry.level]++; // 直接使用 LogLevel 作为键
                        stats.process_counts[entry.source_process]++;

                        if (!stats.earliest_time.isValid() || entry.timestamp < stats.earliest_time) {
                            stats.earliest_time = entry.timestamp;
                        }
                        if (!stats.latest_time.isValid() || entry.timestamp > stats.latest_time) {
                            stats.latest_time = entry.timestamp;
                        }
                    }
                }
            }
            file.close();
        }
    }
    return stats;
}

bool FileLogStorage::isHealthy() const {
    QMutexLocker locker(&m_mutex_);
    // 检查日志目录是否存在且可写
    QDir dir(m_base_dir_);
    if (!dir.exists() || !QFileInfo(m_base_dir_).isWritable()) {
        return false;
    }
    // 如果文件是打开的，说明基本健康
    return m_running_ && m_log_file_.isOpen();
}

QJsonObject FileLogStorage::getStorageInfo() const {
    QMutexLocker locker(&m_mutex_);
    QJsonObject info;
    info["type"] = "file";
    info["base_dir"] = m_base_dir_;
    info["current_file"] = m_current_file_path_;
    info["max_file_size_bytes"] = m_max_file_size_bytes_;
    info["max_days_to_keep"] = m_max_days_to_keep_;
    info["is_initialized"] = m_initialized_;
    info["is_running"] = m_running_;
    return info;
}

bool FileLogStorage::flush() {
    QMutexLocker locker(&m_mutex_);
    if (m_log_file_.isOpen()) {
        m_text_stream_.flush();
        return true;
    }
    return false;
}

bool FileLogStorage::createIndex(const QString& field_name) {
    Q_UNUSED(field_name);
    // 文件存储不支持索引概念，但为了接口完整性返回true
    qDebug() << "[FileLogStorage] 文件存储不支持创建索引:" << field_name;
    return true;
}

QList<LogEntry> FileLogStorage::getProcessLogs(const QString& process_id, int count) {
    QMutexLocker locker(&m_mutex_);
    QList<LogEntry> results;
    QDir dir(m_base_dir_);
    QStringList files = dir.entryList(QDir::Files, QDir::Name);

    for (const QString& file_name : files) {
        QString file_path = dir.filePath(file_name);
        QFile file(file_path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while (!in.atEnd() && results.size() < count) { // 限制读取数量
                QString line = in.readLine();
                if (line.startsWith("[") && line.contains("][")) {
                    LogEntry entry;
                    entry.timestamp = QDateTime::fromString(line.section(']', 0, 0).mid(1), Qt::ISODate);
                    entry.level = stringToLogLevel(line.section(']', 1, 1).section('[', 1, 1));
                    entry.source_process = line.section(']', 2, 2).section('[', 1, 1);
                    entry.message = line.section(']', 6, 6).section(']', 1).trimmed();

                    if (entry.source_process == process_id) {
                        results.append(entry);
                    }
                }
            }
            file.close();
        }
    }

    std::sort(results.begin(), results.end(), 
              [](const LogEntry& a, const LogEntry& b) { return a.timestamp > b.timestamp; });

    if (results.size() > count) {
        return results.mid(0, count);
    }
    return results;
}

QString FileLogStorage::getLastError() const {
    QMutexLocker locker(&m_mutex_);
    return m_last_error_;
}

void FileLogStorage::rotateLogFile() {
    if (m_log_file_.isOpen()) {
        m_text_stream_.flush();
        m_log_file_.close();
    }

    // 获取新的日志文件路径 (按日期)
    QString new_file_name = getLogFileName(QDateTime::currentDateTime());
    m_current_file_path_ = QDir(m_base_dir_).filePath(new_file_name);
    
    m_log_file_.setFileName(m_current_file_path_);
    if (!m_log_file_.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qCritical() << "[FileLogStorage] 无法打开新的日志文件进行轮转:" << m_current_file_path_;
        m_last_error_ = "无法打开日志文件进行轮转: " + m_current_file_path_;
        // 如果打开失败，则尝试使用一个备用文件或直接返回错误状态
        m_current_file_path_.clear();
        emit storageStateChanged(false, "无法打开日志文件进行轮转");
        return;
    }
    m_text_stream_.setDevice(&m_log_file_);
    qDebug() << "[FileLogStorage] 日志文件轮转到:" << m_current_file_path_;
}

QString FileLogStorage::getLogFilePath(const QDateTime& datetime) const {
    return QDir(m_base_dir_).filePath(getLogFileName(datetime));
}

QString FileLogStorage::getLogFileName(const QDateTime& datetime) const {
    return datetime.toString("yyyy-MM-dd") + ".log";
}

LogLevel FileLogStorage::stringToLogLevel(const QString& level_str) const {
    if (level_str == "TRACE") return LogLevel::kTrace;
    if (level_str == "DEBUG") return LogLevel::kDebug;
    if (level_str == "INFO") return LogLevel::kInfo;
    if (level_str == "WARNING") return LogLevel::kWarning;
    if (level_str == "ERROR") return LogLevel::kError;
    if (level_str == "FATAL") return LogLevel::kFatal;
    return LogLevel::kInfo; // 默认值
}

QString FileLogStorage::logLevelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::kTrace: return "TRACE";
        case LogLevel::kDebug: return "DEBUG";
        case LogLevel::kInfo: return "INFO";
        case LogLevel::kWarning: return "WARNING";
        case LogLevel::kError: return "ERROR";
        case LogLevel::kFatal: return "FATAL";
        default: return "UNKNOWN";
    }
}
