#ifndef MASTER_SRC_IIPCCOMMUNICATION_H_
#define MASTER_SRC_IIPCCOMMUNICATION_H_

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <functional>
#include <memory>

/**
 * @brief IPC通信消息类型枚举
 */
enum class MessageType {
    kHello = 0,          // 握手消息
    kHelloAck,           // 握手确认
    kHeartbeat,          // 心跳消息
    kHeartbeatAck,       // 心跳确认
    kConfigUpdate,       // 配置更新
    kCommand,            // 命令消息
    kCommandResponse,    // 命令响应
    kStatusReport,       // 状态上报
    kLogMessage,         // 日志消息
    kErrorReport,        // 错误上报
    kShutdown            // 关闭消息
};

/**
 * @brief IPC通信消息结构
 */
struct IpcMessage {
    MessageType type;           // 消息类型
    QString topic;              // 消息主题
    QString msg_id;             // 消息ID（UUID）
    qint64 timestamp;           // 时间戳
    QString sender_id;          // 发送者ID
    QString receiver_id;        // 接收者ID（空表示广播）
    QJsonObject body;           // 消息体
    
    // 序列化为JSON
    QJsonObject toJson() const;
    
    // 从JSON反序列化
    static IpcMessage fromJson(const QJsonObject& json);
    
    // 转换为字节数组
    QByteArray toByteArray() const;
    
    // 从字节数组解析
    static IpcMessage fromByteArray(const QByteArray& data);
};

/**
 * @brief IPC通信连接状态枚举
 */
enum class ConnectionState {
    kDisconnected = 0,   // 未连接
    kConnecting,         // 连接中
    kConnected,          // 已连接
    kInitialized,        // 已初始化
    kAuthenticated,      // 已认证
    kError               // 连接错误
};

/**
 * @brief IPC通信抽象接口
 * 
 * 该接口定义了进程间通信的标准行为，支持多种IPC实现方式。
 * 采用策略模式，便于扩展和测试不同的通信实现。
 * 
 * 主要功能：
 * - 连接管理（连接、断开、重连）
 * - 消息发送（点对点、广播、Topic订阅）
 * - 消息接收（异步回调处理）
 * - 状态监控（连接状态、心跳检测）
 * - 错误处理（连接异常、消息失败）
 */
class IIpcCommunication : public QObject {
    Q_OBJECT

public:
    explicit IIpcCommunication(QObject* parent = nullptr);
    virtual ~IIpcCommunication() = default;
    virtual bool initialize(const QJsonObject& config) = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;

    /**
     * @brief 获取当前连接状态
     * @return 连接状态
     */
    virtual ConnectionState getConnectionState() const = 0;

    /**
     * @brief 发送消息到指定接收者
     * @param message 要发送的消息
     * @return 是否发送成功
     */
    virtual bool sendMessage(const IpcMessage& message) = 0;
    virtual bool sendMessage(const QString& client_id, const IpcMessage& message) = 0;

    /**
     * @brief 广播消息到所有连接的客户端
     * @param message 要广播的消息
     * @return 是否广播成功
     */
    virtual bool broadcastMessage(const IpcMessage& message) = 0;

    /**
     * @brief 发布消息到指定Topic
     * @param topic 主题名称
     * @param message 要发布的消息
     * @return 是否发布成功
     */
    virtual bool publishToTopic(const QString& topic, const IpcMessage& message) = 0;

    /**
     * @brief 订阅指定Topic
     * @param topic 主题名称
     * @return 是否订阅成功
     */
    virtual bool subscribeToTopic(const QString& topic) = 0;

    /**
     * @brief 取消订阅指定Topic
     * @param topic 主题名称
     * @return 是否取消成功
     */
    virtual bool unsubscribeFromTopic(const QString& topic) = 0;

    /**
     * @brief 获取已订阅的Topic列表
     * @return Topic列表
     */
    virtual QStringList getSubscribedTopics() const = 0;

    /**
     * @brief 获取连接的客户端数量
     * @return 客户端数量
     */
    virtual int getConnectedClientCount() const = 0;

    /**
     * @brief 获取连接的客户端ID列表
     * @return 客户端ID列表
     */
    virtual QStringList getConnectedClientIds() const = 0;

    /**
     * @brief 断开指定客户端连接
     * @param client_id 客户端ID
     * @return 是否断开成功
     */
    virtual bool disconnectClient(const QString& client_id) = 0;

    /**
     * @brief 检查指定客户端是否在线
     * @param client_id 客户端ID
     * @return 是否在线
     */
    virtual bool isClientOnline(const QString& client_id) const = 0;

    /**
     * @brief 获取最后一次错误信息
     * @return 错误信息
     */
    virtual QString getLastError() const = 0;

    /**
     * @brief 根据发送者信息获取对应的客户端ID
     * @param sender_id 原始发送者ID
     * @return 对应的客户端ID，如果未找到则返回空字符串
     */
    virtual QString getClientIdBySenderId(const QString& sender_id) const = 0;

signals:
    /**
     * @brief 收到新消息信号
     * @param message 收到的消息
     */
    void messageReceived(const IpcMessage& message);

    /**
     * @brief 客户端连接信号
     * @param client_id 客户端ID
     */
    void clientConnected(const QString& client_id);

    /**
     * @brief 客户端断开信号
     * @param client_id 客户端ID
     */
    void clientDisconnected(const QString& client_id);

    /**
     * @brief 连接状态变化信号
     * @param state 新的连接状态
     */
    void connectionStateChanged(ConnectionState state);

    /**
     * @brief 发生错误信号
     * @param error_message 错误信息
     */
    void errorOccurred(const QString& error_message);

    /**
     * @brief Topic订阅状态变化信号
     * @param topic 主题名称
     * @param subscribed 是否已订阅
     */
    void topicSubscriptionChanged(const QString& topic, bool subscribed);
};

/**
 * @brief IPC通信类型枚举
 */
enum class IpcType {
    kLocalSocket = 0,    // 本地Socket
    kTcpSocket,          // TCP Socket
    kNamedPipe,          // 命名管道
    kRabbitMQ            // RabbitMQ消息队列
};

/**
 * @brief IPC策略上下文类（策略模式核心）
 * 
 * 该类是策略模式的上下文，负责持有和管理IPC通信策略。
 * 支持运行时动态切换不同的IPC实现方式，提供统一的接口。
 * 
 * 主要功能：
 * - 策略管理（设置、切换、获取当前策略）
 * - 方法代理（代理所有IIpcCommunication接口方法）
 * - 信号转发（转发所有策略对象的信号）
 * - 状态监控（监控策略切换和连接状态）
 */
class IpcContext : public QObject {
    Q_OBJECT

public:
    explicit IpcContext(QObject* parent = nullptr);
    virtual ~IpcContext() = default;

    /**
     * @brief 设置IPC通信策略
     * @param strategy 具体的IPC实现策略
     * @return 是否设置成功
     */
    bool setIpcStrategy(std::unique_ptr<IIpcCommunication> strategy);

    /**
     * @brief 获取当前IPC策略
     * @return 当前策略指针（可能为nullptr）
     */
    IIpcCommunication* getCurrentStrategy() const;

    /**
     * @brief 获取当前策略类型
     * @return 策略类型字符串
     */
    QString getCurrentStrategyType() const;

    /**
     * @brief 检查是否有可用策略
     * @return 是否有策略
     */
    bool hasStrategy() const;

    /**
     * @brief 切换到指定类型的策略
     * @param type 新的IPC类型
     * @param config 配置参数
     * @return 是否切换成功
     */
    bool switchStrategy(IpcType type, const QJsonObject& config = QJsonObject());

    /**
     * @brief 优雅地切换策略（先停止当前策略再启动新策略）
     * @param type 新的IPC类型
     * @param config 配置参数
     * @return 是否切换成功
     */
    bool gracefulSwitchStrategy(IpcType type, const QJsonObject& config = QJsonObject());

    // === 代理IIpcCommunication接口的所有方法 ===
    
    /**
     * @brief 初始化IPC通信
     * @param config 通信配置参数
     * @return 是否初始化成功
     */
    bool initialize(const QJsonObject& config);

    /**
     * @brief 启动IPC通信服务
     * @return 是否启动成功
     */
    bool start();

    /**
     * @brief 停止IPC通信服务
     */
    void stop();

    /**
     * @brief 获取当前连接状态
     * @return 连接状态
     */
    ConnectionState getConnectionState() const;

    /**
     * @brief 发送消息到指定接收者
     * @param message 要发送的消息
     * @return 是否发送成功
     */
    bool sendMessage(const IpcMessage& message);

    bool sendMessage(const QString& client_id, const IpcMessage& message);

    /**
     * @brief 广播消息到所有连接的客户端
     * @param message 要广播的消息
     * @return 是否广播成功
     */
    bool broadcastMessage(const IpcMessage& message);

    /**
     * @brief 发布消息到指定Topic
     * @param topic 主题名称
     * @param message 要发布的消息
     * @return 是否发布成功
     */
    bool publishToTopic(const QString& topic, const IpcMessage& message);

    /**
     * @brief 订阅指定Topic
     * @param topic 主题名称
     * @return 是否订阅成功
     */
    bool subscribeToTopic(const QString& topic);

    /**
     * @brief 取消订阅指定Topic
     * @param topic 主题名称
     * @return 是否取消成功
     */
    bool unsubscribeFromTopic(const QString& topic);

    /**
     * @brief 获取已订阅的Topic列表
     * @return Topic列表
     */
    QStringList getSubscribedTopics() const;

    /**
     * @brief 获取连接的客户端数量
     * @return 客户端数量
     */
    int getConnectedClientCount() const;

    /**
     * @brief 获取连接的客户端ID列表
     * @return 客户端ID列表
     */
    QStringList getConnectedClientIds() const;

    /**
     * @brief 断开指定客户端连接
     * @param client_id 客户端ID
     * @return 是否断开成功
     */
    bool disconnectClient(const QString& client_id);

    /**
     * @brief 检查指定客户端是否在线
     * @param client_id 客户端ID
     * @return 是否在线
     */
    bool isClientOnline(const QString& client_id) const;

    /**
     * @brief 获取最后一次错误信息
     * @return 错误信息
     */
    QString getLastError() const;
    
    QString getClientIdBySenderId(const QString& sender_id) const;
signals:
    /**
     * @brief 策略切换完成信号
     * @param old_type 旧策略类型
     * @param new_type 新策略类型
     * @param success 是否成功
     */
    void strategyChanged(const QString& old_type, const QString& new_type, bool success);

    // === 转发IIpcCommunication接口的所有信号 ===
    
    /**
     * @brief 收到新消息信号
     * @param message 收到的消息
     */
    void messageReceived(const IpcMessage& message);

    /**
     * @brief 客户端连接信号
     * @param client_id 客户端ID
     */
    void clientConnected(const QString& client_id);

    /**
     * @brief 客户端断开信号
     * @param client_id 客户端ID
     */
    void clientDisconnected(const QString& client_id);

    /**
     * @brief 连接状态变化信号
     * @param state 新的连接状态
     */
    void connectionStateChanged(ConnectionState state);

    /**
     * @brief 发生错误信号
     * @param error_message 错误信息
     */
    void errorOccurred(const QString& error_message);

    /**
     * @brief Topic订阅状态变化信号
     * @param topic 主题名称
     * @param subscribed 是否已订阅
     */
    void topicSubscriptionChanged(const QString& topic, bool subscribed);

private:
    std::unique_ptr<IIpcCommunication> m_strategy;  // 当前策略
    QString m_current_strategy_type;                // 当前策略类型名称
    
    /**
     * @brief 连接策略对象的信号
     */
    void connectStrategySignals();
    
    /**
     * @brief 断开策略对象的信号
     */
    void disconnectStrategySignals();
};

/**
 * @brief IPC通信工厂类（工厂模式核心）
 * 
 * 负责根据配置创建具体的IPC通信实现。
 * 与策略模式配合，提供统一的对象创建接口。
 * 
 * 主要功能：
 * - 策略实例创建（根据类型和配置）
 * - 类型转换工具（字符串与枚举互转）
 * - 注册机制（支持动态注册新的IPC实现）
 */
class IpcCommunicationFactory {
public:
    /**
     * @brief 策略创建函数类型定义
     */
    using StrategyCreator = std::function<std::unique_ptr<IIpcCommunication>(const QJsonObject&)>;

    /**
     * @brief 创建IPC通信实例
     * @param type IPC通信类型
     * @param config 配置参数
     * @return IPC通信实例智能指针
     */
    static std::unique_ptr<IIpcCommunication> createIpcCommunication(
        IpcType type, 
        const QJsonObject& config = QJsonObject()
    );

    /**
     * @brief 注册新的IPC实现类型
     * @param type IPC类型
     * @param creator 创建函数
     * @return 是否注册成功
     */
    static bool registerIpcType(IpcType type, StrategyCreator creator);

    /**
     * @brief 检查指定类型是否已注册
     * @param type IPC类型
     * @return 是否已注册
     */
    static bool isTypeRegistered(IpcType type);

    /**
     * @brief 获取所有已注册的类型
     * @return 已注册类型列表
     */
    static QList<IpcType> getRegisteredTypes();

    /**
     * @brief 从字符串获取IPC类型
     * @param type_str 类型字符串
     * @return IPC类型
     */
    static IpcType getIpcTypeFromString(const QString& type_str);

    /**
     * @brief 获取IPC类型字符串
     * @param type IPC类型
     * @return 类型字符串
     */
    static QString getIpcTypeString(IpcType type);

private:
    static QMap<IpcType, StrategyCreator> s_creators;  // 策略创建器映射
};

// 便于日志输出的操作符重载
QString messageTypeToString(MessageType type);
QString connectionStateToString(ConnectionState state);

#endif // MASTER_SRC_IIPCCOMMUNICATION_H_
