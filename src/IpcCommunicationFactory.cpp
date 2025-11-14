#include "IIpcCommunication.h"
#include "LocalSocketIpcCommunication.h"
#include <QDebug>

// 静态成员初始化
QMap<IpcType, IpcCommunicationFactory::StrategyCreator> IpcCommunicationFactory::s_creators;

namespace {
    // 匿名命名空间用于自动注册
    struct LocalSocketRegistrar {
        LocalSocketRegistrar() {
            IpcCommunicationFactory::registerIpcType(
                IpcType::kLocalSocket,
                [](const QJsonObject& config) -> std::unique_ptr<IIpcCommunication> {
                    auto ipc = std::make_unique<LocalSocketIpcCommunication>();
                    if (!ipc->initialize(config)) {
                        qCritical() << "[IpcCommunicationFactory] LocalSocket IPC 初始化失败";
                        return nullptr;
                    }
                    return ipc;
                }
            );
            qDebug() << "[IpcCommunicationFactory] LocalSocket IPC 类型已注册";
        }
    };
    static LocalSocketRegistrar registrar;
}

std::unique_ptr<IIpcCommunication> IpcCommunicationFactory::createIpcCommunication(
    IpcType type, 
    const QJsonObject& config)
{
    if (s_creators.contains(type)) {
        return s_creators[type](config);
    }
    qWarning() << "[IpcCommunicationFactory] 未知的IPC类型:" << static_cast<int>(type);
    return nullptr;
}

bool IpcCommunicationFactory::registerIpcType(IpcType type, StrategyCreator creator)
{
    if (s_creators.contains(type)) {
        qWarning() << "[IpcCommunicationFactory] IPC类型" << static_cast<int>(type) << "已注册";
        return false;
    }
    s_creators[type] = std::move(creator);
    return true;
}

bool IpcCommunicationFactory::isTypeRegistered(IpcType type)
{
    return s_creators.contains(type);
}

QList<IpcType> IpcCommunicationFactory::getRegisteredTypes()
{
    return s_creators.keys();
}

IpcType IpcCommunicationFactory::getIpcTypeFromString(const QString& type_str)
{
    if (type_str == "LocalSocket") return IpcType::kLocalSocket;
    // 添加其他IPC类型的映射
    return IpcType::kLocalSocket; // 默认值或错误处理
}

QString IpcCommunicationFactory::getIpcTypeString(IpcType type)
{
    switch (type) {
        case IpcType::kLocalSocket: return "LocalSocket";
        // 添加其他IPC类型的映射
        default: return "Unknown";
    }
}
