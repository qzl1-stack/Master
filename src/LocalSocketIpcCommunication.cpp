#include "LocalSocketIpcCommunication.h"
#include <QCoreApplication>
#include <QDataStream>
#include <QDebug>
#include <QLocalSocket>
#include <QUuid>

LocalSocketIpcCommunication::LocalSocketIpcCommunication(QObject *parent)
    : IIpcCommunication(parent),
      local_server_(std::make_unique<QLocalServer>(this)),
      connection_state_(ConnectionState::kDisconnected) {
  qDebug() << "[LocalSocketIpcCommunication] 构造函数调用";
  connect(local_server_.get(), &QLocalServer::newConnection, this,
          &LocalSocketIpcCommunication::newConnection);
}

LocalSocketIpcCommunication::~LocalSocketIpcCommunication() {
  qDebug() << "[LocalSocketIpcCommunication] 析构函数调用";
  stop();
}

bool LocalSocketIpcCommunication::initialize(const QJsonObject &config) {
  QMutexLocker locker(&clients_mutex_);
  SetConnectionState(ConnectionState::kConnecting);

  // 从配置中获取服务器名称
  QJsonObject local_socket = config["local_socket"].toObject();
  server_name_ = local_socket["server_name"].toString();
  if (server_name_.isEmpty()) {
    SetLastError("初始化失败: 配置中缺少 'server_name'");
    qWarning()
        << "[LocalSocketIpcCommunication] 初始化失败: 配置中缺少 'server_name'";
    SetConnectionState(ConnectionState::kError);
    return false;
  }

  qDebug() << "[LocalSocketIpcCommunication] 初始化服务器名称:" << server_name_;
  SetConnectionState(ConnectionState::kInitialized); // 已初始化但未启动
  return true;
}

bool LocalSocketIpcCommunication::start() {
  QMutexLocker locker(&clients_mutex_);
  if (connection_state_ == ConnectionState::kConnected) {
    SetLastError("服务器已启动");
    return true;
  }

  // 移除之前的同名服务器（如果存在）
  QLocalServer::removeServer(server_name_);

  if (!local_server_->listen(server_name_)) {
    SetLastError(QString("启动失败: %1").arg(local_server_->errorString()));
    SetConnectionState(ConnectionState::kError);
    return false;
  }

  SetConnectionState(ConnectionState::kConnected);
  qDebug() << "[LocalSocketIpcCommunication] 服务器已启动，监听在:"
           << server_name_;
  return true;
}

void LocalSocketIpcCommunication::stop() {
  qDebug() << "[LocalSocketIpcCommunication] 停止服务器";
  if (connection_state_ == ConnectionState::kDisconnected) {
    qDebug() << "[LocalSocketIpcCommunication] 服务器已停止";
    return;
  }

  shutting_down_.store(true, std::memory_order_relaxed);

  // 停止服务器
  if (local_server_->isListening()) {
    local_server_->close();
    QLocalServer::removeServer(server_name_); // 确保移除服务器文件
  }

  // 断开所有客户端连接（快照后非持锁处理）
  std::vector<QLocalSocket *> sockets;
  {
    QMutexLocker locker(&clients_mutex_);
    qDebug() << "[LocalSocketIpcCommunication] 断开所有客户端连接";
    for (auto &kv : clients_) {
      sockets.push_back(kv.second.get());
    }
    clients_.clear();
    logical_to_internal_id_.clear();
    internal_to_logical_id_.clear();
    topic_subscriptions_.clear();
  }

  for (QLocalSocket *socket : sockets) {
    if (!socket)
      continue;
    QObject::disconnect(socket, nullptr, this, nullptr);
    if (socket->state() != QLocalSocket::UnconnectedState) {
      socket->disconnectFromServer();
      // 不等待，直接中断以避免阻塞
      if (socket->state() != QLocalSocket::UnconnectedState) {
        socket->abort();
      }
    }
    socket->deleteLater();
  }

  SetConnectionState(ConnectionState::kDisconnected);
  qDebug() << "[LocalSocketIpcCommunication] 服务器已停止";
}

ConnectionState LocalSocketIpcCommunication::getConnectionState() const {
  return connection_state_;
}

bool LocalSocketIpcCommunication::sendMessage(const IpcMessage &message) {
  QMutexLocker locker(&clients_mutex_);

  // 1. 将逻辑接收者ID转换为内部ID
  QString internal_receiver_id;
  if (logical_to_internal_id_.contains(message.receiver_id)) {
    internal_receiver_id = logical_to_internal_id_[message.receiver_id];
  } else {
    SetLastError(QString("发送消息失败: 逻辑客户端ID '%1' 未映射到内部ID")
                     .arg(message.receiver_id));
    return false;
  }

  if (clients_.find(internal_receiver_id) == clients_.end()) {
    SetLastError(QString("发送消息失败: 客户端 '%1' 不存在或未连接")
                     .arg(message.receiver_id));
    return false;
  }

  QLocalSocket *socket = clients_[internal_receiver_id].get();
  if (!socket || socket->state() != QLocalSocket::ConnectedState) {
    SetLastError(QString("发送消息失败: 客户端 '%1' 连接状态异常")
                     .arg(message.receiver_id));
    return false;
  }

  QByteArray block = message.toByteArray();

  block.append('\n');

  qint64 bytes_written = socket->write(block);
  if (bytes_written == -1 || bytes_written != block.size()) {
    SetLastError(QString("发送消息到 '%1' 失败: %2")
                     .arg(message.receiver_id)
                     .arg(socket->errorString()));
    return false;
  }
  socket->flush();
  return true;
}

bool LocalSocketIpcCommunication::sendMessage(const QString &client_id,
                                              const IpcMessage &message) {
  QMutexLocker locker(&clients_mutex_);
  if (clients_.find(client_id) == clients_.end()) {
    SetLastError(
        QString("发送消息失败: 客户端 '%1' 不存在或未连接").arg(client_id));
    return false;
  }

  QLocalSocket *socket = clients_[client_id].get();
  if (!socket || socket->state() != QLocalSocket::ConnectedState) {
    SetLastError(QString("发送消息失败: 客户端 '%1' 连接状态异常")
                     .arg(message.receiver_id));
    return false;
  }

  QByteArray block = message.toByteArray();

  block.append('\n');

  qint64 bytes_written = socket->write(block);
  if (bytes_written == -1 || bytes_written != block.size()) {
    SetLastError(QString("发送消息到 '%1' 失败: %2")
                     .arg(message.receiver_id)
                     .arg(socket->errorString()));
    return false;
  }
  socket->flush();
  // qDebug() << "[LocalSocketIpcCommunication] 消息发送成功到:" <<
  // message.receiver_id << "类型:" << static_cast<int>(message.type);
  return true;
}

bool LocalSocketIpcCommunication::broadcastMessage(const IpcMessage &message) {
  QMutexLocker locker(&clients_mutex_);
  if (clients_.empty()) {
    qWarning() << "[LocalSocketIpcCommunication] 广播消息: 没有连接的客户端";
    return true; // 没有客户端连接，也算成功发送（但不实际发送）
  }

  QByteArray block;
  QDataStream out(&block, QIODevice::WriteOnly);
  out.setVersion(QDataStream::Qt_6_0);
  out << message.toByteArray();

  bool all_success = true;
  for (auto it = clients_.begin(); it != clients_.end(); ++it) {
    QLocalSocket *socket = it->second.get();
    if (socket && socket->state() == QLocalSocket::ConnectedState) {
      qint64 bytes_written = socket->write(block);
      if (bytes_written == -1 || bytes_written != block.size()) {
        SetLastError(QString("广播消息到 '%1' 失败: %2")
                         .arg(it->first)
                         .arg(socket->errorString()));
        all_success = false;
      }
      socket->flush();
    }
  }
  qDebug() << "[LocalSocketIpcCommunication] 广播消息完成，类型:"
           << static_cast<int>(message.type);
  return all_success;
}

bool LocalSocketIpcCommunication::publishToTopic(const QString &topic,
                                                 const IpcMessage &message) {
  QMutexLocker locker(&clients_mutex_);
  if (!topic_subscriptions_.contains(topic) ||
      topic_subscriptions_[topic].isEmpty()) {
    qWarning() << "[LocalSocketIpcCommunication] 发布到Topic '" << topic
               << "': 没有订阅者";
    return true; // 没有订阅者，也算成功发布
  }

  QByteArray block;
  QDataStream out(&block, QIODevice::WriteOnly);
  out.setVersion(QDataStream::Qt_6_0);
  out << message.toByteArray();

  bool all_success = true;
  for (const QString &client_id : topic_subscriptions_[topic]) {
    if (clients_.find(client_id) != clients_.end()) {
      QLocalSocket *socket = clients_[client_id].get();
      if (socket && socket->state() == QLocalSocket::ConnectedState) {
        qint64 bytes_written = socket->write(block);
        if (bytes_written == -1 || bytes_written != block.size()) {
          SetLastError(QString("发布消息到Topic '%1' 给客户端 '%2' 失败: %3")
                           .arg(topic)
                           .arg(client_id)
                           .arg(socket->errorString()));
          all_success = false;
        }
        socket->flush();
      }
    }
  }
  qDebug() << "[LocalSocketIpcCommunication] 发布到Topic '" << topic
           << "' 完成，类型:" << static_cast<int>(message.type);
  return all_success;
}

bool LocalSocketIpcCommunication::subscribeToTopic(const QString &topic) {
  qDebug() << "[LocalSocketIpcCommunication] 订阅Topic:" << topic
           << " (服务器端操作)";
  // 如果需要动态添加订阅者，需要在消息处理逻辑中实现。
  return true;
}

bool LocalSocketIpcCommunication::unsubscribeFromTopic(const QString &topic) {
  qDebug() << "[LocalSocketIpcCommunication] 取消订阅Topic:" << topic
           << " (服务器端操作)";
  // 同上，实际应由接收到的消息驱动或在客户端断开时清理。
  return true;
}

QStringList LocalSocketIpcCommunication::getSubscribedTopics() const {
  QMutexLocker locker(&clients_mutex_);
  return topic_subscriptions_.keys();
}

int LocalSocketIpcCommunication::getConnectedClientCount() const {
  QMutexLocker locker(&clients_mutex_);
  return clients_.size();
}

QStringList LocalSocketIpcCommunication::getConnectedClientIds() const {
  QMutexLocker locker(&clients_mutex_);
  QStringList keys;
  for (const auto &pair : clients_) {
    keys.append(pair.first);
  }
  return keys;
}

bool LocalSocketIpcCommunication::disconnectClient(const QString &client_id) {
  QMutexLocker locker(&clients_mutex_);
  if (clients_.find(client_id) != clients_.end()) {
    clients_[client_id]->disconnectFromServer();
    return true;
  }
  SetLastError(QString("断开客户端失败: '%1' 不存在").arg(client_id));
  return false;
}

bool LocalSocketIpcCommunication::isClientOnline(
    const QString &client_id) const {
  QMutexLocker locker(&clients_mutex_);
  auto it = clients_.find(client_id);
  return it != clients_.end() &&
         it->second->state() == QLocalSocket::ConnectedState;
}

QString LocalSocketIpcCommunication::getLastError() const {
  return last_error_;
}

void LocalSocketIpcCommunication::newConnection() {
  QMutexLocker locker(&clients_mutex_);
  while (local_server_->hasPendingConnections()) {
    std::unique_ptr<QLocalSocket> client_socket(
        local_server_->nextPendingConnection());
    if (client_socket) {
      QString client_id = QUuid::createUuid().toString(
          QUuid::WithoutBraces); // 为每个客户端生成唯一ID
      qDebug() << "[LocalSocketIpcCommunication] 新的IPC连接:" << client_id;

      connect(client_socket.get(), &QLocalSocket::disconnected, this,
              &LocalSocketIpcCommunication::socketDisconnected);
      connect(client_socket.get(), &QLocalSocket::readyRead, this,
              &LocalSocketIpcCommunication::readyRead);
      connect(client_socket.get(),
              QOverload<QLocalSocket::LocalSocketError>::of(
                  &QLocalSocket::errorOccurred),
              this, &LocalSocketIpcCommunication::socketError);

      clients_[client_id] = std::move(client_socket);
      emit clientConnected(client_id);
    }
  }
}

void LocalSocketIpcCommunication::socketDisconnected() {
  if (shutting_down_.load(std::memory_order_relaxed))
    return;
  // No need to lock here because RemoveClient will handle locking.
  QLocalSocket *sender_socket = qobject_cast<QLocalSocket *>(sender());
  if (!sender_socket)
    return;

  QString client_id = GetClientId(sender_socket);
  if (!client_id.isEmpty()) {
    qDebug() << "[LocalSocketIpcCommunication] IPC连接断开:" << client_id;
    RemoveClient(sender_socket);
    emit clientDisconnected(client_id);
  }
}

void LocalSocketIpcCommunication::readyRead() {
  if (shutting_down_.load(std::memory_order_relaxed))
    return;
  QLocalSocket *sender_socket = qobject_cast<QLocalSocket *>(sender());
  if (!sender_socket)
    return;

  QByteArray data = sender_socket->readAll();
  receive_buffers_[sender_socket].append(data);

  QByteArray &buffer = receive_buffers_[sender_socket];
  while (buffer.contains('\n')) {
    int newline_pos = buffer.indexOf('\n');
    QByteArray message_data = buffer.left(newline_pos);
    buffer.remove(0, newline_pos + 1);

    if (!message_data.isEmpty()) {
      // qDebug() << "[LocalSocketIpcCommunication] Processing complete
      // message:" << message_data;
      IpcMessage message = IpcMessage::fromByteArray(message_data);

      // 建立ID映射
      establishIdMapping(sender_socket, message);

      // 处理订阅和取消订阅消息
      if (message.type == MessageType::kCommand &&
          (message.topic == "subscribe_topic" ||
           message.topic == "unsubscribe_topic")) {
        handleSubscriptionMessage(message);
      }
      // 可以在这里添加其他消息类型的处理逻辑，例如使用switch-case或更复杂的策略模式

      emit messageReceived(message); // 确保所有消息都发出这个信号
    }
  }
}

void LocalSocketIpcCommunication::establishIdMapping(
    QLocalSocket *socket, const IpcMessage &message) {
  QString internal_id = GetClientId(socket);
  if (!internal_id.isEmpty() && !message.sender_id.isEmpty()) {
    QMutexLocker locker(&clients_mutex_);
    if (!logical_to_internal_id_.contains(message.sender_id)) {
      qDebug() << "[LocalSocketIpcCommunication] 建立新的ID映射: "
               << message.sender_id << "->" << internal_id;
      logical_to_internal_id_[message.sender_id] = internal_id;
      internal_to_logical_id_[internal_id] = message.sender_id;
    }
  }
}

void LocalSocketIpcCommunication::handleSubscriptionMessage(
    const IpcMessage &message) {
  // 这个函数只处理订阅和取消订阅消息
  if (message.type == MessageType::kCommand &&
      message.topic == "subscribe_topic") {
    QString topic_to_subscribe = message.body["topic"].toString();
    QString client_id = message.sender_id; // 假设sender_id是客户端ID
    if (!topic_to_subscribe.isEmpty() && !client_id.isEmpty()) {
      QMutexLocker locker(&clients_mutex_);
      if (!topic_subscriptions_[topic_to_subscribe].contains(client_id)) {
        topic_subscriptions_[topic_to_subscribe].append(client_id);
        qDebug() << "[LocalSocketIpcCommunication] 客户端 '" << client_id
                 << "' 订阅Topic:" << topic_to_subscribe;
        emit topicSubscriptionChanged(topic_to_subscribe, true);
      }
    }
  } else if (message.type == MessageType::kCommand &&
             message.topic == "unsubscribe_topic") {
    QString topic_to_unsubscribe = message.body["topic"].toString();
    QString client_id = message.sender_id; // 假设sender_id是客户端ID
    if (!topic_to_unsubscribe.isEmpty() && !client_id.isEmpty()) {
      QMutexLocker locker(&clients_mutex_);
      topic_subscriptions_[topic_to_unsubscribe].removeOne(client_id);
      qDebug() << "[LocalSocketIpcCommunication] 客户端 '" << client_id
               << "' 取消订阅Topic:" << topic_to_unsubscribe;
      emit topicSubscriptionChanged(topic_to_unsubscribe, false);
    }
  }
}

void LocalSocketIpcCommunication::socketError(
    QLocalSocket::LocalSocketError socket_error) {
  if (shutting_down_.load(std::memory_order_relaxed))
    return;
  QLocalSocket *sender_socket = qobject_cast<QLocalSocket *>(sender());
  if (!sender_socket)
    return;

  QString client_id = GetClientId(sender_socket);
  QString error_message =
      QString("Socket错误: %1").arg(sender_socket->errorString());
  SetLastError(error_message);
  qWarning() << "[LocalSocketIpcCommunication] 客户端 '" << client_id
             << "' 发生错误: " << error_message;
  emit errorOccurred(error_message);
}

void LocalSocketIpcCommunication::SetConnectionState(ConnectionState state) {
  if (connection_state_ != state) {
    connection_state_ = state;
    emit connectionStateChanged(state);
  }
}

void LocalSocketIpcCommunication::SetLastError(const QString &error) {
  last_error_ = error;
  emit errorOccurred(error);
}

QString LocalSocketIpcCommunication::GetClientId(QLocalSocket *socket) const {
  QMutexLocker locker(&clients_mutex_);
  for (auto it = clients_.begin(); it != clients_.end(); ++it) {
    if (it->second.get() == socket) {
      return it->first;
    }
  }
  return QString();
}

void LocalSocketIpcCommunication::RemoveClient(QLocalSocket *socket) {
  QMutexLocker locker(&clients_mutex_);
  QString client_id_to_remove;
  for (auto it = clients_.begin(); it != clients_.end(); ++it) {
    if (it->second.get() == socket) {
      client_id_to_remove = it->first;
      break;
    }
  }

  if (!client_id_to_remove.isEmpty()) {
    clients_.erase(client_id_to_remove);

    // --- 新增代码：清理ID映射 ---
    if (internal_to_logical_id_.contains(client_id_to_remove)) {
      QString logical_id = internal_to_logical_id_.take(client_id_to_remove);
      logical_to_internal_id_.remove(logical_id);
      qDebug() << "[LocalSocketIpcCommunication] 清理ID映射: " << logical_id
               << "->" << client_id_to_remove;
    }
    // --- 新增代码结束 ---

    // 清理所有订阅中包含此客户端ID的Topic
    for (auto it = topic_subscriptions_.begin();
         it != topic_subscriptions_.end(); ++it) {
      it.value().removeOne(client_id_to_remove);
      // 如果Topic没有订阅者了，可以考虑移除该Topic
      if (it.value().isEmpty()) {
        // Optionally, remove the topic itself if no subscribers remain
        // topic_subscriptions_.erase(it);
      }
    }
  }
}

QString LocalSocketIpcCommunication::getClientIdBySenderId(
    const QString &sender_id) const {
  QMutexLocker locker(&clients_mutex_);

  if (logical_to_internal_id_.contains(sender_id)) {
    return logical_to_internal_id_.value(sender_id);
  }

  qDebug() << "[LocalSocketIpcCommunication] 未找到 " << sender_id
           << " 对应的内部客户端ID";
  return QString();
}
