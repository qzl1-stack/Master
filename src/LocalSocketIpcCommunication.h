#ifndef MASTER_SRC_LOCALSOCKETIPCCOMMUNICATION_H_
#define MASTER_SRC_LOCALSOCKETIPCCOMMUNICATION_H_

#include "IIpcCommunication.h"
#include <QLocalServer>
#include <QLocalSocket>
#include <QMap>
#include <memory>
#include <QMutex>
#include <map>
#include <atomic>

/**
 * @brief 基于QLocalSocket的IPC通信实现
 *
 * 该类实现了IIpcCommunication接口，使用Qt的QLocalServer和QLocalSocket
 * 进行本地进程间通信。
 */
class LocalSocketIpcCommunication : public IIpcCommunication {
    Q_OBJECT

public:
    explicit LocalSocketIpcCommunication(QObject* parent = nullptr);
    ~LocalSocketIpcCommunication() override;

    bool initialize(const QJsonObject& config) override;
    bool start() override;
    void stop() override;
    ConnectionState getConnectionState() const override;
    bool sendMessage(const IpcMessage& message) override;
    bool sendMessage(const QString& client_id, const IpcMessage& message);
    bool broadcastMessage(const IpcMessage& message) override;
    bool publishToTopic(const QString& topic, const IpcMessage& message) override;
    bool subscribeToTopic(const QString& topic) override;
    bool unsubscribeFromTopic(const QString& topic) override;
    QStringList getSubscribedTopics() const override;
    int getConnectedClientCount() const override;
    QStringList getConnectedClientIds() const override;
    bool disconnectClient(const QString& client_id) override;
    bool isClientOnline(const QString& client_id) const override;
    QString getLastError() const override;
    QString getClientIdBySenderId(const QString& sender_id) const override;

private slots:
    void newConnection();
    void socketDisconnected();
    void readyRead();
    void socketError(QLocalSocket::LocalSocketError socket_error);

private:
    std::unique_ptr<QLocalServer> local_server_;
    std::map<QString, std::unique_ptr<QLocalSocket>> clients_; // 以客户端ID为键
    mutable QMutex clients_mutex_; // 保护clients_的访问

    QString server_name_;
    ConnectionState connection_state_;
    QString last_error_;
    QMap<QString, QList<QString>> topic_subscriptions_; // Topic到客户端ID列表的映射
    QHash<QLocalSocket*, QByteArray> receive_buffers_;// 使用 QHash 来为每一个连接的客户端维护一个独立的接收缓冲区
    QMap<QString, QString> logical_to_internal_id_; // 逻辑ID -> 内部UUID
    QMap<QString, QString> internal_to_logical_id_; // 内部UUID -> 逻辑ID
    std::atomic_bool shutting_down_{false};
    
    //逻辑ID：是子进程生成的一个唯一ID，用于标识子进程
    //内部ID：是Master生成的一个唯一ID，用于标识Master与子进程的连接的client的映射
    
    void SetConnectionState(ConnectionState state);
    void SetLastError(const QString& error);
    QString GetClientId(QLocalSocket* socket) const;
    void RemoveClient(QLocalSocket* socket);
    void establishIdMapping(QLocalSocket* socket, const IpcMessage& message);
    void handleSubscriptionMessage(const IpcMessage& message);
    
};

#endif // MASTER_SRC_LOCALSOCKETIPCCOMMUNICATION_H_
