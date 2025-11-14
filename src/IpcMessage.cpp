#include "IIpcCommunication.h"
#include <QUuid>
#include <QJsonDocument>
#include <QDebug>
#include <QMetaObject>
#include <QTimer>

// ================== IpcMessage 实现 ==================

// IIpcCommunication 构造函数实现
IIpcCommunication::IIpcCommunication(QObject* parent) : QObject(parent) {}

QJsonObject IpcMessage::toJson() const {
    QJsonObject json;
    json["type"] = static_cast<int>(type);
    json["topic"] = topic;
    json["msg_id"] = msg_id;
    json["timestamp"] = timestamp;
    json["sender_id"] = sender_id;
    json["receiver_id"] = receiver_id;
    json["body"] = body;
    return json;
}

IpcMessage IpcMessage::fromJson(const QJsonObject& json) {
    IpcMessage message;
    message.type = static_cast<MessageType>(json["type"].toInt());
    message.topic = json["topic"].toString();
    message.msg_id = json["msg_ id"].toString();
    message.timestamp = json["timestamp"].toVariant().toLongLong();
    message.sender_id = json["sender_id"].toString();
    message.receiver_id = json["receiver_id"].toString();
    message.body = json["body"].toObject();
    return message;
}

QByteArray IpcMessage::toByteArray() const {
    QJsonDocument doc(toJson());
    return doc.toJson(QJsonDocument::Compact);
}

IpcMessage IpcMessage::fromByteArray(const QByteArray& data) {
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse IpcMessage from JSON:" << error.errorString();
        return IpcMessage{};
    }
    
    return fromJson(doc.object());
}

// 工具函数实现
QString messageTypeToString(MessageType type) {
    switch (type) {
        case MessageType::kHello: return "HELLO";
        case MessageType::kHelloAck: return "HELLO_ACK";
        case MessageType::kHeartbeat: return "HEARTBEAT";
        case MessageType::kHeartbeatAck: return "HEARTBEAT_ACK";
        case MessageType::kConfigUpdate: return "CONFIG_UPDATE";
        case MessageType::kCommand: return "COMMAND";
        case MessageType::kCommandResponse: return "COMMAND_RESPONSE";
        case MessageType::kStatusReport: return "STATUS_REPORT";
        case MessageType::kLogMessage: return "LOG_MESSAGE";
        case MessageType::kErrorReport: return "ERROR_REPORT";
        case MessageType::kShutdown: return "SHUTDOWN";
        default: return "UNKNOWN";
    }
}

QString connectionStateToString(ConnectionState state) {
    switch (state) {
        case ConnectionState::kDisconnected: return "DISCONNECTED";
        case ConnectionState::kConnecting: return "CONNECTING";
        case ConnectionState::kConnected: return "CONNECTED";
        case ConnectionState::kAuthenticated: return "AUTHENTICATED";
        case ConnectionState::kError: return "ERROR";
        default: return "UNKNOWN";
    }
}

// ================== IpcContext 策略模式实现 ==================

IpcContext::IpcContext(QObject* parent) 
    : QObject(parent), m_strategy(nullptr), m_current_strategy_type("none") {
}

bool IpcContext::setIpcStrategy(std::unique_ptr<IIpcCommunication> strategy) {
    if (!strategy) {
        qWarning() << "Cannot set null strategy";
        return false;
    }
    
    QString old_type = m_current_strategy_type;
    
    // 断开旧策略的信号连接
    if (m_strategy) {
        disconnectStrategySignals();
        m_strategy->stop();
    }
    
    // 设置新策略
    m_strategy = std::move(strategy);
    m_current_strategy_type = "custom"; // 可以通过参数传入具体类型
    
    // 连接新策略的信号
    connectStrategySignals();
    
    emit strategyChanged(old_type, m_current_strategy_type, true);
    qDebug() << "IPC strategy changed from" << old_type << "to" << m_current_strategy_type;
    
    return true;
}

IIpcCommunication* IpcContext::getCurrentStrategy() const {
    return m_strategy.get();
}

QString IpcContext::getCurrentStrategyType() const {
    return m_current_strategy_type;
}

bool IpcContext::hasStrategy() const {
    return m_strategy != nullptr;
}

bool IpcContext::switchStrategy(IpcType type, const QJsonObject& config) {
    auto new_strategy = IpcCommunicationFactory::createIpcCommunication(type, config);
    if (!new_strategy) {
        qWarning() << "Failed to create strategy for type:" << IpcCommunicationFactory::getIpcTypeString(type);
        return false;
    }
    
    QString old_type = m_current_strategy_type;
    m_current_strategy_type = IpcCommunicationFactory::getIpcTypeString(type);
    
    return setIpcStrategy(std::move(new_strategy));
}

bool IpcContext::gracefulSwitchStrategy(IpcType type, const QJsonObject& config) {
    // 先停止当前策略
    if (m_strategy) {
        qDebug() << "Gracefully stopping current strategy:" << m_current_strategy_type;
        m_strategy->stop();
    }
    
    // 等待一小段时间确保资源释放
    QTimer::singleShot(100, [this, type, config]() {
        if (!switchStrategy(type, config)) {
            emit strategyChanged(m_current_strategy_type, 
                               IpcCommunicationFactory::getIpcTypeString(type), false);
        }
    });
    
    return true;
}

void IpcContext::connectStrategySignals() {
    if (!m_strategy) return;
    
    // 转发所有策略信号
    connect(m_strategy.get(), &IIpcCommunication::messageReceived,
            this, &IpcContext::messageReceived);
    connect(m_strategy.get(), &IIpcCommunication::clientConnected,
            this, &IpcContext::clientConnected);
    connect(m_strategy.get(), &IIpcCommunication::clientDisconnected,
            this, &IpcContext::clientDisconnected);
    connect(m_strategy.get(), &IIpcCommunication::connectionStateChanged,
            this, &IpcContext::connectionStateChanged);
    connect(m_strategy.get(), &IIpcCommunication::errorOccurred,
            this, &IpcContext::errorOccurred);
    connect(m_strategy.get(), &IIpcCommunication::topicSubscriptionChanged,
            this, &IpcContext::topicSubscriptionChanged);
}

void IpcContext::disconnectStrategySignals() {
    if (!m_strategy) return;
    
    // 断开所有信号连接
    disconnect(m_strategy.get(), nullptr, this, nullptr);
}

// === 代理所有IIpcCommunication接口方法 ===

bool IpcContext::initialize(const QJsonObject& config) {
    if (!m_strategy) {
        qWarning() << "No strategy set, cannot initialize";
        return false;
    }
    return m_strategy->initialize(config);
}

bool IpcContext::start() {
    if (!m_strategy) {
        qWarning() << "No strategy set, cannot start";
        return false;
    }
    return m_strategy->start();
}

void IpcContext::stop() {
    if (m_strategy) {
        m_strategy->stop();
    }
}

ConnectionState IpcContext::getConnectionState() const {
    if (!m_strategy) {
        return ConnectionState::kDisconnected;
    }
    return m_strategy->getConnectionState();
}

bool IpcContext::sendMessage(const IpcMessage& message) {
    if (!m_strategy) {
        qWarning() << "No strategy set, cannot send message";
        return false;
    }
    return m_strategy->sendMessage(message);
}

bool IpcContext::broadcastMessage(const IpcMessage& message) {
    if (!m_strategy) {
        qWarning() << "No strategy set, cannot broadcast message";
        return false;
    }
    return m_strategy->broadcastMessage(message);
}

bool IpcContext::publishToTopic(const QString& topic, const IpcMessage& message) {
    if (!m_strategy) {
        qWarning() << "No strategy set, cannot publish to topic";
        return false;
    }
    return m_strategy->publishToTopic(topic, message);
}

bool IpcContext::subscribeToTopic(const QString& topic) {
    if (!m_strategy) {
        qWarning() << "No strategy set, cannot subscribe to topic";
        return false;
    }
    return m_strategy->subscribeToTopic(topic);
}

bool IpcContext::unsubscribeFromTopic(const QString& topic) {
    if (!m_strategy) {
        qWarning() << "No strategy set, cannot unsubscribe from topic";
        return false;
    }
    return m_strategy->unsubscribeFromTopic(topic);
}

QStringList IpcContext::getSubscribedTopics() const {
    if (!m_strategy) {
        return QStringList();
    }
    return m_strategy->getSubscribedTopics();
}

int IpcContext::getConnectedClientCount() const {
    if (!m_strategy) {
        return 0;
    }
    return m_strategy->getConnectedClientCount();
}

QStringList IpcContext::getConnectedClientIds() const {
    if (!m_strategy) {
        return QStringList();
    }
    return m_strategy->getConnectedClientIds();
}

bool IpcContext::disconnectClient(const QString& client_id) {
    if (!m_strategy) {
        qWarning() << "No strategy set, cannot disconnect client";
        return false;
    }
    return m_strategy->disconnectClient(client_id);
}

bool IpcContext::isClientOnline(const QString& client_id) const {
    if (!m_strategy) {
        return false;
    }
    return m_strategy->isClientOnline(client_id);
}

QString IpcContext::getLastError() const {
    if (!m_strategy) {
        return "No strategy set";
    }
    return m_strategy->getLastError();
}

QString IpcContext::getClientIdBySenderId(const QString& sender_id) const {
    if (!m_strategy) {
        qDebug() << "[IpcContext] 没有策略，无法获取客户端ID";
        return QString();
    }
    return m_strategy->getClientIdBySenderId(sender_id);
}

bool IpcContext::sendMessage(const QString& client_id, const IpcMessage& message) {
    if (!m_strategy) {
        qWarning() << "No strategy set, cannot send message";
        return false;
    }
    return m_strategy->sendMessage(client_id, message);
}