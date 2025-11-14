#include "DataStore.h"
#include <QDebug>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QMetaType>

// 静态成员初始化
std::unique_ptr<DataStore> DataStore::instance_ = nullptr;
QMutex DataStore::instance_mutex_;

// 内部键名常量
namespace {
    const QString PROCESS_STATUS_PREFIX = "process_status.";
    const QString PROCESS_HEARTBEAT_PREFIX = "process_heartbeat.";
    const QString SYSTEM_METRICS_PREFIX = "system_metrics.";
    const QString IP_TABLE_KEY = "current_ip_table";
    const QString CPU_USAGE_KEY = "system_metrics.cpu_usage";
    const QString MEMORY_USAGE_KEY = "system_metrics.memory_usage";
}

DataStore::DataStore(QObject *parent)
    : QObject(parent)
    , cleanup_timer_(nullptr)
    , initialized_(false)
{
    // 初始化清理定时器
    cleanup_timer_ = new QTimer(this);
    connect(cleanup_timer_, &QTimer::timeout,
            this, &DataStore::cleanupDisconnectedSubscribers);
}

DataStore::~DataStore()
{
    if (cleanup_timer_) {
        cleanup_timer_->stop();
    }
}

DataStore& DataStore::getInstance()
{
    QMutexLocker locker(&instance_mutex_);
    if (!instance_) {
        instance_ = std::unique_ptr<DataStore>(new DataStore());
    }
    return *instance_;
}

bool DataStore::initialize()
{
    QMutexLocker locker(&data_mutex_);
    
    if (initialized_) {
        qWarning() << "DataStore already initialized";
        return true;
    }
    
    // 初始化基础数据
    data_.clear();
    subscribers_.clear();
    
    // 初始化系统监控数据
    data_[CPU_USAGE_KEY] = 0.0;
    data_[MEMORY_USAGE_KEY] = 0.0;
    
    // 初始化IP表
    data_[IP_TABLE_KEY] = QStringList();
    
    // 启动清理定时器（每5分钟清理一次）
    cleanup_timer_->start(5 * 60 * 1000);
    
    initialized_ = true;
    qInfo() << "DataStore initialized successfully";
    
    return true;
}

void DataStore::setValue(const QString& key, const QVariant& value, bool notifySubscribers)
{
    if (key.isEmpty()) {
        qWarning() << "Cannot set value with empty key";
        return;
    }
    
    QVariant oldValue;
    
    {
        QMutexLocker locker(&data_mutex_);
        oldValue = data_.value(key);
        
        // 如果值没有变化，则不需要更新
        if (oldValue == value) {
            return;
        }
        
        data_[key] = value;
    }
    
    // 发送信号
    emit valueChanged(key, oldValue, value);
    
    // 通知订阅者
    if (notifySubscribers) {
        this->notifySubscribers(key, oldValue, value);
    }
}

QVariant DataStore::getValue(const QString& key, const QVariant& defaultValue) const
{
    QMutexLocker locker(&data_mutex_);
    return data_.value(key, defaultValue);
}

bool DataStore::contains(const QString& key) const
{
    QMutexLocker locker(&data_mutex_);
    return data_.contains(key);
}

bool DataStore::removeValue(const QString& key)
{
    QMutexLocker locker(&data_mutex_);
    
    if (!data_.contains(key)) {
        return false;
    }
    
    QVariant oldValue = data_.value(key);
    data_.remove(key);
    
    // 在解锁后发送信号
    QMetaObject::invokeMethod(this, [this, key, oldValue]() {
        emit valueChanged(key, oldValue, QVariant());
        notifySubscribers(key, oldValue, QVariant());
    }, Qt::QueuedConnection);
    
    return true;
}

QStringList DataStore::getAllKeys() const
{
    QMutexLocker locker(&data_mutex_);
    return data_.keys();
}

void DataStore::clear()
{
    QHash<QString, QVariant> oldData;
    
    {
        QMutexLocker locker(&data_mutex_);
        oldData = data_;
        data_.clear();
    }
    
    // 通知所有数据被清空
    for (auto it = oldData.begin(); it != oldData.end(); ++it) {
        QMetaObject::invokeMethod(this, [this, key = it.key(), oldValue = it.value()]() {
            emit valueChanged(key, oldValue, QVariant());
            notifySubscribers(key, oldValue, QVariant());
        }, Qt::QueuedConnection);
    }
    
    qInfo() << "DataStore cleared";
}

void DataStore::setProcessStatus(const QString& processName, const QString& status)
{
    if (processName.isEmpty()) {
        qWarning() << "Cannot set process status with empty process name";
        return;
    }
    
    QString key = generateInternalKey(PROCESS_STATUS_PREFIX, processName);
    QString oldStatus = getValue(key).toString();
    
    setValue(key, status);
    
    // 发送专用的进程状态变化信号
    if (oldStatus != status) {
        emit processStatusChanged(processName, oldStatus, status);
    }
}

QString DataStore::getProcessStatus(const QString& processName) const
{
    QMutexLocker locker(&data_mutex_);
    return data_.value(generateInternalKey(PROCESS_STATUS_PREFIX, processName), "未知").toString();
}

QHash<QString, QString> DataStore::getAllProcessStatus() const
{
    QMutexLocker locker(&data_mutex_);
    
    QHash<QString, QString> processStatus;
    
    for (auto it = data_.begin(); it != data_.end(); ++it) {
        const QString& key = it.key();
        if (key.startsWith(PROCESS_STATUS_PREFIX)) {
            QString processName = key.mid(PROCESS_STATUS_PREFIX.length());
            processStatus[processName] = it.value().toString();
        }
    }
    
    return processStatus;
}

void DataStore::setCurrentIpTable(const QStringList& ipList)
{
    setValue(IP_TABLE_KEY, ipList);
}

QStringList DataStore::getCurrentIpTable() const
{
    QMutexLocker locker(&data_mutex_);
    return data_.value(IP_TABLE_KEY).toStringList();
}

void DataStore::setCpuUsage(double usage)
{
    setValue(CPU_USAGE_KEY, usage);
}

double DataStore::getCpuUsage() const
{
    QMutexLocker locker(&data_mutex_);
    return data_.value(CPU_USAGE_KEY, 0.0).toDouble();
}

void DataStore::setMemoryUsage(double usage)
{
    setValue(MEMORY_USAGE_KEY, usage);
}

double DataStore::getMemoryUsage() const
{
    QMutexLocker locker(&data_mutex_);
    return data_.value(MEMORY_USAGE_KEY, 0.0).toDouble();
}

void DataStore::updateProcessHeartbeat(const QString& processName)
{
    if (processName.isEmpty()) {
        qWarning() << "Cannot update heartbeat with empty process name";
        return;
    }
    
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    QString key = generateInternalKey(PROCESS_HEARTBEAT_PREFIX, processName);
    
    setValue(key, timestamp);
    
    // 发送心跳更新信号
    emit processHeartbeatUpdated(processName, timestamp);
}

qint64 DataStore::getProcessLastHeartbeat(const QString& processName) const
{
    QMutexLocker locker(&data_mutex_);
    return data_.value(generateInternalKey(PROCESS_HEARTBEAT_PREFIX, processName), 0).toLongLong();
}

bool DataStore::subscribe(const QString& key, QObject* subscriber, SubscriberCallback callback)
{
    if (key.isEmpty() || !subscriber || !callback) {
        qWarning() << "Invalid subscription parameters";
        return false;
    }
    
    QMutexLocker locker(&data_mutex_);
    
    // 检查是否已经订阅
    if (subscribers_.contains(key)) {
        for (const auto& info : subscribers_[key]) {
            if (info.subscriber == subscriber) {
                qWarning() << "Subscriber already exists for key:" << key;
                return false;
            }
        }
    }
    
    // 添加订阅
    subscribers_[key].append(SubscriberInfo(subscriber, std::move(callback), key));
    
    // 监听订阅者的销毁信号
    connect(subscriber, &QObject::destroyed, this, [this, key, subscriber]() {
        unsubscribe(key, subscriber);
    }, Qt::UniqueConnection);
    
    qDebug() << "Subscription added for key:" << key << "subscriber:" << subscriber;
    return true;
}

bool DataStore::unsubscribe(const QString& key, QObject* subscriber)
{
    QMutexLocker locker(&data_mutex_);
    
    if (!subscribers_.contains(key)) {
        return false;
    }
    
    auto& subscriberList = subscribers_[key];
    for (int i = subscriberList.size() - 1; i >= 0; --i) {
        if (subscriberList[i].subscriber == subscriber) {
            subscriberList.removeAt(i);
            qDebug() << "Subscription removed for key:" << key << "subscriber:" << subscriber;
            
            // 如果该键没有订阅者了，移除键
            if (subscriberList.isEmpty()) {
                subscribers_.remove(key);
            }
            
            return true;
        }
    }
    
    return false;
}

void DataStore::unsubscribeAll(QObject* subscriber)
{
    QMutexLocker locker(&data_mutex_);
    
    QStringList keysToRemove;
    
    for (auto it = subscribers_.begin(); it != subscribers_.end(); ++it) {
        const QString& key = it.key();
        auto& subscriberList = it.value();
        
        // 移除指定订阅者
        for (int i = subscriberList.size() - 1; i >= 0; --i) {
            if (subscriberList[i].subscriber == subscriber) {
                subscriberList.removeAt(i);
            }
        }
        
        // 如果该键没有订阅者了，标记为待移除
        if (subscriberList.isEmpty()) {
            keysToRemove.append(key);
        }
    }
    
    // 移除空的键
    for (const QString& key : keysToRemove) {
        subscribers_.remove(key);
    }
    
    qDebug() << "All subscriptions removed for subscriber:" << subscriber;
}

int DataStore::getSubscriberCount(const QString& key) const
{
    QMutexLocker locker(&data_mutex_);
    return subscribers_.value(key).size();
}

QJsonObject DataStore::createSnapshot() const
{
    QMutexLocker locker(&data_mutex_);
    
    QJsonObject snapshot;
    snapshot["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    snapshot["version"] = "1.0";
    
    QJsonObject dataObj;
    for (auto it = data_.begin(); it != data_.end(); ++it) {
        const QString& key = it.key();
        const QVariant& value = it.value();
        
        // 转换QVariant到QJsonValue
        if (value.typeId() == QMetaType::QStringList) {
            QJsonArray array;
            for (const QString& str : value.toStringList()) {
                array.append(str);
            }
            dataObj[key] = array;
        } else {
            dataObj[key] = QJsonValue::fromVariant(value);
        }
    }
    
    snapshot["data"] = dataObj;
    
    return snapshot;
}

bool DataStore::restoreFromSnapshot(const QJsonObject& snapshot)
{
    if (!snapshot.contains("data") || !snapshot["data"].isObject()) {
        qWarning() << "Invalid snapshot format";
        return false;
    }
    
    QJsonObject dataObj = snapshot["data"].toObject();
    
    // 清空现有数据
    clear();
    
    // 恢复数据
    for (auto it = dataObj.begin(); it != dataObj.end(); ++it) {
        const QString& key = it.key();
        const QJsonValue& jsonValue = it.value();
        
        QVariant value;
        if (jsonValue.isArray()) {
            QStringList stringList;
            QJsonArray array = jsonValue.toArray();
            for (const QJsonValue& item : array) {
                stringList.append(item.toString());
            }
            value = stringList;
        } else {
            value = jsonValue.toVariant();
        }
        
        setValue(key, value, false); // 不通知订阅者，避免大量信号
    }
    
    qInfo() << "DataStore restored from snapshot, data count:" << data_.size();
    return true;
}

QHash<QString, QVariant> DataStore::exportData(const QString& prefix) const
{
    QMutexLocker locker(&data_mutex_);
    
    QHash<QString, QVariant> exported;
    
    if (prefix.isEmpty()) {
        exported = data_;
    } else {
        for (auto it = data_.begin(); it != data_.end(); ++it) {
            if (it.key().startsWith(prefix)) {
                exported[it.key()] = it.value();
            }
        }
    }
    
    return exported;
}

void DataStore::cleanupDisconnectedSubscribers()
{
    QMutexLocker locker(&data_mutex_);
    
    QStringList keysToRemove;
    int removedCount = 0;
    
    for (auto it = subscribers_.begin(); it != subscribers_.end(); ++it) {
        const QString& key = it.key();
        auto& subscriberList = it.value();
        
        // 移除已销毁的订阅者
        for (int i = subscriberList.size() - 1; i >= 0; --i) {
            if (!subscriberList[i].subscriber) {
                subscriberList.removeAt(i);
                removedCount++;
            }
        }
        
        // 如果该键没有订阅者了，标记为待移除
        if (subscriberList.isEmpty()) {
            keysToRemove.append(key);
        }
    }
    
    // 移除空的键
    for (const QString& key : keysToRemove) {
        subscribers_.remove(key);
    }
    
    if (removedCount > 0) {
        qDebug() << "Cleaned up" << removedCount << "disconnected subscribers";
    }
}

void DataStore::notifySubscribers(const QString& key, const QVariant& oldValue, const QVariant& newValue)
{
    // 创建订阅者回调的副本，避免在回调过程中修改订阅者列表导致的问题
    QList<SubscriberInfo> callbackList;
    
    {
        QMutexLocker locker(&data_mutex_);
        
        // 查找所有匹配的订阅者
        for (auto it = subscribers_.begin(); it != subscribers_.end(); ++it) {
            const QString& pattern = it.key();
            const auto& subscriberList = it.value();
            
            if (matchesPattern(pattern, key)) {
                for (const auto& info : subscriberList) {
                    if (info.subscriber) {  // 确保订阅者仍然有效
                        callbackList.append(info);
                    }
                }
            }
        }
    }
    
    // 在解锁后执行回调
    for (const auto& info : callbackList) {
        if (info.subscriber && info.callback) {
            try {
                info.callback(key, oldValue, newValue);
            } catch (const std::exception& e) {
                qWarning() << "Exception in subscriber callback:" << e.what();
            } catch (...) {
                qWarning() << "Unknown exception in subscriber callback";
            }
        }
    }
}

bool DataStore::matchesPattern(const QString& pattern, const QString& key) const
{
    // 简单的通配符匹配，支持 * 通配符
    if (pattern == "*") {
        return true;
    }
    
    if (!pattern.contains('*')) {
        return pattern == key;
    }
    
    // 使用正则表达式进行通配符匹配
    QString regexPattern = QRegularExpression::escape(pattern);
    regexPattern.replace("\\*", ".*");
    regexPattern = "^" + regexPattern + "$";
    
    QRegularExpression regex(regexPattern);
    return regex.match(key).hasMatch();
}

QString DataStore::generateInternalKey(const QString& category, const QString& key) const
{
    return category + key;
}
