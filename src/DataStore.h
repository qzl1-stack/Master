#ifndef DATA_STORE_H
#define DATA_STORE_H

#include <QObject>
#include <QVariant>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QJsonObject>
#include <QJsonValue>
#include <QTimer>
#include <functional>
#include <memory>

/**
 * @brief DataStore 动态数据中心类
 * 
 * 运行时共享数据中心，负责存储和分发如IP列表、进程状态等动态数据，
 * 支持事件订阅与通知，便于界面和业务模块实时响应数据变化。
 * 采用单例模式确保数据中心全局唯一。
 * 
 * 主要功能：
 * - 动态数据存储和管理
 * - 事件订阅与通知机制
 * - 线程安全的数据访问
 * - 数据变化监控
 * - 实时状态更新
 */
class DataStore : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 事件订阅回调函数类型
     * @param key 数据键
     * @param oldValue 旧值
     * @param newValue 新值
     */
    using SubscriberCallback = std::function<void(const QString& key, const QVariant& oldValue, const QVariant& newValue)>;

    static DataStore& getInstance();
    DataStore(const DataStore&) = delete;
    DataStore& operator=(const DataStore&) = delete;
    bool initialize();

    // === 数据存储和访问 ===
    /**
     * @brief 设置数据值
     * @param key 数据键
     * @param value 数据值
     * @param notifySubscribers 是否通知订阅者
     */
    void setValue(const QString& key, const QVariant& value, bool notifySubscribers = true);

    /**
     * @brief 获取数据值
     * @param key 数据键
     * @param defaultValue 默认值
     * @return 数据值
     */
    QVariant getValue(const QString& key, const QVariant& defaultValue = QVariant()) const;

    /**
     * @brief 检查是否包含指定键
     * @param key 数据键
     * @return 是否包含该键
     */
    bool contains(const QString& key) const;

    /**
     * @brief 移除数据项
     * @param key 数据键
     * @return 是否成功移除
     */
    bool removeValue(const QString& key);

    /**
     * @brief 获取所有数据键
     * @return 数据键列表
     */
    QStringList getAllKeys() const;

    /**
     * @brief 清空所有数据
     */
    void clear();

    // === 进程状态管理 ===
    /**
     * @brief 设置进程状态
     * @param processName 进程名称
     * @param status 进程状态
     */
    void setProcessStatus(const QString& processName, const QString& status);

    /**
     * @brief 获取进程状态
     * @param processName 进程名称
     * @return 进程状态
     */
    QString getProcessStatus(const QString& processName) const;

    /**
     * @brief 获取所有进程状态
     * @return 进程状态映射表
     */
    QHash<QString, QString> getAllProcessStatus() const;

    // === IP表管理 ===
    /**
     * @brief 设置当前IP表快照
     * @param ipList IP地址列表
     */
    void setCurrentIpTable(const QStringList& ipList);

    /**
     * @brief 获取当前IP表快照
     * @return IP地址列表
     */
    QStringList getCurrentIpTable() const;

    // === 系统监控数据管理 ===
    /**
     * @brief 设置CPU使用率
     * @param usage CPU使用率（百分比）
     */
    void setCpuUsage(double usage);

    /**
     * @brief 获取CPU使用率
     * @return CPU使用率（百分比）
     */
    double getCpuUsage() const;

    /**
     * @brief 设置内存使用量
     * @param usage 内存使用量（MB）
     */
    void setMemoryUsage(double usage);

    /**
     * @brief 获取内存使用量
     * @return 内存使用量（MB）
     */
    double getMemoryUsage() const;

    /**
     * @brief 更新进程心跳时间
     * @param processName 进程名称
     */
    void updateProcessHeartbeat(const QString& processName);

    /**
     * @brief 获取进程最后心跳时间
     * @param processName 进程名称
     * @return 最后心跳时间戳
     */
    qint64 getProcessLastHeartbeat(const QString& processName) const;

    // === 事件订阅机制 ===
    /**
     * @brief 订阅数据变化事件
     * @param key 数据键（支持通配符*）
     * @param subscriber 订阅者标识
     * @param callback 回调函数
     * @return 订阅是否成功
     */
    bool subscribe(const QString& key, QObject* subscriber, SubscriberCallback callback);

    /**
     * @brief 取消订阅
     * @param key 数据键
     * @param subscriber 订阅者标识
     * @return 取消订阅是否成功
     */
    bool unsubscribe(const QString& key, QObject* subscriber);

    /**
     * @brief 取消订阅者的所有订阅
     * @param subscriber 订阅者标识
     */
    void unsubscribeAll(QObject* subscriber);

    /**
     * @brief 获取订阅者数量
     * @param key 数据键
     * @return 订阅者数量
     */
    int getSubscriberCount(const QString& key) const;

    // === 数据快照和导出 ===
    /**
     * @brief 创建数据快照
     * @return 数据快照JSON对象
     */
    QJsonObject createSnapshot() const;

    /**
     * @brief 从快照恢复数据
     * @param snapshot 数据快照JSON对象
     * @return 恢复是否成功
     */
    bool restoreFromSnapshot(const QJsonObject& snapshot);

    /**
     * @brief 导出指定前缀的数据
     * @param prefix 数据键前缀
     * @return 导出的数据映射表
     */
    QHash<QString, QVariant> exportData(const QString& prefix = "") const;

signals:
    /**
     * @brief 数据值已改变信号
     * @param key 数据键
     * @param oldValue 旧值
     * @param newValue 新值
     */
    void valueChanged(const QString& key, const QVariant& oldValue, const QVariant& newValue);

    /**
     * @brief 进程状态已改变信号
     * @param processName 进程名称
     * @param oldStatus 旧状态
     * @param newStatus 新状态
     */
    void processStatusChanged(const QString& processName, const QString& oldStatus, const QString& newStatus);

    /**
     * @brief 系统监控数据已更新信号
     * @param cpuUsage CPU使用率
     * @param memoryUsage 内存使用量
     */
    void systemMetricsUpdated(double cpuUsage, double memoryUsage);

    /**
     * @brief 进程心跳更新信号
     * @param processName 进程名称
     * @param timestamp 心跳时间戳
     */
    void processHeartbeatUpdated(const QString& processName, qint64 timestamp);

private slots:
    /**
     * @brief 清理断开连接的订阅者
     */
    void cleanupDisconnectedSubscribers();

private:
    /**
     * @brief 私有构造函数（单例模式）
     */
    explicit DataStore(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~DataStore() override;

    // 友元类，允许unique_ptr的默认删除器访问私有析构函数
    friend class std::default_delete<DataStore>;

    /**
     * @brief 通知订阅者数据变化
     * @param key 数据键
     * @param oldValue 旧值
     * @param newValue 新值
     */
    void notifySubscribers(const QString& key, const QVariant& oldValue, const QVariant& newValue);

    /**
     * @brief 检查键是否匹配模式（支持通配符）
     * @param pattern 模式字符串
     * @param key 数据键
     * @return 是否匹配
     */
    bool matchesPattern(const QString& pattern, const QString& key) const;

    /**
     * @brief 生成内部键名
     * @param category 类别
     * @param key 键名
     * @return 内部键名
     */
    QString generateInternalKey(const QString& category, const QString& key) const;

private:
    /**
     * @brief 订阅者信息结构
     */
    struct SubscriberInfo {
        QObject* subscriber;           ///< 订阅者对象指针
        SubscriberCallback callback;   ///< 回调函数
        QString pattern;              ///< 订阅模式
        
        SubscriberInfo(QObject* sub, SubscriberCallback cb, const QString& pat)
            : subscriber(sub), callback(std::move(cb)), pattern(pat) {}
    };

    static std::unique_ptr<DataStore> instance_;    ///< 单例实例
    static QMutex instance_mutex_;                  ///< 单例创建互斥锁

    mutable QMutex data_mutex_;                     ///< 数据访问互斥锁
    QHash<QString, QVariant> data_;                 ///< 数据存储
    QHash<QString, QList<SubscriberInfo>> subscribers_; ///< 事件订阅者
    QTimer* cleanup_timer_;                         ///< 清理定时器
    bool initialized_;                              ///< 初始化状态
};

#endif // DATA_STORE_H
