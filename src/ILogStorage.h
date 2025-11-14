#ifndef MASTER_SRC_ILOGSTORAGE_H_
#define MASTER_SRC_ILOGSTORAGE_H_

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QStringList>
#include <memory>
#include <map>

/**
 * @brief 日志级别枚举
 */
enum class LogLevel {
    kTrace = 0,     // 追踪级别，最详细的调试信息
    kDebug,         // 调试级别，开发时的调试信息
    kInfo,          // 信息级别，一般性信息
    kWarning,       // 警告级别，可能的问题
    kError,         // 错误级别，错误但不影响系统运行
    kFatal          // 致命级别，严重错误导致系统无法继续
};

/**
 * @brief 日志分类枚举
 */
enum class LogCategory {
    kSystem = 0,        // 系统日志
    kBusiness,          // 业务日志
    kPerformance,       // 性能日志
    kSecurity,          // 安全日志
    kNetwork,           // 网络日志
    kDatabase,          // 数据库日志
    kUser               // 用户操作日志
};

/**
 * @brief 日志条目结构
 */
struct LogEntry {
    QString log_id;             // 日志唯一ID
    QDateTime timestamp;        // 时间戳
    LogLevel level;             // 日志级别
    LogCategory category;       // 日志分类
    QString source_process;     // 来源进程ID
    QString module_name;        // 模块名称
    QString function_name;      // 函数名称
    int line_number;            // 行号
    QString message;            // 日志消息
    QJsonObject context;        // 上下文信息（键值对）
    QString thread_id;          // 线程ID
    QString session_id;         // 会话ID（可选）
    
    // 序列化为JSON
    QJsonObject toJson() const;
    
    // 从JSON反序列化
    static LogEntry fromJson(const QJsonObject& json);
    
    // 格式化为可读字符串
    QString toString() const;
    
    // 创建简单日志条目的便捷方法
    static LogEntry create(LogLevel level, 
                          LogCategory category,
                          const QString& source_process,
                          const QString& message,
                          const QString& module_name = QString(),
                          const QString& function_name = QString(),
                          int line_number = 0);
};

/**
 * @brief 日志查询条件结构
 */
struct LogQueryCondition {
    QDateTime start_time;           // 开始时间
    QDateTime end_time;             // 结束时间
    QList<LogLevel> levels;         // 日志级别过滤
    QList<LogCategory> categories;  // 日志分类过滤
    QList<QString> process_ids;    // 来源进程ID过滤
    QStringList source_processes;   // 来源进程过滤
    QStringList module_names;       // 模块名称过滤
    QString keyword;                // 关键词搜索
    int limit;                      // 结果数量限制
    int offset;                     // 结果偏移量
    
    LogQueryCondition();
    
    // 重置查询条件
    void reset();
    
    // 验证查询条件是否有效
    bool isValid() const;
};

/**
 * @brief 日志统计信息结构
 */
struct LogStatistics {
    int total_count;                    // 总日志数量
    QMap<LogLevel, int> level_counts;   // 各级别日志数量
    QMap<LogCategory, int> category_counts; // 各分类日志数量
    QMap<QString, int> process_counts;  // 各进程日志数量
    QDateTime earliest_time;            // 最早日志时间
    QDateTime latest_time;              // 最新日志时间
    qint64 total_size_bytes;            // 总存储大小（字节）
    
    // 转换为JSON
    QJsonObject toJson() const;
};

/**
 * @brief 日志存储抽象接口
 * 
 * 该接口定义了日志存储的标准行为，支持多种存储后端。
 * 采用策略模式，便于扩展和切换不同的存储实现。
 * 
 * 主要功能：
 * - 日志写入（单条、批量、异步）
 * - 日志查询（条件查询、分页、排序）
 * - 日志管理（清理、归档、压缩）
 * - 性能优化（缓存、索引、批量操作）
 * - 监控统计（日志统计、存储状态）
 */
class ILogStorage : public QObject {
    Q_OBJECT

public:
    explicit ILogStorage(QObject* parent = nullptr);
    virtual ~ILogStorage() = default;

    /**
     * @brief 初始化日志存储
     * @param config 存储配置参数
     * @return 是否初始化成功
     */
    virtual bool initialize(const QJsonObject& config) = 0;

    /**
     * @brief 启动日志存储服务
     * @return 是否启动成功
     */
    virtual bool start() = 0;

    /**
     * @brief 停止日志存储服务
     */
    virtual void stop() = 0;

    /**
     * @brief 写入单条日志
     * @param entry 日志条目
     * @return 是否写入成功
     */
    virtual bool writeLog(const LogEntry& entry) = 0;

    /**
     * @brief 批量写入日志
     * @param entries 日志条目列表
     * @return 成功写入的日志数量
     */
    virtual int writeLogs(const QList<LogEntry>& entries) = 0;

    /**
     * @brief 异步写入日志（非阻塞）
     * @param entry 日志条目
     * @return 是否提交成功（不保证写入成功）
     */
    virtual bool writeLogAsync(const LogEntry& entry) = 0;

    /**
     * @brief 查询日志
     * @param condition 查询条件
     * @return 匹配的日志条目列表
     */
    virtual QList<LogEntry> queryLogs(const LogQueryCondition& condition) = 0;

    /**
     * @brief 获取最新的N条日志
     * @param count 日志数量
     * @param level_filter 级别过滤（空表示所有级别）
     * @return 最新日志列表
     */
    virtual QList<LogEntry> getLatestLogs(int count, 
                                         const QList<LogLevel>& level_filter = {}) = 0;

    /**
     * @brief 获取指定进程的日志
     * @param process_id 进程ID
     * @param count 数量限制
     * @return 进程日志列表
     */
    virtual QList<LogEntry> getProcessLogs(const QString& process_id, int count = 100) = 0;

    /**
     * @brief 清理过期日志
     * @param days_to_keep 保留天数
     * @return 清理的日志数量
     */
    virtual int cleanupOldLogs(int days_to_keep) = 0;

    /**
     * @brief 归档日志
     * @param start_time 开始时间
     * @param end_time 结束时间
     * @param archive_path 归档路径
     * @return 是否归档成功
     */
    virtual bool archiveLogs(const QDateTime& start_time, 
                           const QDateTime& end_time, 
                           const QString& archive_path) = 0;

    /**
     * @brief 获取日志统计信息
     * @param condition 统计条件（可选）
     * @return 统计信息
     */
    virtual LogStatistics getStatistics(const LogQueryCondition& condition = LogQueryCondition()) = 0;

    /**
     * @brief 检查存储健康状态
     * @return 是否健康
     */
    virtual bool isHealthy() const = 0;

    /**
     * @brief 获取存储容量信息
     * @return 容量信息JSON（used_bytes, total_bytes, free_bytes等）
     */
    virtual QJsonObject getStorageInfo() const = 0;

    /**
     * @brief 刷新缓存，确保数据持久化
     * @return 是否刷新成功
     */
    virtual bool flush() = 0;

    /**
     * @brief 创建索引以提升查询性能
     * @param field_name 索引字段名
     * @return 是否创建成功
     */
    virtual bool createIndex(const QString& field_name) = 0;

    /**
     * @brief 获取最后一次错误信息
     * @return 错误信息
     */
    virtual QString getLastError() const = 0;

signals:
    /**
     * @brief 日志写入完成信号
     * @param entry 写入的日志条目
     * @param success 是否成功
     */
    void logWritten(const LogEntry& entry, bool success);

    /**
     * @brief 批量日志写入完成信号
     * @param count 写入数量
     * @param total 总数量
     */
    void batchLogWritten(int count, int total);

    /**
     * @brief 存储状态变化信号
     * @param is_healthy 是否健康
     * @param error_message 错误信息（如果有）
     */
    void storageStateChanged(bool is_healthy, const QString& error_message = QString());

    /**
     * @brief 存储容量警告信号
     * @param used_percentage 使用百分比
     * @param free_bytes 剩余字节数
     */
    void storageCapacityWarning(double used_percentage, qint64 free_bytes);

    /**
     * @brief 日志归档完成信号
     * @param archive_path 归档路径
     * @param log_count 归档日志数量
     * @param success 是否成功
     */
    void archiveCompleted(const QString& archive_path, int log_count, bool success);
};

/**
 * @brief 日志存储类型枚举
 */
enum class LogStorageType {
    kFileStorage = 0,    // 文件存储
    kSQLiteStorage,      // SQLite数据库存储
    kPostgreSQLStorage,  // PostgreSQL数据库存储
    kElasticSearch,      // ElasticSearch存储
    kMemoryStorage       // 内存存储（测试用）
};

/**
 * @brief 日志聚合器类（聚合器模式核心）
 * 
 * 该类负责统一管理多个不同类型的日志存储后端，实现多源异构日志的聚合管理。
 * 支持按进程ID、存储类型等维度进行日志路由和统一查询。
 * 
 * 主要功能：
 * - 多后端管理（注册、路由、统一操作）
 * - 日志写入路由（按进程ID自动路由到对应存储）
 * - 统一查询接口（跨所有存储后端查询）
 * - 存储状态监控（健康检查、容量监控）
 * - 批量操作支持（批量写入、清理、归档）
 */
class LogAggregator : public QObject {
    Q_OBJECT

public:
    explicit LogAggregator(QObject* parent = nullptr);
    virtual ~LogAggregator() = default;

    /**
     * @brief 注册日志存储后端
     * @param process_id 进程ID（用于路由）
     * @param storage_type 存储类型
     * @param config 存储配置
     * @return 是否注册成功
     */
    bool registerStorage(const QString& process_id, 
                        LogStorageType storage_type, 
                        const QJsonObject& config = QJsonObject());

    /**
     * @brief 注册日志存储后端（直接传入实例）
     * @param process_id 进程ID
     * @param storage 存储实例
     * @return 是否注册成功
     */
    bool registerStorage(const QString& process_id, 
                        std::unique_ptr<ILogStorage> storage);

    /**
     * @brief 取消注册日志存储后端
     * @param process_id 进程ID
     * @return 是否取消成功
     */
    bool unregisterStorage(const QString& process_id);

    /**
     * @brief 获取已注册的进程列表
     * @return 进程ID列表
     */
    QStringList getRegisteredProcesses() const;

    /**
     * @brief 获取指定进程的存储类型
     * @param process_id 进程ID
     * @return 存储类型字符串
     */
    QString getStorageType(const QString& process_id) const;

    /**
     * @brief 检查指定进程是否已注册
     * @param process_id 进程ID
     * @return 是否已注册
     */
    bool isProcessRegistered(const QString& process_id) const;

    // === 日志写入接口（自动路由） ===
    
    /**
     * @brief 写入单条日志（自动路由到对应存储）
     * @param entry 日志条目
     * @return 是否写入成功
     */
    bool writeLog(const LogEntry& entry);

    /**
     * @brief 批量写入日志（自动路由）
     * @param entries 日志条目列表
     * @return 成功写入的日志数量
     */
    int writeLogs(const QList<LogEntry>& entries);

    /**
     * @brief 异步写入日志（自动路由）
     * @param entry 日志条目
     * @return 是否提交成功
     */
    bool writeLogAsync(const LogEntry& entry);

    /**
     * @brief 写入日志到指定进程存储
     * @param process_id 目标进程ID
     * @param entry 日志条目
     * @return 是否写入成功
     */
    bool writeLogToProcess(const QString& process_id, const LogEntry& entry);

    // === 统一查询接口（跨所有存储） ===
    
    /**
     * @brief 跨所有存储查询日志
     * @param condition 查询条件
     * @return 所有匹配的日志条目列表
     */
    QList<LogEntry> queryAllLogs(const LogQueryCondition& condition);

    /**
     * @brief 从指定进程查询日志
     * @param process_id 进程ID
     * @param condition 查询条件
     * @return 匹配的日志条目列表
     */
    QList<LogEntry> queryProcessLogs(const QString& process_id, 
                                   const LogQueryCondition& condition);

    /**
     * @brief 获取所有存储的最新日志
     * @param count 每个存储的日志数量
     * @param level_filter 级别过滤
     * @return 最新日志列表
     */
    QList<LogEntry> getAllLatestLogs(int count, 
                                   const QList<LogLevel>& level_filter = {});

    /**
     * @brief 获取指定进程的最新日志
     * @param process_id 进程ID
     * @param count 日志数量
     * @param level_filter 级别过滤
     * @return 最新日志列表
     */
    QList<LogEntry> getProcessLatestLogs(const QString& process_id, 
                                       int count,
                                       const QList<LogLevel>& level_filter = {});

    // === 统一管理接口 ===
    
    /**
     * @brief 启动所有存储服务
     * @return 成功启动的存储数量
     */
    int startAllStorages();

    /**
     * @brief 停止所有存储服务
     */
    void stopAllStorages();

    /**
     * @brief 清理所有存储的过期日志
     * @param days_to_keep 保留天数
     * @return 总共清理的日志数量
     */
    int cleanupAllOldLogs(int days_to_keep);

    /**
     * @brief 归档所有存储的日志
     * @param start_time 开始时间
     * @param end_time 结束时间
     * @param archive_base_path 归档基础路径
     * @return 成功归档的存储数量
     */
    int archiveAllLogs(const QDateTime& start_time, 
                      const QDateTime& end_time, 
                      const QString& archive_base_path);

    /**
     * @brief 获取所有存储的统计信息
     * @param condition 统计条件
     * @return 聚合统计信息
     */
    LogStatistics getAggregatedStatistics(const LogQueryCondition& condition = LogQueryCondition());

    /**
     * @brief 获取指定进程的统计信息
     * @param process_id 进程ID
     * @param condition 统计条件
     * @return 统计信息
     */
    LogStatistics getProcessStatistics(const QString& process_id, 
                                     const LogQueryCondition& condition = LogQueryCondition());

    /**
     * @brief 检查所有存储的健康状态
     * @return 健康状态映射表
     */
    QMap<QString, bool> checkAllStorageHealth();

    /**
     * @brief 获取所有存储的容量信息
     * @return 容量信息映射表
     */
    QMap<QString, QJsonObject> getAllStorageInfo();

    /**
     * @brief 刷新所有存储缓存
     * @return 成功刷新的存储数量
     */
    int flushAllStorages();

    /**
     * @brief 为所有存储创建索引
     * @param field_name 索引字段名
     * @return 成功创建索引的存储数量
     */
    int createIndexForAllStorages(const QString& field_name);

    /**
     * @brief 获取聚合器状态信息
     * @return 状态信息JSON
     */
    QJsonObject getAggregatorStatus() const;

signals:
    /**
     * @brief 存储注册完成信号
     * @param process_id 进程ID
     * @param storage_type 存储类型
     * @param success 是否成功
     */
    void storageRegistered(const QString& process_id, const QString& storage_type, bool success);

    /**
     * @brief 存储取消注册信号
     * @param process_id 进程ID
     */
    void storageUnregistered(const QString& process_id);

    /**
     * @brief 日志写入完成信号（聚合）
     * @param process_id 进程ID
     * @param entry 日志条目
     * @param success 是否成功
     */
    void logWrittenToProcess(const QString& process_id, const LogEntry& entry, bool success);

    /**
     * @brief 批量日志写入完成信号（聚合）
     * @param total_count 总写入数量
     * @param success_count 成功数量
     */
    void batchLogWrittenAggregated(int total_count, int success_count);

    /**
     * @brief 存储健康状态变化信号
     * @param process_id 进程ID
     * @param is_healthy 是否健康
     * @param error_message 错误信息
     */
    void storageHealthChanged(const QString& process_id, bool is_healthy, const QString& error_message);

    /**
     * @brief 聚合器状态变化信号
     * @param total_storages 总存储数量
     * @param healthy_storages 健康存储数量
     */
    void aggregatorStatusChanged(int total_storages, int healthy_storages);

private:
    // 存储进程ID与存储类型的映射
    QMap<QString, LogStorageType> m_storage_types;
    // 存储进程ID与存储配置的映射
    QMap<QString, QJsonObject> m_storage_configs;
    // 存储进程ID与存储类型字符串的映射
    QMap<QString, QString> m_storage_type_strings;
    // 存储进程ID与存储实例的映射
    std::map<QString, std::unique_ptr<ILogStorage>> m_storage_instances;
    
    /**
     * @brief 连接存储信号
     * @param process_id 进程ID
     * @param storage 存储指针
     */
    void connectStorageSignals(const QString& process_id, ILogStorage* storage);
    
    /**
     * @brief 断开存储信号
     * @param storage 存储指针
     */
    void disconnectStorageSignals(ILogStorage* storage);

private slots:
    /**
     * @brief 处理存储日志写入信号
     */
    void onStorageLogWritten(const LogEntry& entry, bool success);
    
    /**
     * @brief 处理存储状态变化信号
     */
    void onStorageStateChanged(bool is_healthy, const QString& error_message);
};

/**
 * @brief 日志存储工厂类（工厂模式核心）
 * 
 * 负责根据配置创建具体的日志存储实现。
 * 与聚合器配合，提供统一的对象创建接口。
 * 
 * 主要功能：
 * - 存储实例创建（根据类型和配置）
 * - 类型转换工具（字符串与枚举互转）
 * - 注册机制（支持动态注册新的存储实现）
 */
class LogStorageFactory {
public:
    /**
     * @brief 存储创建函数类型定义
     */
    using StorageCreator = std::function<std::unique_ptr<ILogStorage>(const QJsonObject&)>;

    /**
     * @brief 创建日志存储实例
     * @param type 存储类型
     * @param config 配置参数
     * @return 日志存储实例智能指针
     */
    static std::unique_ptr<ILogStorage> createLogStorage(
        LogStorageType type, 
        const QJsonObject& config = QJsonObject()
    );

    /**
     * @brief 注册新的存储实现类型
     * @param type 存储类型
     * @param creator 创建函数
     * @return 是否注册成功
     */
    static bool registerStorageType(LogStorageType type, StorageCreator creator);

    /**
     * @brief 检查指定类型是否已注册
     * @param type 存储类型
     * @return 是否已注册
     */
    static bool isTypeRegistered(LogStorageType type);

    /**
     * @brief 获取所有已注册的类型
     * @return 已注册类型列表
     */
    static QList<LogStorageType> getRegisteredTypes();

    /**
     * @brief 从字符串获取存储类型
     * @param type_str 类型字符串
     * @return 存储类型
     */
    static LogStorageType getStorageTypeFromString(const QString& type_str);

    /**
     * @brief 获取存储类型字符串
     * @param type 存储类型
     * @return 类型字符串
     */
    static QString getStorageTypeString(LogStorageType type);

private:
    static QMap<LogStorageType, StorageCreator> s_creators;  // 存储创建器映射
};

// 便于日志输出的操作符重载
QString logLevelToString(LogLevel level);
QString logCategoryToString(LogCategory category);
LogLevel logLevelFromString(const QString& level_str);
LogCategory logCategoryFromString(const QString& category_str);

// 日志宏定义，便于使用
#define LOG_TRACE(process, message) \
    LogEntry::create(LogLevel::kTrace, LogCategory::kSystem, process, message, __FILE__, __FUNCTION__, __LINE__)

#define LOG_DEBUG(process, message) \
    LogEntry::create(LogLevel::kDebug, LogCategory::kSystem, process, message, __FILE__, __FUNCTION__, __LINE__)

#define LOG_INFO(process, message) \
    LogEntry::create(LogLevel::kInfo, LogCategory::kSystem, process, message, __FILE__, __FUNCTION__, __LINE__)

#define LOG_WARNING(process, message) \
    LogEntry::create(LogLevel::kWarning, LogCategory::kSystem, process, message, __FILE__, __FUNCTION__, __LINE__)

#define LOG_ERROR(process, message) \
    LogEntry::create(LogLevel::kError, LogCategory::kSystem, process, message, __FILE__, __FUNCTION__, __LINE__)

#define LOG_FATAL(process, message) \
    LogEntry::create(LogLevel::kFatal, LogCategory::kSystem, process, message, __FILE__, __FUNCTION__, __LINE__)

#endif // MASTER_SRC_ILOGSTORAGE_H_
