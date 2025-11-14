#include "ILogStorage.h"
#include <QDebug>
#include <algorithm>
#include <QJsonArray> // Add this include
#include <map>

// ================== LogAggregator 聚合器模式实现 ==================

LogAggregator::LogAggregator(QObject* parent) 
    : QObject(parent) {
    qDebug() << "LogAggregator created";
}

bool LogAggregator::registerStorage(const QString& process_id, 
                                   LogStorageType storage_type, 
                                   const QJsonObject& config) {
    if (process_id.isEmpty()) {
        qWarning() << "Cannot register storage with empty process_id";
        return false;
    }
    
    // 检查是否已存在，如果存在则先注销
    if (m_storage_instances.find(process_id) != m_storage_instances.end()) {
        qWarning() << "Process" << process_id << "already registered, unregistering first";
        unregisterStorage(process_id);
    }
    
    // 通过工厂创建存储实例
    auto storage = LogStorageFactory::createLogStorage(storage_type, config);
    if (!storage) {
        qWarning() << "Failed to create storage for process:" << process_id;
        emit storageRegistered(process_id, LogStorageFactory::getStorageTypeString(storage_type), false);
        return false;
    }
    
    // 初始化存储实例
    if (!storage->initialize(config)) {
        qWarning() << "Failed to initialize storage for process:" << process_id;
        emit storageRegistered(process_id, LogStorageFactory::getStorageTypeString(storage_type), false);
        return false;
    }
    
    // 连接信号
    connectStorageSignals(process_id, storage.get());
    
    // 保存类型和配置信息
    QString type_string = LogStorageFactory::getStorageTypeString(storage_type);
    m_storage_types[process_id] = storage_type;
    m_storage_configs[process_id] = config;
    m_storage_type_strings[process_id] = type_string;
    
    m_storage_instances.emplace(process_id, std::move(storage));
    
    qDebug() << "Registered storage for process:" << process_id;
    emit storageRegistered(process_id, type_string, true);
    
    // 更新聚合器状态
    emit aggregatorStatusChanged(m_storage_instances.size(), checkAllStorageHealth().size());
    
    return true;
}

bool LogAggregator::registerStorage(const QString& process_id, 
                                   std::unique_ptr<ILogStorage> storage) {
    if (!storage) {
        qWarning() << "Cannot register null storage for process:" << process_id;
        return false;
    }
    
    // 检查是否已存在，如果存在则先注销
    if (m_storage_instances.find(process_id) != m_storage_instances.end()) {
        qWarning() << "Process" << process_id << "already registered, unregistering first";
        unregisterStorage(process_id);
    }

    // 连接信号
    connectStorageSignals(process_id, storage.get());
    
    // 保存类型和配置信息（使用默认值）
    m_storage_types[process_id] = LogStorageType::kFileStorage;
    m_storage_configs[process_id] = QJsonObject();
    m_storage_type_strings[process_id] = "custom";
    
    m_storage_instances.emplace(process_id, std::move(storage));
    
    qDebug() << "Registered storage for process:" << process_id;
    emit storageRegistered(process_id, "custom", true);
    
    // 更新聚合器状态
    emit aggregatorStatusChanged(m_storage_instances.size(), checkAllStorageHealth().size());
    
    return true;
}

bool LogAggregator::unregisterStorage(const QString& process_id) {
    auto it = m_storage_instances.find(process_id);
    if (it == m_storage_instances.end()) {
        qWarning() << "Process not registered:" << process_id;
        return false;
    }
    
    // 断开信号连接
    disconnectStorageSignals(it->second.get());
    
    // 停止存储服务
    it->second->stop();
    
    // 移除存储相关的所有信息
    m_storage_instances.erase(process_id);
    m_storage_types.remove(process_id);
    m_storage_configs.remove(process_id);
    m_storage_type_strings.remove(process_id);
    
    qDebug() << "Unregistered storage for process:" << process_id;
    emit storageUnregistered(process_id);
    
    return true;
}

QStringList LogAggregator::getRegisteredProcesses() const {
    QStringList processes;
    for (const auto& pair : m_storage_instances) {
        processes.append(pair.first);
    }
    return processes;
}

QString LogAggregator::getStorageType(const QString& process_id) const {
    return m_storage_type_strings.value(process_id);
}

bool LogAggregator::isProcessRegistered(const QString& process_id) const {
    return m_storage_instances.find(process_id) != m_storage_instances.end();
}

bool LogAggregator::writeLog(const LogEntry& entry) {
    const QString& process_id = entry.source_process;
    if (process_id.isEmpty()) {
        qWarning() << "Cannot route log with empty source_process";
        return false;
    }
    
    return writeLogToProcess(process_id, entry);
}

int LogAggregator::writeLogs(const QList<LogEntry>& entries) {
    int success_count = 0;
    
    // 按进程ID分组
    QMap<QString, QList<LogEntry>> process_logs;
    for (const auto& entry : entries) {
        if (!entry.source_process.isEmpty()) {
            process_logs[entry.source_process].append(entry);
        }
    }
    
    // 批量写入每个进程的日志
    for (auto it = process_logs.begin(); it != process_logs.end(); ++it) {
        const QString& process_id = it.key();
        const auto& logs = it.value();
        
        auto storage_it = m_storage_instances.find(process_id);
        if (storage_it != m_storage_instances.end()) {
            success_count += storage_it->second->writeLogs(logs);
        } else {
            qWarning() << "No storage registered for process:" << process_id;
        }
    }
    
    emit batchLogWrittenAggregated(entries.size(), success_count);
    return success_count;
}

bool LogAggregator::writeLogAsync(const LogEntry& entry) {
    const QString& process_id = entry.source_process;
    if (process_id.isEmpty()) {
        qWarning() << "Cannot route async log with empty source_process";
        return false;
    }
    
    auto it = m_storage_instances.find(process_id);
    if (it != m_storage_instances.end()) {
        return it->second->writeLogAsync(entry);
    }
    
    qWarning() << "No storage registered for process:" << process_id;
    return false;
}

bool LogAggregator::writeLogToProcess(const QString& process_id, const LogEntry& entry) {
    auto it = m_storage_instances.find(process_id);
    if (it != m_storage_instances.end()) {
        return it->second->writeLog(entry);
    }
    
    qWarning() << "No storage registered for process:" << process_id;
    return false;
}

QList<LogEntry> LogAggregator::queryAllLogs(const LogQueryCondition& condition) {
    QList<LogEntry> all_logs;
    
    for (const auto& pair : m_storage_instances) {
        const auto& storage = pair.second;
        auto logs = storage->queryLogs(condition);
        all_logs.append(logs);
    }
    
    // 按时间戳排序
    std::sort(all_logs.begin(), all_logs.end(), 
              [](const LogEntry& a, const LogEntry& b) {
                  return a.timestamp < b.timestamp;
              });
    
    return all_logs;
}

QList<LogEntry> LogAggregator::queryProcessLogs(const QString& process_id, 
                                               const LogQueryCondition& condition) {
    auto it = m_storage_instances.find(process_id);
    if (it == m_storage_instances.cend()) {
        qWarning() << "No storage registered for process:" << process_id;
        return QList<LogEntry>();
    }
    
    return it->second->queryLogs(condition);
}

QList<LogEntry> LogAggregator::getAllLatestLogs(int count, 
                                               const QList<LogLevel>& level_filter) {
    QList<LogEntry> all_logs;
    
    for (const auto& pair : m_storage_instances) {
        const auto& storage = pair.second;
        auto logs = storage->getLatestLogs(count, level_filter);
        all_logs.append(logs);
    }
    
    // 按时间戳降序排序，取最新的count条
    std::sort(all_logs.begin(), all_logs.end(), 
              [](const LogEntry& a, const LogEntry& b) {
                  return a.timestamp > b.timestamp;
              });
    
    if (all_logs.size() > count) {
        all_logs = all_logs.mid(0, count);
    }
    
    return all_logs;
}

QList<LogEntry> LogAggregator::getProcessLatestLogs(const QString& process_id, 
                                                   int count,
                                                   const QList<LogLevel>& level_filter) {
    auto it = m_storage_instances.find(process_id);
    if (it == m_storage_instances.cend()) {
        return QList<LogEntry>();
    }
    
    return it->second->getLatestLogs(count, level_filter);
}

int LogAggregator::startAllStorages() {
    int success_count = 0;
    
    for (auto& pair : m_storage_instances) {
        auto& storage = pair.second;
        if (storage->start()) {
            qDebug() << "[LogAggregator] Storage started successfully for process:" << pair.first; // 添加调试信息
            success_count++;
        } else {
            qWarning() << "[LogAggregator] Failed to start storage for process:" << pair.first; // 添加调试信息
        }
    }
    
    qDebug() << "Started" << success_count << "out of" << m_storage_instances.size() << "storages";
    return success_count;
}

void LogAggregator::stopAllStorages() {
    for (auto& pair : m_storage_instances) {
        auto& storage = pair.second;
        storage->stop();
    }
    qDebug() << "Stopped all" << m_storage_instances.size() << "storages";
}

int LogAggregator::cleanupAllOldLogs(int days_to_keep) {
    int total_cleaned = 0;
    
    for (auto& pair : m_storage_instances) {
        auto& storage = pair.second;
        total_cleaned += storage->cleanupOldLogs(days_to_keep);
    }
    
    qDebug() << "Cleaned up" << total_cleaned << "old logs from all storages";
    return total_cleaned;
}

int LogAggregator::archiveAllLogs(const QDateTime& start_time, 
                                 const QDateTime& end_time, 
                                 const QString& archive_base_path) {
    int success_count = 0;
    
    // 使用显式迭代器进行键值对访问
    for (auto it = m_storage_instances.begin(); it != m_storage_instances.end(); ++it) {
        const QString& process_id = it->first;
        auto& storage = it->second;
        
        QString archive_path = QString("%1/%2_%3_%4.archive")
                              .arg(archive_base_path)
                              .arg(process_id)
                              .arg(start_time.toString("yyyyMMdd"))
                              .arg(end_time.toString("yyyyMMdd"));
        
        if (storage->archiveLogs(start_time, end_time, archive_path)) {
            success_count++;
        }
    }
    
    qDebug() << "Archived logs for" << success_count << "storages";
    return success_count;
}

LogStatistics LogAggregator::getAggregatedStatistics(const LogQueryCondition& condition) {
    LogStatistics aggregated;
    aggregated.total_count = 0;
    aggregated.total_size_bytes = 0;
    
    bool first = true;
    
    for (const auto& pair : m_storage_instances) {
        const auto& storage = pair.second;
        auto stats = storage->getStatistics(condition);
        
        // 聚合数值
        aggregated.total_count += stats.total_count;
        aggregated.total_size_bytes += stats.total_size_bytes;
        
        // 合并级别统计
        for (auto it = stats.level_counts.begin(); it != stats.level_counts.end(); ++it) {
            aggregated.level_counts[it.key()] += it.value();
        }
        
        // 合并分类统计
        for (auto it = stats.category_counts.begin(); it != stats.category_counts.end(); ++it) {
            aggregated.category_counts[it.key()] += it.value();
        }
        
        // 合并进程统计
        for (auto it = stats.process_counts.begin(); it != stats.process_counts.end(); ++it) {
            aggregated.process_counts[it.key()] += it.value();
        }
        
        // 时间范围
        if (first || stats.earliest_time.isValid() && stats.earliest_time < aggregated.earliest_time) {
            aggregated.earliest_time = stats.earliest_time;
        }
        if (first || stats.latest_time.isValid() && stats.latest_time > aggregated.latest_time) {
            aggregated.latest_time = stats.latest_time;
        }
        
        first = false;
    }
    
    return aggregated;
}

LogStatistics LogAggregator::getProcessStatistics(const QString& process_id, 
                                                 const LogQueryCondition& condition) {
    auto it = m_storage_instances.find(process_id);
    if (it == m_storage_instances.cend()) {
        return LogStatistics();
    }
    
    return it->second->getStatistics(condition);
}

QMap<QString, bool> LogAggregator::checkAllStorageHealth() {
    QMap<QString, bool> health_map;
    
    for (auto it = m_storage_instances.begin(); it != m_storage_instances.end(); ++it) { // 使用迭代器进行键值对访问
        health_map[it->first] = it->second->isHealthy();
    }
    
    return health_map;
}

QMap<QString, QJsonObject> LogAggregator::getAllStorageInfo() {
    QMap<QString, QJsonObject> info_map;
    
    for (auto it = m_storage_instances.begin(); it != m_storage_instances.end(); ++it) { // 使用迭代器进行键值对访问
        info_map[it->first] = it->second->getStorageInfo();
    }
    
    return info_map;
}

int LogAggregator::flushAllStorages() {
    int success_count = 0;
    
    for (auto& pair : m_storage_instances) {
        auto& storage = pair.second;
        if (storage->flush()) {
            success_count++;
        }
    }
    
    return success_count;
}

int LogAggregator::createIndexForAllStorages(const QString& field_name) {
    int success_count = 0;
    
    for (auto& pair : m_storage_instances) {
        auto& storage = pair.second;
        if (storage->createIndex(field_name)) {
            success_count++;
        }
    }
    
    return success_count;
}

QJsonObject LogAggregator::getAggregatorStatus() const {
    QJsonObject status;
    status["total_storages"] = static_cast<int>(m_storage_instances.size());

    int healthy_count = 0;
    QJsonArray processes; 
    for (auto it = m_storage_instances.cbegin(); it != m_storage_instances.cend(); ++it) {
        QJsonObject process_info;
        process_info["process_id"] = it->first;
        process_info["storage_type"] = m_storage_type_strings.value(it->first);
        process_info["is_healthy"] = it->second->isHealthy();
        
        if (process_info["is_healthy"].toBool()) {
            healthy_count++;
        }
        
        processes.append(process_info);
    }
    
    status["healthy_storages"] = healthy_count;
    status["processes"] = processes;
    
    return status;
}

void LogAggregator::connectStorageSignals(const QString& process_id, ILogStorage* storage) {
    if (!storage) return;
    
    // 连接日志写入信号
    connect(storage, &ILogStorage::logWritten,
            this, &LogAggregator::onStorageLogWritten);
    
    // 连接存储状态变化信号
    connect(storage, &ILogStorage::storageStateChanged,
            this, &LogAggregator::onStorageStateChanged);
    
    // 可以连接更多信号...
}

void LogAggregator::disconnectStorageSignals(ILogStorage* storage) {
    if (!storage) return;
    
    disconnect(storage, nullptr, this, nullptr);
}

void LogAggregator::onStorageLogWritten(const LogEntry& entry, bool success) {
    // 找到发送信号的存储对应的进程ID
    ILogStorage* sender_storage = qobject_cast<ILogStorage*>(sender());
    if (!sender_storage) return;
    
    QString process_id;
    for (auto it = m_storage_instances.cbegin(); it != m_storage_instances.cend(); ++it) {
        if (it->second.get() == sender_storage) {
            process_id = it->first;
            break;
        }
    }
    
    if (!process_id.isEmpty()) {
        emit logWrittenToProcess(process_id, entry, success);
    }
}

void LogAggregator::onStorageStateChanged(bool is_healthy, const QString& error_message) {
    // 找到发送信号的存储对应的进程ID
    ILogStorage* sender_storage = qobject_cast<ILogStorage*>(sender());
    if (!sender_storage) return;
    
    QString process_id;
    for (auto it = m_storage_instances.cbegin(); it != m_storage_instances.cend(); ++it) {
        if (it->second.get() == sender_storage) {
            process_id = it->first;
            break;
        }
    }
    
    if (!process_id.isEmpty()) {
        emit storageHealthChanged(process_id, is_healthy, error_message);
        
        // 更新聚合器状态
        auto health_map = checkAllStorageHealth();
        int healthy_count = 0;
        for (auto it = health_map.constBegin(); it != health_map.constEnd(); ++it) {
            if (it.value()) healthy_count++;
        }
        emit aggregatorStatusChanged(m_storage_instances.size(), healthy_count);
    }
}

// ================== LogStorageFactory 增强工厂模式实现 ==================

// 静态成员变量定义
QMap<LogStorageType, LogStorageFactory::StorageCreator> LogStorageFactory::s_creators;

std::unique_ptr<ILogStorage> LogStorageFactory::createLogStorage(
    LogStorageType type, const QJsonObject& config) {
    
    // 检查是否已注册该类型的创建器
    if (s_creators.contains(type)) {
        try {
            auto storage = s_creators[type](config);
            if (storage) {
                qDebug() << "Successfully created log storage:" << getStorageTypeString(type);
                return storage;
            }
        } catch (const std::exception& e) {
            qWarning() << "Exception while creating log storage:" << e.what();
        }
    }
    
    // 如果没有注册的创建器，记录警告
    qWarning() << "No registered creator for storage type:" << getStorageTypeString(type);
    qWarning() << "Available types:" << getRegisteredTypes().size();
    
    return nullptr;
}

bool LogStorageFactory::registerStorageType(LogStorageType type, StorageCreator creator) {
    if (!creator) {
        qWarning() << "Cannot register null creator for type:" << getStorageTypeString(type);
        return false;
    }
    
    s_creators[type] = creator;
    qDebug() << "Registered log storage type:" << getStorageTypeString(type);
    return true;
}

bool LogStorageFactory::isTypeRegistered(LogStorageType type) {
    return s_creators.contains(type);
}

QList<LogStorageType> LogStorageFactory::getRegisteredTypes() {
    return s_creators.keys();
}

LogStorageType LogStorageFactory::getStorageTypeFromString(const QString& type_str) {
    if (type_str == "file") return LogStorageType::kFileStorage;
    if (type_str == "sqlite") return LogStorageType::kSQLiteStorage;
    if (type_str == "postgresql") return LogStorageType::kPostgreSQLStorage;
    if (type_str == "elasticsearch") return LogStorageType::kElasticSearch;
    if (type_str == "memory") return LogStorageType::kMemoryStorage;
    return LogStorageType::kFileStorage; // 默认值
}

QString LogStorageFactory::getStorageTypeString(LogStorageType type) {
    switch (type) {
        case LogStorageType::kFileStorage: return "file";
        case LogStorageType::kSQLiteStorage: return "sqlite";
        case LogStorageType::kPostgreSQLStorage: return "postgresql";
        case LogStorageType::kElasticSearch: return "elasticsearch";
        case LogStorageType::kMemoryStorage: return "memory";
        default: return "file";
    }
}
