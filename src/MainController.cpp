#include "MainController.h"
#include "ProcessManager.h"
#include "ProjectConfig.h"
#include "DataStore.h"
#include "ILogStorage.h"
#include "IIpcCommunication.h"
#include "update_checker.h"
#include <QUuid>
#include <QFile>
#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QThread>
#include <QJsonArray>
#include <QtConcurrent/QtConcurrent>

#ifdef Q_OS_WIN
#include <windows.h>
#endif


// 静态成员初始化
std::unique_ptr<MainController> MainController::instance_ = nullptr;
QMutex MainController::instance_mutex_;

MainController::MainController(QObject* parent)
    : QObject(parent)
    , process_manager_(nullptr)
    , update_checker_(nullptr)
    , initialization_state_(kNotInitialized)
    , system_status_(kSystemIdle)
    , is_system_healthy_(false)
    , startup_time_(QDateTime::currentDateTime())
    , health_check_interval_ms_(5000)      // 默认5秒健康检查
    , statistics_update_interval_ms_(10000) // 默认10秒统计更新
{
    qDebug() << "[MainController] 构造函数调用";
    
    // 初始化定时器
    health_check_timer_ = std::make_unique<QTimer>(this);
    statistics_timer_ = std::make_unique<QTimer>(this);
    update_checker_ = std::make_unique<UpdateChecker>(this);
    
    // 连接定时器信号
    connect(health_check_timer_.get(), &QTimer::timeout, 
            this, &MainController::PerformSystemHealthCheck);
    connect(statistics_timer_.get(), &QTimer::timeout,
            this, &MainController::UpdateSystemStatistics);
    
    // 初始化工作区历史记录文件路径
    QString app_data_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(app_data_dir);
    workspace_history_file_path_ = app_data_dir + "/workspace_history.json";
    
    // 加载工作区历史记录
    LoadWorkspaceHistory();
}

MainController::~MainController()
{
    qDebug() << "[MainController] 析构函数调用";
    
    // 停止系统（如果还在运行）
    // if (initialization_state_ == kStarted) {
        // Stop();
    // }
    
    // 清理资源
    CleanupSystemResources();
}

MainController& MainController::GetInstance()
{
    QMutexLocker locker(&instance_mutex_);
    if (!instance_) {
        instance_ = std::unique_ptr<MainController>(new MainController());
    }
    return *instance_;
}

bool MainController::Initialize(const QString& config_file_path)
{
    QMutexLocker locker(&state_mutex_);
    
    if (initialization_state_ != kNotInitialized) {
        qWarning() << "[MainController] 系统已经初始化，当前状态:" << initialization_state_;
        return initialization_state_ == kInitialized || initialization_state_ == kStarted;
    }
    
    qDebug() << "[MainController] 开始系统初始化";

    try {
        // 设置配置文件路径
        if (config_file_path.isEmpty()) {
            // 获取可执行文件所在目录
            QString exe_dir = QCoreApplication::applicationDirPath();
            QDir config_dir(exe_dir + "/Config");
            if (!config_dir.exists()) {
                config_dir.mkpath(".");
            }
            current_config_file_path_ = config_dir.filePath("config.json");
        } else {
            current_config_file_path_ = config_file_path;
        }

    

        // 初始化核心模块
        if (!InitializeCoreModules()) {
            HandleSystemError("核心模块初始化失败", true);
            UpdateInitializationState(kError); // 错误状态
            return false;
        }

        // 核心模块初始化完成后，再更新状态
        UpdateInitializationState(kInitializing);
        
        // 检查模块依赖
        if (!CheckModuleDependencies()) {
            HandleSystemError("模块依赖检查失败", true);
            return false;
        }
        
        // 同步配置到DataStore
        SyncConfigurationToDataStore();
        
        // 连接模块间信号槽
        ConnectModuleSignals();
        
        UpdateInitializationState(kInitialized);
        is_system_healthy_ = true;
        
        qDebug() << "[MainController] 系统初始化完成";
        return true;
        
    } catch (const std::exception& e) {
        HandleSystemError(QString("初始化异常: %1").arg(e.what()), true);
        return false;
    }
}

bool MainController::Start()
{
    QMutexLocker locker(&state_mutex_);
    
    if (initialization_state_ != kInitialized) {
        qWarning() << "[MainController] 系统未初始化，无法启动";
        return false;
    }
    
    try {
        // 1. 启动IPC服务端监听
        if (ipc_context_) {
            if (!ipc_context_->start()) {
                qWarning() << "[MainController] IPC服务启动失败";
                return false;
            }
            qDebug() << "[MainController] IPC服务启动成功";
        }
        
        // 2. 启动日志聚合服务
        if (log_aggregator_) {
            if (!log_aggregator_->startAllStorages()) {
                qWarning() << "[MainController] 日志聚合服务启动失败";
                return false;
            }
            qDebug() << "[MainController] 日志聚合服务启动成功";
        }
        
        // 3. 初始化进程管理器
        if (process_manager_) {
            if (!process_manager_->Initialize()) {
                qWarning() << "[MainController] 进程管理器初始化失败";
                return false;
            }
            qDebug() << "[MainController] 进程管理器初始化成功";
        }
        
        
        // 5. 启动系统监控
        StartSystemMonitoring();
        
        // 6. 启动更新检查
        if (update_checker_) {
            qDebug() << "[MainController] 启动自动更新检查";
            update_checker_->startAutoUpdateCheck();
        }
        
        // 7. 更新系统状态
        UpdateInitializationState(kStarted);
        UpdateSystemStatus(kSystemRunning);
        
        qDebug() << "[MainController] 系统启动完成";
        return true;
        
    } catch (const std::exception& e) {
        HandleSystemError(QString("启动异常: %1").arg(e.what()), true);
        return false;
    }
}

bool MainController::Stop(int timeout_ms)
{
    QMutexLocker locker(&state_mutex_);
    
    if (initialization_state_ != kStarted) {
        qDebug() << "[MainController] 系统未启动，无需停止";
        return true;
    }
    
    qDebug() << "[MainController] 开始停止系统服务";
    UpdateInitializationState(kStopping);
    UpdateSystemStatus(kSystemMaintenance);
    
    try {
        // 1. 停止系统监控
        StopSystemMonitoring();

        // 1.1 优雅关闭提示：先通知子进程自行退出（通过IPC），给予短暂宽限
        if (ipc_context_ && process_manager_) {
            QJsonObject params; // 可按需添加上下文
            qDebug() << "[MainController] 广播优雅退出指令给子进程";
            BroadcastCommand("graceful_shutdown", params);
        }

        QThread::msleep(200);

        // 2. 停止IPC服务
        if (ipc_context_)
        {
            ipc_context_->stop();
            qDebug() << "[MainController] IPC服务已停止";
        }

        // 3. 停止日志聚合服务
        if (log_aggregator_)
        {
            log_aggregator_->stopAllStorages();
            qDebug() << "[MainController] 日志聚合服务已停止";
        }

        // 4. 保存最终状态
        if (data_store_)
        {
            // 保存关键状态信息
            data_store_->setValue("system.shutdown_time", QDateTime::currentDateTime());
            data_store_->setValue("system.shutdown_reason", "normal");
        }

        // 5. 停止所有子进程（作为兜底）
        if (process_manager_)
        {
            if (!process_manager_->StopAllProcesses(timeout_ms / 2)) {
                qWarning() << "[MainController] 部分子进程停止超时";
            }
        }
        qDebug() << "[MainController] 所有子进程已停止";

        
        UpdateInitializationState(kStopped);
        UpdateSystemStatus(kSystemIdle);
        
        qDebug() << "[MainController] 系统停止完成";
        return true;
        
    } catch (const std::exception& e) {
        HandleSystemError(QString("停止异常: %1").arg(e.what()), false);
        return false;
    }
}

bool MainController::Restart(const QString& config_file_path)
{
    qDebug() << "[MainController] 开始重启系统";
    
    // 先停止
    if (!Stop()) {
        qWarning() << "[MainController] 停止系统失败，重启中止";
        return false;
    }
    
    // 等待一秒确保资源清理完成
    QThread::sleep(1);
    
    // 重新初始化
    {
        QMutexLocker locker(&state_mutex_);
        initialization_state_ = kNotInitialized;
        system_status_ = kSystemIdle;
    }
    
    // 初始化并启动
    if (!Initialize(config_file_path)) {
        qWarning() << "[MainController] 重新初始化失败";
        return false;
    }
    
    if (!Start()) {
        qWarning() << "[MainController] 重新启动失败";
        return false;
    }
    
    qDebug() << "[MainController] 系统重启完成";
    return true;
}

MainController::InitializationState MainController::GetInitializationState() const
{
    QMutexLocker locker(&state_mutex_);
    return initialization_state_;
}

MainController::SystemStatus MainController::GetSystemStatus() const
{
    QMutexLocker locker(&state_mutex_);
    return system_status_;
}

bool MainController::IsSystemHealthy() const
{
    QMutexLocker locker(&state_mutex_);
    return is_system_healthy_;
}

QJsonObject MainController::GetSystemStatistics() const
{
    QMutexLocker stat_locker(&statistics_mutex_);
    QMutexLocker state_locker(&state_mutex_);
    
    QJsonObject stats;
    stats["initialization_state"] = static_cast<int>(initialization_state_);
    stats["system_status"] = static_cast<int>(system_status_);
    stats["is_healthy"] = is_system_healthy_;
    stats["startup_time"] = startup_time_.toString(Qt::ISODate);
    stats["uptime_seconds"] = startup_time_.secsTo(QDateTime::currentDateTime());
    stats["total_messages_processed"] = static_cast<qint64>(system_statistics_.total_messages_processed);
    stats["total_commands_executed"] = static_cast<qint64>(system_statistics_.total_commands_executed);
    stats["total_config_updates"] = static_cast<qint64>(system_statistics_.total_config_updates);
    stats["total_process_restarts"] = static_cast<qint64>(system_statistics_.total_process_restarts);
    stats["last_statistics_update"] = system_statistics_.last_statistics_update.toString(Qt::ISODate);
    
    // 添加模块状态
    QJsonObject modules;
    modules["process_manager"] = (process_manager_ != nullptr);
    modules["project_config"] = (project_config_ != nullptr);
    modules["data_store"] = (data_store_ != nullptr);
    modules["log_aggregator"] = (log_aggregator_ != nullptr);
    modules["ipc_context"] = (ipc_context_ != nullptr);
    stats["modules"] = modules;
    
    return stats;
}

// ==================== 模块访问接口 ====================

ProcessManager* MainController::GetProcessManager() const
{
    return process_manager_;
}

ProjectConfig* MainController::GetProjectConfig() const
{
    return project_config_;
}

DataStore* MainController::GetDataStore() const
{
    return data_store_;
}

LogAggregator* MainController::GetLogAggregator() const
{
    return log_aggregator_.get();
}

IpcContext* MainController::GetIpcContext() const
{
    return ipc_context_.get();
}

UpdateChecker* MainController::GetUpdateChecker() const
{
    return update_checker_.get();
}

// ==================== 业务流程接口 ====================

bool MainController::StartSubProcess(const QString& process_id, bool force_restart)
{
    if (!process_manager_) {
        qWarning() << "[MainController] ProcessManager未初始化";
        return false;
    }
    
    // 如果需要强制重启且进程正在运行，先停止它
    if (force_restart && process_manager_->GetProcessStatus(process_id) == ProcessManager::kRunning) {
        if (!process_manager_->StopProcess(process_id)) {
            qWarning() << "[MainController] 停止进程失败:" << process_id;
            return false;
        }
    }
    
    // 从配置中获取进程启动参数
    if (!project_config_) {
        qWarning() << "[MainController] ProjectConfig未初始化";
        return false;
    }
    
    QJsonObject process_config = project_config_->getConfigValue("processes").toObject();
    QJsonObject process_config_item = process_config[process_id].toObject();
    if (process_config_item.isEmpty()) {
        qWarning() << "[MainController] 未找到进程配置:" << process_id;
        return false;
    }
    
    QString executable = process_config_item["executable"].toString();
    QStringList arguments;
    QJsonArray args_array = process_config_item["arguments"].toArray();
    for (const auto& arg : args_array) {
        arguments.append(arg.toString());
    }
    QString working_dir = process_config_item["working_directory"].toString();
    bool auto_restart = process_config_item["auto_restart"].toBool(true);
    
    bool success = process_manager_->StartProcess(process_id, executable, arguments, working_dir, auto_restart);
    
    if (success) {
        // 更新统计信息
        QMutexLocker locker(&statistics_mutex_);
        // 这里可能需要统计启动次数
        
        qDebug() << "[MainController] 子进程启动成功:" << process_id;
    } else {
        qWarning() << "[MainController] 子进程启动失败:" << process_id;
    }
    
    return success;
}

bool MainController::StopSubProcess(const QString& process_id, int timeout_ms)
{
    if (!process_manager_) {
        qWarning() << "[MainController] ProcessManager未初始化";
        return false;
    }
    
    bool success = process_manager_->StopProcess(process_id, false, timeout_ms);
    
    if (success) {
        qDebug() << "[MainController] 子进程停止成功:" << process_id;
    } else {
        qWarning() << "[MainController] 子进程停止失败:" << process_id;
    }
    
    return success;
}

bool MainController::RestartSubProcess(const QString& process_id)
{
    if (!process_manager_) {
        qWarning() << "[MainController] ProcessManager未初始化";
        return false;
    }
    
    bool success = process_manager_->RestartProcess(process_id);
    
    if (success) {
        // 更新统计信息
        QMutexLocker locker(&statistics_mutex_);
        system_statistics_.total_process_restarts++;
        
        qDebug() << "[MainController] 子进程重启成功:" << process_id;
    } else {
        qWarning() << "[MainController] 子进程重启失败:" << process_id;
    }
    
    return success;
}

QJsonObject MainController::GetAllProcessInfo() const
{
    QJsonObject status_obj;
    
    if (!process_manager_) {
        status_obj["error"] = "ProcessManager未初始化";
        return status_obj;
    }
    
    QStringList process_list = process_manager_->GetProcessList();
    
    for (const QString& process_id : process_list) {
        const ProcessManager::ProcessInfo* info = process_manager_->GetProcessInfo(process_id);
        if (info) {
            QJsonObject process_info;
            process_info["status"] = static_cast<int>(info->status);
            process_info["start_time"] = info->start_time.toString(Qt::ISODate);
            process_info["restart_count"] = info->restart_count;
            process_info["auto_restart"] = info->auto_restart;
            process_info["executable_path"] = info->executable_path;
            process_info["working_directory"] = info->working_directory;
            
            // 添加参数数组
            QJsonArray args_array;
            for (const QString& arg : info->arguments) {
                args_array.append(arg);
            }
            process_info["arguments"] = args_array;
            
            status_obj[process_id] = process_info;
        }
    }
    
    return status_obj;
}

QVariantList MainController::GetConfiguredProcessNames() const
{
    QVariantList process_names_list;
    if (project_config_) {
        QStringList names = project_config_->getProcessList();
        for (const QString& name : names) {
            process_names_list.append(name);
        }
    }
    return process_names_list;
}

// ==================== 窗口嵌入相关方法实现 ====================

bool MainController::EmbedProcessWindow(const QString& process_id, QObject* container_item)
{
    if (!container_item) {
        qWarning() << "[MainController] EmbedProcessWindow: 容器项无效";
        return false;
    }

    QQuickItem* item = qobject_cast<QQuickItem*>(container_item);
    if (!item) {
        qWarning() << "[MainController] EmbedProcessWindow: 无法将 QObject 转换为 QQuickItem";
        return false;
    }

    QQuickWindow* window = item->window();
    if (!window) {
        qWarning() << "[MainController] EmbedProcessWindow: 无法从容器项获取 QQuickWindow";
        return false;
    }

    const qulonglong container_window_id = window->winId();
    const qreal dpr = window->devicePixelRatio();
    
    // 修正：使用 mapToItem(nullptr, ...) 获取相对于顶层窗口的坐标
    const QPointF window_pos = item->mapToItem(nullptr, QPointF(0, 0));
    const QRect geometry(qRound(window_pos.x() * dpr),
                        qRound(window_pos.y() * dpr),
                        qRound(item->width() * dpr),
                        qRound(item->height() * dpr));

    qDebug() << "[MainController] 容器几何信息 - 位置:(" << window_pos.x() << "," << window_pos.y() 
             << ") 尺寸:(" << item->width() << "x" << item->height() 
             << ") DPR:" << dpr << " 最终几何:" << geometry;

    if (process_id.isEmpty() || container_window_id == 0) {
        qWarning() << "[MainController] EmbedProcessWindow: 参数无效 - process_id:" 
                   << process_id << "container_window_id:" << container_window_id;
        return false;
    }
    
    if (!process_manager_) {
        qWarning() << "[MainController] EmbedProcessWindow: ProcessManager 未初始化";
        return false;
    }
    
    // 获取进程信息
    auto process_list = process_manager_->GetProcessList();
    bool process_found = false;
    for (const QString& pid : process_list) {
        if (pid == process_id) {
            process_found = true;
            break;
        }
    }
    
    if (!process_found) {
        qWarning() << "[MainController] EmbedProcessWindow: 进程不存在:" << process_id;
        return false;
    }
    
    
    auto process_info = process_manager_->GetProcessInfo(process_id);
    int status = process_info->status;
    if (status != static_cast<int>(ProcessManager::kStarting)) {
        qWarning() << "[MainController] EmbedProcessWindow: 进程未运行，状态:" << status;
        return false;
    }
    
    // 获取进程的窗口句柄并嵌入
    // 注意：这里需要平台特定的实现
    bool success = EmbedProcessWindowImpl(process_id, container_window_id, geometry);
    
    if (success) {
        qInfo() << "[MainController] 成功嵌入进程窗口:" << process_id 
                << "到容器:" << container_window_id << "几何:" << geometry;
    } else {
        qWarning() << "[MainController] 嵌入进程窗口失败:" << process_id;
    }
    
    return success;
}

bool MainController::UpdateEmbeddedWindowGeometry(const QString& process_id, QObject* container_item)
{
    if (!container_item) {
        qWarning() << "[MainController] UpdateEmbeddedWindowGeometry: 容器项无效";
        return false;
    }

    QQuickItem* item = qobject_cast<QQuickItem*>(container_item);
    if (!item) {
        qWarning() << "[MainController] UpdateEmbeddedWindowGeometry: 无法将 QObject 转换为 QQuickItem";
        return false;
    }

    QQuickWindow* window = item->window();
    if (!window) {
        qWarning() << "[MainController] UpdateEmbeddedWindowGeometry: 无法从容器项获取 QQuickWindow";
        return false;
    }

    const qreal dpr = window->devicePixelRatio();
    const QPointF window_pos = item->mapToItem(nullptr, QPointF(0, 0));
    const QRect geometry(qRound(window_pos.x() * dpr),
                        qRound(window_pos.y() * dpr),
                        qRound(item->width() * dpr),
                        qRound(item->height() * dpr));

    // 查找子进程的主窗口
    qulonglong child_window_handle = FindProcessMainWindow(process_id);
    if (child_window_handle == 0) {
        qWarning() << "[MainController] UpdateEmbeddedWindowGeometry: 无法找到进程窗口:" << process_id;
        return false;
    }

#ifdef Q_OS_WIN
    HWND childHwnd = reinterpret_cast<HWND>(child_window_handle);
    
    // 更新窗口位置和尺寸
    BOOL result = SetWindowPos(childHwnd, nullptr, 
                               geometry.x(), geometry.y(),
                               geometry.width(), geometry.height(),
                               SWP_NOZORDER | SWP_NOACTIVATE);
    
    if (!result) {
        DWORD error = GetLastError();
        qWarning() << "[MainController] UpdateEmbeddedWindowGeometry: SetWindowPos 失败，错误码:" << error;
        return false;
    }
    
    qDebug() << "[MainController] 更新嵌入窗口几何:" << process_id << "新几何:" << geometry;
    return true;
#else
    Q_UNUSED(child_window_handle)
    qWarning() << "[MainController] 当前平台不支持更新嵌入窗口几何";
    return false;
#endif
}

void MainController::startEmbeddingProcess(const QString& process_name)
{
    QMutexLocker locker(&embedding_mutex_);
    qDebug() << "[MainController] 开始窗口嵌入过程 for" << process_name;
    embedding_in_progress_[process_name] = true;
    embedding_cancelled_.remove(process_name);
}

void MainController::finishEmbeddingProcess(const QString& process_name)
{
    QMutexLocker locker(&embedding_mutex_);
    qDebug() << "[MainController] 结束窗口嵌入过程 for" << process_name;
    embedding_in_progress_.remove(process_name);
    embedding_cancelled_.remove(process_name);
}


/**
 * @brief 发送命令到指定进程并等待响应
 * @param process_id 目标进程ID
 * @param command 命令字符串
 * @param parameters 命令参数
 * @param timeout_ms 响应超时时间（毫秒）
 * @return 命令响应结果的JSON对象
 */
QJsonObject MainController::SendCommandToProcess(const QString& process_id, 
                                                const QString& command, 
                                                const QJsonObject& parameters,
                                                int timeout_ms)
{
    QJsonObject response;
    response["success"] = false;
    response["process_id"] = process_id;
    response["command"] = command;
    response["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    if (!ipc_context_) {
        response["error"] = "IPC未初始化";
        return response;
    }
    
    // 1. 根据 command 字符串确定 IpcMessage::MessageType
    MessageType msg_type;
    if (command == "config_update") {
        msg_type = MessageType::kConfigUpdate;
    } else if (command == "select_ip") {
        msg_type = MessageType::kCommand;
    } else {
        // 默认为通用命令
        msg_type = MessageType::kCommand;
    }

    // 2. 构建IPC消息
    IpcMessage ipc_msg;
    ipc_msg.type = msg_type;
    ipc_msg.topic = "MainController";
    ipc_msg.msg_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    ipc_msg.timestamp = QDateTime::currentMSecsSinceEpoch();
    ipc_msg.sender_id = "MainController";
    ipc_msg.receiver_id = process_id;
    ipc_msg.body = parameters;
    if (ipc_context_) {
        if (!ipc_context_->sendMessage(ipc_msg)) {
            qWarning() << "[MainController] 发送命令失败到:" << ipc_msg.receiver_id;
        }
    }
    
    // 更新统计信息
    QMutexLocker locker(&statistics_mutex_);
    system_statistics_.total_commands_executed++;

    response["success"] = true;  // 临时设置
    response["message"] = "命令已发送";
    return response;
}

QJsonObject MainController::BroadcastCommand(const QString& command, 
                                            const QJsonObject& parameters)
{
    QJsonObject response;
    response["success"] = false;
    response["command"] = command;
    response["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    if (!ipc_context_) {
        response["error"] = "IPC未初始化";
        return response;
    }
    
    if (!process_manager_) {
        response["error"] = "ProcessManager未初始化";
        return response;
    }
    
    QStringList running_processes = process_manager_->GetRunningProcessList();
    qDebug() << "[MainController] 广播命令到运行中的进程:" << running_processes;
    QJsonObject process_responses;
    int success_count = 0;
    
    for (const QString& process_id : running_processes) {
        QJsonObject single_response = SendCommandToProcess(process_id, command, parameters);
        process_responses[process_id] = single_response;
        
        if (single_response["success"].toBool()) {
            success_count++;
        }
    }
    
    response["success"] = (success_count > 0);
    response["total_processes"] = running_processes.size();
    response["success_count"] = success_count;
    response["responses"] = process_responses;
    
    return response;
}

// ==================== 配置管理接口 ====================

bool MainController::ReloadConfiguration(const QString& config_file_path)
{
    if (!project_config_) {
        qWarning() << "[MainController] ProjectConfig未初始化";
        return false;
    }
    
    QString config_path = config_file_path.isEmpty() ? current_config_file_path_ : config_file_path;
    
    if (!project_config_->loadConfig(config_path)) {
        qWarning() << "[MainController] 配置文件加载失败:" << config_path;
        return false;
    }
    
    // 同步到DataStore
    SyncConfigurationToDataStore();
    
    // 更新配置路径和时间
    current_config_file_path_ = config_path;
    last_config_update_time_ = QDateTime::currentDateTime();
    
    qDebug() << "[MainController] 配置重新加载成功:" << config_path;
    
    emit ConfigurationFileChanged(config_path, "reloaded");
    
    return true;
}

bool MainController::HotUpdateConfiguration(const QJsonObject& updated_config)
{
    if (!project_config_) {
        qWarning() << "[MainController] ProjectConfig未初始化";
        return false;
    }
    
    qDebug() << "[MainController] 热更新配置:" << updated_config;
    // 应用配置更新
    QStringList updated_keys;
    for (auto it = updated_config.begin(); it != updated_config.end(); ++it) {
        QString key = it.key();
        QJsonValue value = it.value();
        
        // 更新ProjectConfig
        project_config_->setConfigValue(key, value);
        updated_keys.append(key);
    }
    
    qDebug() << "config:" << project_config_->getFullConfig();

    project_config_->hotUpdateConfig(project_config_->getFullConfig());

    // 同步到DataStore
    SyncConfigurationToDataStore();
    
    // 广播配置更新到所有子进程
    QJsonObject broadcast_params;
    broadcast_params["updated_config"] = updated_config;

    last_config_update_params_ = broadcast_params; // 记录选择的工作目录
    
    QJsonObject broadcast_result = BroadcastCommand("config_update", broadcast_params);
    
    // 更新统计信息
    QMutexLocker locker(&statistics_mutex_);
    system_statistics_.total_config_updates++;
    
    // 发出热更新完成信号
    int success_count = broadcast_result["success_count"].toInt();
    int total_count = broadcast_result["total_processes"].toInt();
    
    emit ConfigurationHotUpdateCompleted(updated_keys, success_count, total_count);
    
    qDebug() << "[MainController] 配置热更新完成，成功:" << success_count << "/" << total_count;
    
    return success_count > 0 || total_count == 0;  // 如果没有运行的进程也算成功
}

QJsonObject MainController::GetConfigurationSnapshot() const
{
    if (!project_config_) {
        return QJsonObject();
    }
    
    return project_config_->getFullConfig();
}


// ==================== 内部事件处理槽 ====================

void MainController::HandleProcessStatusChanged(const QString& process_id, int old_status, int new_status)
{
    
    // 更新DataStore中的进程状态
    if (data_store_) {
        QString status_key = QString("process.%1.status").arg(process_id);
        data_store_->setValue(status_key, new_status);

        QString update_time_key = QString("process.%1.last_update").arg(process_id);
        data_store_->setValue(update_time_key, QDateTime::currentDateTime());
    }
    
    // 根据状态变化发出相应信号
    if (new_status == static_cast<int>(ProcessManager::kRunning)) {
        QJsonObject process_info;
        process_info["process_id"] = process_id;
        process_info["status"] = new_status;
        process_info["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        emit SubProcessStarted(process_id, process_info);
    } else if (new_status == static_cast<int>(ProcessManager::kNotRunning)) {
        emit SubProcessStopped(process_id, 0);  // 这里可能需要获取实际的退出码
    } else if (new_status == static_cast<int>(ProcessManager::kCrashed)) {
        emit SubProcessCrashed(process_id, "进程崩溃");
    }
    
    // 触发事件回调
    TriggerEventCallback("process_status_changed", {
        {"process_id", process_id},
        {"old_status", old_status},
        {"new_status", new_status},
        {"timestamp", QDateTime::currentDateTime().toString(Qt::ISODate)}
    });
}

void MainController::HandleProcessHeartbeatTimeout(const QString& process_id)
{
    qWarning() << "[MainController] 进程心跳超时:" << process_id;
    
    // 更新DataStore
    if (data_store_) {
        QString timeout_key = QString("process.%1.heartbeat_timeout").arg(process_id);
        data_store_->setValue(timeout_key, QDateTime::currentDateTime());
    }
    
    // 触发事件回调
    TriggerEventCallback("process_heartbeat_timeout", {
        {"process_id", process_id},
        {"timestamp", QDateTime::currentDateTime().toString(Qt::ISODate)}
    });
}

void MainController::HandleIpcMessage(const IpcMessage& message)
{
    // 更新统计信息
    QMutexLocker locker(&statistics_mutex_);
    system_statistics_.total_messages_processed++;
    locker.unlock();
    
    qDebug() << "[MainController] 收到IPC消息:" << static_cast<int>(message.type) 
             << "来自:" << message.sender_id;
    
    // 根据消息类型进行处理
    switch (message.type) {
        case MessageType::kHello:
            HandleHelloMessage(message);
            break;
        case MessageType::kHeartbeat:
            HandleHeartbeatMessage(message);
            break;
        case MessageType::kLogMessage:
            HandleLogMessage(message);
            break;
        case MessageType::kErrorReport:
            HandleErrorReportMessage(message);
            break;
        case MessageType::kCommandResponse:
            HandleCommandResponseMessage(message);
            break;
        default:
            qDebug() << "[MainController] 未处理的消息类型:" << static_cast<int>(message.type);
            break;
    }
    
    // 发出信号
    emit IpcMessageReceived(message);
    
    // 触发事件回调
    TriggerEventCallback("ipc_message_received", {
        {"message_type", static_cast<int>(message.type)},
        {"sender_id", message.sender_id},
        {"topic", message.topic},
        {"timestamp", QDateTime::currentDateTime().toString(Qt::ISODate)}
    });
}

void MainController::HandleIpcConnectionEvent(const QString& client_id, bool connected)
{
    if (connected) {
        
        // 更新DataStore
        if (data_store_) {
            QString conn_key = QString("ipc.connections.%1").arg(client_id);
            QJsonObject conn_info;
            conn_info["connected"] = true;
            conn_info["connect_time"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            data_store_->setValue(conn_key, conn_info);
        }
        IpcMessage ipc_msg;
        ipc_msg.type = MessageType::kConfigUpdate;
        ipc_msg.topic = "MainController";
        ipc_msg.msg_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        ipc_msg.timestamp = QDateTime::currentMSecsSinceEpoch();
        ipc_msg.sender_id = "MainController";
        ipc_msg.receiver_id = client_id;
        ipc_msg.body = last_config_update_params_;
        QtConcurrent::run([this, client_id, ipc_msg]() {
            ipc_context_->sendMessage(client_id, ipc_msg);
        });
        qDebug() << "[MainController] 发送配置更新消息到客户端:" << client_id;
        
        emit IpcClientConnected(client_id, QJsonObject());
        
    } else {
        qDebug() << "[MainController] IPC连接断开:" << client_id;
        
        // 更新DataStore
        if (data_store_) {
            QString conn_key = QString("ipc.connections.%1").arg(client_id);
            data_store_->removeValue(conn_key);
        }
        
        emit IpcClientDisconnected(client_id, "连接断开");
    }
}

void MainController::HandleConfigurationFileChanged(const QString& file_path)
{
    qDebug() << "[MainController] 配置文件变化:" << file_path;
    
    // 如果是当前使用的配置文件，重新加载
    if (file_path == current_config_file_path_) {
        if (ReloadConfiguration()) {
            qDebug() << "[MainController] 配置文件自动重新加载成功";
        } else {
            qWarning() << "[MainController] 配置文件自动重新加载失败";
        }
    }
    
    emit ConfigurationFileChanged(file_path, "modified");
}

void MainController::HandleLogMessage(const LogEntry& log_entry)
{
    // 转发到LogAggregator
    if (log_aggregator_) {
        log_aggregator_->writeLog(log_entry);
    }
    
    // 更新DataStore中的最新日志信息
    if (data_store_) {
        QString log_key = QString("logs.latest.%1").arg(log_entry.source_process);
        QJsonObject log_info;
        log_info["level"] = static_cast<int>(log_entry.level);
        log_info["message"] = log_entry.message;
        log_info["timestamp"] = log_entry.timestamp.toString(Qt::ISODate);
        log_info["thread_id"] = log_entry.thread_id;
        data_store_->setValue(log_key, log_info);
    }
}

void MainController::PerformSystemHealthCheck()
{
    bool was_healthy = is_system_healthy_;
    bool is_healthy = true;
    QString error_message;
    
    // 检查核心模块状态
    if (!process_manager_ || !project_config_ || !data_store_ || !log_aggregator_) {
        is_healthy = false;
        error_message = "核心模块未初始化";
    }
    
    // 检查关键进程状态
    if (is_healthy && process_manager_) {
        QStringList running_processes = process_manager_->GetRunningProcessList();
        // 这里可以添加更多健康检查逻辑
    }
    
    // 更新健康状态
    {
        QMutexLocker locker(&state_mutex_);
        is_system_healthy_ = is_healthy;
        if (!is_healthy) {
            last_error_message_ = error_message;
        }
    }
    
    // 如果健康状态发生变化，发出信号
    if (was_healthy != is_healthy) {
        emit SystemHealthChanged(is_healthy, error_message);
    }
    
    // 更新DataStore
    if (data_store_) {
        data_store_->setValue("system.health.is_healthy", is_healthy);
        data_store_->setValue("system.health.last_check", QDateTime::currentDateTime());
        if (!is_healthy) {
            data_store_->setValue("system.health.error_message", error_message);
        }
    }
}

void MainController::UpdateSystemStatistics()
{
    QMutexLocker locker(&statistics_mutex_);
    system_statistics_.last_statistics_update = QDateTime::currentDateTime();
    
    // 更新DataStore中的统计信息
    if (data_store_) {
        data_store_->setValue("system.statistics.messages_processed",
                            static_cast<qint64>(system_statistics_.total_messages_processed));
        data_store_->setValue("system.statistics.commands_executed",
                            static_cast<qint64>(system_statistics_.total_commands_executed));
        data_store_->setValue("system.statistics.config_updates",
                            static_cast<qint64>(system_statistics_.total_config_updates));
        data_store_->setValue("system.statistics.process_restarts",
                            static_cast<qint64>(system_statistics_.total_process_restarts));
        data_store_->setValue("system.statistics.last_update",
                            system_statistics_.last_statistics_update);
    }
}

// ==================== 内部辅助方法 ====================

bool MainController::InitializeCoreModules()
{
    qDebug() << "[MainController] 初始化核心模块";
    
    try {
        // 1. 获取ProjectConfig单例实例
        project_config_ = &ProjectConfig::getInstance();
        if (!project_config_->initialize(current_config_file_path_)) {
            qWarning() << "[MainController] ProjectConfig初始化失败";
            return false;
        }

        // 如果初始化时配置文件未加载（即创建了默认配置），则保存默认配置
        if (!project_config_->isConfigLoaded()) {
            qInfo() << "[MainController] ProjectConfig未加载，正在保存默认配置...";
            if (!project_config_->saveConfig(current_config_file_path_)) {
                qCritical() << "[MainController] 保存默认配置失败！";
                return false;
            }
            qInfo() << "[MainController] 默认配置保存成功。";
        }

        // 2. 获取DataStore单例实例
        data_store_ = &DataStore::getInstance();
        if (!data_store_->initialize()) {
            qWarning() << "[MainController] DataStore初始化失败";
            return false;
        }
        
        // 3. 初始化LogAggregator
        log_aggregator_ = std::make_unique<LogAggregator>();

        // 3.1 从配置中初始化日志存储
        if (!InitializeLogStorageFromConfig()) {
            qWarning() << "[MainController] 日志存储初始化失败";
            return false;
        }

        // LogAggregator的初始化在Start()中进行 (现在已经注册了存储实例)

        // 4. 初始化IpcContext
        if (!InitializeIpcFromConfig()) {
            qWarning() << "[MainController] IPC初始化失败";
            return false;
        }
        
        // 5. 获取ProcessManager单例
        process_manager_ = &ProcessManager::GetInstance();
        
        // 6. 从配置中注册所有进程到ProcessManager
        QJsonObject processes_config = project_config_->getFullConfig().value("processes").toObject();
        for (const QString& process_id : processes_config.keys()) {
            QJsonObject process_details = processes_config.value(process_id).toObject();
            
            QString executable = process_details.value("executable").toString();
            QString workdir = process_details.value("working_directory").toString();
            bool auto_start = process_details.value("auto_start").toBool(true); // 默认为true

            QStringList arguments;
            QJsonArray args_array = process_details.value("arguments").toArray();
            for (const QJsonValue& arg : args_array) {
                arguments.append(arg.toString());
            }

            if (!executable.isEmpty()) {
                process_manager_->AddProcess(process_id, executable, arguments, workdir, auto_start);
            } else {
                qWarning() << "[MainController] 进程配置错误: 进程" << process_id << "缺少 'executable' 字段";
            }
        }

        qDebug() << "[MainController] UpdateChecker初始化完成";

        return true;
        
    } catch (const std::exception& e) {
        qCritical() << "[MainController] 核心模块初始化异常:" << e.what();
        return false;
    }
}

void MainController::StartSystemMonitoring()
{
    qDebug() << "[MainController] 启动系统监控";
    
    // 启动健康检查定时器
    if (health_check_timer_) {
        health_check_timer_->start(health_check_interval_ms_);
    }
    
    // 启动统计更新定时器
    if (statistics_timer_) {
        statistics_timer_->start(statistics_update_interval_ms_);
    }
}

void MainController::StopSystemMonitoring()
{
    qDebug() << "[MainController] 停止系统监控";
    
    // 停止定时器
    if (health_check_timer_) {
        health_check_timer_->stop();
    }
    
    if (statistics_timer_) {
        statistics_timer_->stop();
    }
}

void MainController::UpdateInitializationState(InitializationState new_state)
{
    InitializationState old_state = initialization_state_;
    if (old_state != new_state) {
        initialization_state_ = new_state;
        
        qDebug() << "[MainController] 初始化状态变化:" << old_state << "->" << new_state;
        
        // 更新DataStore
        if (data_store_) {
            data_store_->setValue("system.initialization_state", static_cast<int>(new_state));
        }
        
        emit InitializationStateChanged(old_state, new_state);
    }
}

void MainController::UpdateSystemStatus(SystemStatus new_status)
{
    SystemStatus old_status = system_status_;
    if (old_status != new_status) {
        system_status_ = new_status;
        
        qDebug() << "[MainController] 系统状态变化:" << old_status << "->" << new_status;
        
        // 更新DataStore
        if (data_store_) {
            data_store_->setValue("system.status", static_cast<int>(new_status));
        }
        
        emit SystemStatusChanged(old_status, new_status);
    }
}

void MainController::SyncConfigurationToDataStore()
{
    if (!project_config_ || !data_store_) {
        return;
    }
    
    qDebug() << "[MainController] 同步配置到DataStore";
    
    // 获取所有配置并同步到DataStore
    QJsonObject all_config = project_config_->getFullConfig();
    
    for (auto it = all_config.begin(); it != all_config.end(); ++it) {
        QString key = QString("config.%1").arg(it.key());
        data_store_->setValue(key, it.value().toVariant());
    }

    // 显式同步IP列表
    if (project_config_) {
        data_store_->setCurrentIpTable(project_config_->getIpTable());
    }

    // 更新同步时间
    data_store_->setValue("config.last_sync_time", QDateTime::currentDateTime());
}

void MainController::HandleSystemError(const QString& error_message, bool is_fatal)
{
    qCritical() << "[MainController] 系统错误:" << error_message << "致命:" << is_fatal;
    
    {
        QMutexLocker locker(&state_mutex_);
        last_error_message_ = error_message;
        is_system_healthy_ = false;
        
        if (is_fatal) {
            initialization_state_ = kError;
            system_status_ = kSystemError;
        }
    }
    
    // 更新DataStore
    if (data_store_) {
        data_store_->setValue("system.last_error", error_message);
        data_store_->setValue("system.error_time", QDateTime::currentDateTime());
        data_store_->setValue("system.is_fatal_error", is_fatal);
    }
    
    emit SystemHealthChanged(false, error_message);
    
    // 如果是致命错误，可能需要触发系统关闭流程
    if (is_fatal) {
        // 这里可以添加致命错误处理逻辑
        qCritical() << "[MainController] 致命错误，系统可能需要重启";
    }
}

void MainController::CleanupSystemResources()
{
    qDebug() << "[MainController] 清理系统资源";
    
    // 停止定时器
    if (health_check_timer_) {
        health_check_timer_->stop();
    }
    if (statistics_timer_) {
        statistics_timer_->stop();
    }
    
    // 清理模块（智能指针会自动清理）
    ipc_context_.reset();
    log_aggregator_.reset();
    // data_store_和project_config_是单例，不需要清理
    // process_manager_不需要清理，因为它是单例
    process_manager_ = nullptr;
    
    // 清理回调
    {
        QMutexLocker locker(&callbacks_mutex_);
        event_callbacks_.clear();
    }
}

bool MainController::CheckModuleDependencies() const
{
    bool all_ok = true;
    
    if (!process_manager_) {
        qCritical() << "[MainController] ProcessManager依赖缺失";
        all_ok = false;
    }
    
    if (!project_config_) {
        qCritical() << "[MainController] ProjectConfig依赖缺失";
        all_ok = false;
    }
    
    if (!data_store_) {
        qCritical() << "[MainController] DataStore依赖缺失";
        all_ok = false;
    }
    
    if (!log_aggregator_) {
        qCritical() << "[MainController] LogAggregator依赖缺失";
        all_ok = false;
    }
    
    if (!ipc_context_) {
        qCritical() << "[MainController] IpcContext依赖缺失";
        all_ok = false;
    }
    
    return all_ok;
}

bool MainController::InitializeLogStorageFromConfig()
{
    if (!log_aggregator_ || !project_config_) {
        qWarning() << "[MainController] LogAggregator或ProjectConfig未初始化";
        return false;
    }

    qDebug() << "[MainController] 开始从配置中初始化日志存储";

    // 从配置中注册日志存储实例
    QJsonObject log_storages_config = project_config_->getConfigValue("log_storages").toObject();
    if (log_storages_config.isEmpty()) {
        qDebug() << "[MainController] 配置中没有日志存储设置，使用默认配置";
        return true; // 没有配置也算成功
    }

    bool all_success = true;
    for (auto it = log_storages_config.begin(); it != log_storages_config.end(); ++it) {
        QString process_id = it.key();
        QJsonObject storage_entry = it.value().toObject();
        QString storage_type_str = storage_entry["type"].toString("file"); // 默认为文件存储
        QJsonObject storage_config = storage_entry["config"].toObject();

        LogStorageType storage_type = LogStorageFactory::getStorageTypeFromString(storage_type_str);

        if (!log_aggregator_->registerStorage(process_id, storage_type, storage_config)) {
            qWarning() << "[MainController] 注册日志存储失败，进程:" << process_id << ", 类型:" << storage_type_str;
            all_success = false;
        } else {
            qDebug() << "[MainController] 成功注册日志存储，进程:" << process_id << ", 类型:" << storage_type_str;
        }
    }

    if (all_success) {
        qDebug() << "[MainController] 日志存储初始化完成";
    } else {
        qWarning() << "[MainController] 部分日志存储初始化失败";
    }

    return all_success;
}

bool MainController::InitializeIpcFromConfig()
{
    if (!project_config_) {
        qWarning() << "[MainController] ProjectConfig未初始化";
        return false;
    }

    qDebug() << "[MainController] 开始从配置中初始化IPC";

    // 创建IpcContext实例
    ipc_context_ = std::make_unique<IpcContext>();

    // IPC的具体初始化需要根据策略进行
    QJsonObject ipc_config = project_config_->getConfigValue("ipc").toObject();
    QString ipc_type_str = ipc_config["type"].toString("LocalSocket"); // 默认使用LocalSocket
    IpcType ipc_type = IpcCommunicationFactory::getIpcTypeFromString(ipc_type_str);

    auto ipc_strategy = IpcCommunicationFactory::createIpcCommunication(ipc_type, ipc_config);
    if (!ipc_strategy || !ipc_context_->setIpcStrategy(std::move(ipc_strategy))) {
        qWarning() << "[MainController] IPCContext初始化失败或设置策略失败";
        return false;
    }

    qDebug() << "[MainController] IPCContext初始化完成，使用类型:" << ipc_type_str;
    return true;
}

void MainController::ConnectModuleSignals()
{
    qDebug() << "[MainController] 连接模块间信号槽";
    
    // 连接ProcessManager信号
    if (process_manager_) {
        connect(process_manager_, &ProcessManager::ProcessStatusChanged,
                this, &MainController::HandleProcessStatusChanged);
        connect(process_manager_, &ProcessManager::HeartbeatTimeout,
                this, &MainController::HandleProcessHeartbeatTimeout);
        connect(process_manager_, &ProcessManager::ProcessAutoRestarted,
                this, &MainController::SubProcessAutoRestarted);
    }
    
    // 连接ProjectConfig信号（如果有文件监控）
    if (project_config_) {
        // connect(project_config_, &ProjectConfig::ConfigurationChanged,
        //         this, &MainController::HandleConfigurationFileChanged);
    }
    
    // 连接IpcContext信号（需要具体实现）
    if (ipc_context_) {
        connect(ipc_context_.get(), &IpcContext::messageReceived,
                this, &MainController::HandleIpcMessage);
        connect(ipc_context_.get(), &IpcContext::clientConnected,
                this, [this](const QString& client_id) {
                    HandleIpcConnectionEvent(client_id, true);
                });
        connect(ipc_context_.get(), &IpcContext::clientDisconnected,
                this, [this](const QString& client_id) {
                    HandleIpcConnectionEvent(client_id, false);
                });
        connect(ipc_context_.get(), &IpcContext::errorOccurred,
                this, [this](const QString& error_message) {
                    HandleSystemError(error_message, false);
                });
        connect(ipc_context_.get(), &IpcContext::connectionStateChanged,
                this, [this](ConnectionState state) {
                    qDebug() << "[MainController] IPC连接状态变化:" << static_cast<int>(state);
                    // 可以在这里更新DataStore或发出更高级别的信号
                });
        connect(ipc_context_.get(), &IpcContext::topicSubscriptionChanged,
                this, [this](const QString& topic, bool subscribed) {
                    qDebug() << "[MainController] Topic订阅状态变化:" << topic << ", 订阅:" << subscribed;
                    // 可以在这里更新DataStore或发出更高级别的信号
                });
    }
}


void MainController::TriggerEventCallback(const QString& event_type, const QJsonObject& event_data)
{
    QMutexLocker locker(&callbacks_mutex_);
    
    auto it = event_callbacks_.find(event_type);
    if (it != event_callbacks_.end()) {
        try {
            it.value()(event_data);
        } catch (const std::exception& e) {
            qWarning() << "[MainController] 事件回调异常:" << event_type << e.what();
        }
    }
}

// ==================== IPC消息处理辅助方法 ====================

void MainController::HandleHelloMessage(const IpcMessage& message)
{
    qDebug() << "[MainController] 处理HELLO消息来自:" << message.sender_id;
    
    // 构造HELLO_ACK响应
    IpcMessage response;
    response.type = MessageType::kHelloAck;
    response.topic = message.topic;
    response.msg_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    response.timestamp = QDateTime::currentMSecsSinceEpoch();
    response.sender_id = "main_controller";
    response.receiver_id = message.sender_id;
    
    // 在响应中包含配置信息
    if (project_config_) {
        QJsonObject process_config = project_config_->getConfigValue(QString("processes.%1").arg(message.sender_id)).toObject();
        response.body["config"] = process_config;
    }
    
    response.body["server_time"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    response.body["welcome_message"] = "欢迎连接到主控程序";
    
    qDebug() << "response:" << response.toJson();
    // 发送响应（需要具体的IPC实现）
    if (ipc_context_) {
        if (!ipc_context_->sendMessage(response)) {
            qWarning() << "[MainController] 发送HELLO_ACK失败到:" << message.sender_id;
        }
    }
    
}

void MainController::HandleHeartbeatMessage(const IpcMessage& message)
{
    // 更新进程心跳
    if (process_manager_) {
        qDebug() << "[MainController] 收到心跳来自:" << message.sender_id;
        QJsonObject body = message.body;
        QString process_name = body["process_name"].toString();
        qDebug() << "[MainController] 更新心跳:" << process_name;
        process_manager_->UpdateHeartbeat(process_name);
    }
    
    IpcMessage ack;
    ack.type = MessageType::kHeartbeatAck;
    ack.topic = message.topic;
    ack.msg_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    ack.timestamp = QDateTime::currentMSecsSinceEpoch();
    ack.sender_id = "main_controller";
    ack.receiver_id = message.sender_id;
    
    ack.body["server_time"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    // 发送确认 ACK
    if (ipc_context_) {
        if (!ipc_context_->sendMessage(ack)) {
            qWarning() << "[MainController] 发送心跳确认失败到:" << message.sender_id;
        }
    }
}



void MainController::HandleLogMessage(const IpcMessage& message)
{
    // 从IPC消息构造LogEntry
    LogEntry log_entry = LogEntry::create(
        static_cast<LogLevel>(message.body["level"].toInt()),
        LogCategory::kBusiness,
        message.sender_id,
        message.body["message"].toString(),
        QString(), // module_name
        message.body["function_name"].toString(),
        message.body["line_number"].toInt()
    );

    // 设置其他属性
    log_entry.timestamp = QDateTime::fromMSecsSinceEpoch(message.timestamp);
    log_entry.thread_id = message.body["thread_id"].toString();

    HandleLogMessage(log_entry);
}

void MainController::HandleErrorReportMessage(const IpcMessage& message)
{
    QString error_message = QString("进程错误报告 [%1]: %2")
                           .arg(message.sender_id)
                           .arg(message.body["error_message"].toString());
    
    qWarning() << "[MainController]" << error_message;
    
    // 更新DataStore
    if (data_store_) {
        QString error_key = QString("process.%1.last_error").arg(message.sender_id);
        QJsonObject error_info;
        error_info["message"] = message.body["error_message"].toString();
        error_info["error_code"] = message.body["error_code"].toInt();
        error_info["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        data_store_->setValue(error_key, error_info);
    }
    
    // 触发错误事件回调
    TriggerEventCallback("process_error_reported", {
        {"process_id", message.sender_id},
        {"error_message", message.body["error_message"].toString()},
        {"error_code", message.body["error_code"].toInt()},
        {"timestamp", QDateTime::currentDateTime().toString(Qt::ISODate)}
    });
}

void MainController::HandleCommandResponseMessage(const IpcMessage& message)
{
    qDebug() << "[MainController] 收到命令响应来自:" << message.sender_id 
             << "消息ID:" << message.msg_id;
    
    // 这里可以实现命令响应的缓存和匹配逻辑
    // 用于SendCommandToProcess方法的异步响应处理
    
    // 触发命令响应事件回调
    TriggerEventCallback("command_response_received", {
        {"process_id", message.sender_id},
        {"message_id", message.msg_id},
        {"response_data", message.body},
        {"timestamp", QDateTime::currentDateTime().toString(Qt::ISODate)}
    });
}


// ==================== 窗口嵌入私有实现方法 ====================

#ifdef Q_OS_WIN
#include <tlhelp32.h>

// Windows特定的窗口枚举结构
struct WindowSearchData {
    DWORD processId;
    HWND foundWindow;
};

// EnumWindowsProc 回调函数
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    WindowSearchData* data = reinterpret_cast<WindowSearchData*>(lParam);
    DWORD windowProcessId;
    GetWindowThreadProcessId(hwnd, &windowProcessId);
    if (windowProcessId == data->processId) {
        if (IsWindowVisible(hwnd)) {
            data->foundWindow = hwnd;
            return FALSE;
        }
    }
    return TRUE;
}

qulonglong MainController::FindProcessMainWindow(const QString& process_id, int max_retries, int retry_delay_ms)
{   
    const ProcessManager::ProcessInfo* process_info = process_manager_->GetProcessInfo(process_id);
    if (!process_info) {
        qWarning() << "[MainController] FindProcessMainWindow: 进程信息不存在或已失效:" << process_id;
        return 0;
    }
    qint64 target_pid = process_info->pid;
    
    // 明确指示正在查找的PID
    qDebug() << "[MainController] FindProcessMainWindow: 正在查找进程\"" << process_id 
             << "\" (目标PID:" << target_pid << ") 的主窗口，最大重试次数:" << max_retries;

    HWND returnData = nullptr;
    // 重试逻辑
    for (int attempt = 0; attempt < max_retries; ++attempt) {
        WindowSearchData searchData = {static_cast<DWORD>(target_pid), nullptr};
        EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&searchData));
        
        if (searchData.foundWindow) {
            qInfo() << "[MainController] 成功找到进程\"" << process_id 
                    << "\"的主窗口。PID:" << target_pid 
                    << " HWND:" << reinterpret_cast<qulonglong>(searchData.foundWindow)
                    << " (尝试次数:" << (attempt + 1) << ")";
            returnData = searchData.foundWindow;
            break;
        }
        
        // 如果不是最后一次尝试，等待后重试
        if (attempt < max_retries - 1) {
            qDebug() << "[MainController] 未找到进程\"" << process_id 
                     << "\"的主窗口，等待" << retry_delay_ms << "ms后重试... (尝试" 
                     << (attempt + 1) << "/" << max_retries << ")";
            QThread::msleep(retry_delay_ms);
        }
    }

    if(returnData) {
        return reinterpret_cast<qulonglong>(returnData);
    }
    
    qWarning() << "[MainController] 经过" << max_retries << "次尝试后，仍未找到进程\"" 
               << process_id << "\" (目标PID:" << target_pid << ") 的主窗口。";
    return 0;
}


bool MainController::EmbedProcessWindowImpl(const QString& process_id, qulonglong container_window_id, const QRect& geometry)
{
    // 查找子进程的主窗口
    qulonglong child_window_handle = FindProcessMainWindow(process_id);
    if (child_window_handle == 0) {
        qWarning() << "[MainController] 无法找到进程窗口:" << process_id;
        return false;
    }
    
    HWND childHwnd = reinterpret_cast<HWND>(child_window_handle);
    HWND parentHwnd = reinterpret_cast<HWND>(container_window_id);
    
    // 设置子窗口的父窗口
    HWND oldParent = SetParent(childHwnd, parentHwnd);
    if (oldParent == nullptr) {
        DWORD error = GetLastError();
        qWarning() << "[MainController] SetParent 失败，错误码:" << error;
        return false;
    }
    
    // 调整窗口样式，移除标题栏和边框
    LONG style = GetWindowLong(childHwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
    style |= WS_CHILD | WS_VISIBLE;
    SetWindowLong(childHwnd, GWL_STYLE, style);
    
    // 移除扩展样式中可能导致窗口置顶的标志
    LONG exStyle = GetWindowLong(childHwnd, GWL_EXSTYLE);
    exStyle &= ~(WS_EX_TOPMOST | WS_EX_TOOLWINDOW);
    SetWindowLong(childHwnd, GWL_EXSTYLE, exStyle);
    
    // 调整子窗口大小和位置以匹配QML项，不改变Z-order
    SetWindowPos(childHwnd, nullptr, geometry.x(), geometry.y(),
                 geometry.width(), geometry.height(),
                 SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOACTIVATE);
    
    qInfo() << "[MainController] 成功嵌入窗口:" << process_id 
            << "子窗口:" << child_window_handle 
            << "父窗口:" << container_window_id;
    
    return true;
}


#else
// 非Windows平台的实现（Linux/macOS）
qulonglong MainController::FindProcessMainWindow(const QString& process_id, int max_retries, int retry_delay_ms)
{
    Q_UNUSED(max_retries)
    Q_UNUSED(retry_delay_ms)
    // TODO: 实现Linux/macOS的窗口查找逻辑
    qWarning() << "[MainController] 当前平台不支持窗口嵌入功能";
    return 0;
}

bool MainController::EmbedProcessWindowImpl(const QString& process_id, qulonglong container_window_id, const QRect& geometry)
{
    // TODO: 实现Linux/macOS的窗口嵌入逻辑
    Q_UNUSED(process_id)
    Q_UNUSED(container_window_id)
    Q_UNUSED(geometry)
    qWarning() << "[MainController] 当前平台不支持窗口嵌入功能";
    return false;
}
#endif

QStringList MainController::GetIpListFromDataStore() const
{
    if (data_store_) {
        return data_store_->getCurrentIpTable();
    }
    return QStringList();
}

/**
 * @brief 通知所有子进程当前选择的IP地址
 * @param selected_ip 被选中的IP地址
 * @return true 通知已发送, false 发送失败
 */
bool MainController::SelectIpAndNotify(const QString& selected_ip)
{
    if (!ipc_context_) {
        qWarning() << "[MainController] IpcContext not initialized, cannot send IP selection notification.";
        return false;
    }

    qInfo() << "[MainController] Notifying all subprocesses of selected IP:" << selected_ip;

    // 1. 构建广播命令参数
    QJsonObject params;
    params["selected_ip"] = selected_ip;
    params["command"] = "select_ip";

    // 2. 广播 "select_ip" 命令
    QJsonObject broadcast_result = BroadcastCommand("select_ip", params);
    
    // 3. 更新统计信息
    // 可根据需要添加新的统计项

    // 4. 发出通知完成信号
    int success_count = broadcast_result.value("success_count").toInt(0);
    int total_count = broadcast_result.value("total_processes").toInt(0);
    
    emit IpSelectionNotified(selected_ip, success_count, total_count);
    
    qDebug() << "[MainController] IP selection notification complete. Success:" << success_count << "/" << total_count;
    
    return success_count > 0 || total_count == 0;  // 如果没有运行的进程也算成功
}

// ==================== 工作区管理方法实现 ====================

/**
 * @brief 设置当前工作目录
 * @param workspace_path 工作目录路径
 * @return true 设置成功, false 设置失败
 */
bool MainController::SetWorkspaceDirectory(const QString& workspace_path)
{
    
    if (!ValidateWorkspacePath(workspace_path)) {
        qWarning() << "[MainController] Invalid workspace path:" << workspace_path;
        return false;
    }
    
    current_workspace_path_ = workspace_path;
    qInfo() << "[MainController] Workspace directory set to:" << workspace_path;
    
    // 自动添加到历史记录
    AddToWorkspaceHistory(workspace_path);

    QJsonObject params;
    params["workspace_path"] = workspace_path;
    params["command"] = "set_workspace_directory";

    QJsonObject broadcast_result = BroadcastCommand("set_workspace_directory", params);

    int success_count = broadcast_result.value("success_count").toInt(0);
    int total_count = broadcast_result.value("total_processes").toInt(0);
    
    qDebug() << "[MainController] Workspace directory set complete. Success:" << success_count << "/" << total_count;
    
    return success_count > 0 || total_count == 0; 
}

/**
 * @brief 获取当前工作目录
 * @return 当前工作目录路径
 */
QString MainController::GetWorkspaceDirectory() const
{
    QMutexLocker locker(&workspace_mutex_);
    return current_workspace_path_;
}

/**
 * @brief 添加工作区到历史记录
 * @param workspace_path 工作区路径
 * @return true 添加成功, false 添加失败
 */
bool MainController::AddToWorkspaceHistory(const QString& workspace_path)
{
    QMutexLocker locker(&workspace_mutex_);
    
    if (!ValidateWorkspacePath(workspace_path)) {
        qWarning() << "[MainController] Cannot add invalid workspace path to history:" << workspace_path;
        return false;
    }
    
    // 检查是否已存在，如果存在则更新时间
    for (int i = 0; i < workspace_history_.size(); ++i) {
        QJsonObject workspace = workspace_history_[i].toObject();
        if (workspace.value("path").toString() == workspace_path) {
            // 更新最后使用时间并移到最前面
            workspace["lastUsed"] = QDateTime::currentDateTime().toMSecsSinceEpoch();
            workspace_history_.removeAt(i);
            workspace_history_.prepend(workspace);
            SaveWorkspaceHistory();
            return true;
        }
    }
    
    // 创建新的工作区记录
    QJsonObject new_workspace;
    new_workspace["path"] = workspace_path;
    new_workspace["name"] = QDir(workspace_path).dirName();
    new_workspace["lastUsed"] = QDateTime::currentDateTime().toMSecsSinceEpoch();
    new_workspace["addedTime"] = QDateTime::currentDateTime().toMSecsSinceEpoch();
    
    // 添加到历史记录开头
    workspace_history_.prepend(new_workspace);
    
    // 限制历史记录数量（最多保留10个）
    while (workspace_history_.size() > 10) {
        workspace_history_.removeLast();
    }
    
    // 保存到文件
    SaveWorkspaceHistory();
    
    qInfo() << "[MainController] Added workspace to history:" << workspace_path;
    return true;
}

/**
 * @brief 从历史记录中移除工作区
 * @param workspace_path 工作区路径
 * @return true 移除成功, false 移除失败
 */
bool MainController::RemoveFromWorkspaceHistory(const QString& workspace_path)
{
    QMutexLocker locker(&workspace_mutex_);
    
    for (int i = 0; i < workspace_history_.size(); ++i) {
        QJsonObject workspace = workspace_history_[i].toObject();
        if (workspace.value("path").toString() == workspace_path) {
            workspace_history_.removeAt(i);
            SaveWorkspaceHistory();
            qInfo() << "[MainController] Removed workspace from history:" << workspace_path;
            return true;
        }
    }
    
    qWarning() << "[MainController] Workspace not found in history:" << workspace_path;
    return false;
}

/**
 * @brief 获取工作区历史记录
 * @return 工作区历史记录的JSON数组
 */
QJsonArray MainController::GetWorkspaceHistory() const
{
    QMutexLocker locker(&workspace_mutex_);
    return workspace_history_;
}

/**
 * @brief 清空工作区历史记录
 * @return true 清空成功, false 清空失败
 */
bool MainController::ClearWorkspaceHistory()
{
    QMutexLocker locker(&workspace_mutex_);
    
    workspace_history_ = QJsonArray();
    SaveWorkspaceHistory();
    
    qInfo() << "[MainController] Cleared workspace history";
    return true;
}

/**
 * @brief 加载工作区历史记录
 * @return true 加载成功, false 加载失败
 */
bool MainController::LoadWorkspaceHistory()
{
    QFile file(workspace_history_file_path_);
    if (!file.exists()) {
        qDebug() << "[MainController] Workspace history file does not exist, creating empty history";
        workspace_history_ = QJsonArray();
        return true;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[MainController] Failed to open workspace history file for reading:" << file.errorString();
        workspace_history_ = QJsonArray();
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError parse_error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parse_error);
    
    if (parse_error.error != QJsonParseError::NoError) {
        qWarning() << "[MainController] Failed to parse workspace history JSON:" << parse_error.errorString();
        workspace_history_ = QJsonArray();
        return false;
    }
    
    if (!doc.isArray()) {
        qWarning() << "[MainController] Workspace history file does not contain a JSON array";
        workspace_history_ = QJsonArray();
        return false;
    }
    
    workspace_history_ = doc.array();
    qDebug() << "[MainController] Loaded" << workspace_history_.size() << "workspace history entries";
    return true;
}

/**
 * @brief 保存工作区历史记录
 * @return true 保存成功, false 保存失败
 */
bool MainController::SaveWorkspaceHistory() const
{
    QFile file(workspace_history_file_path_);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "[MainController] Failed to open workspace history file for writing:" << file.errorString();
        return false;
    }
    
    QJsonDocument doc(workspace_history_);
    QByteArray data = doc.toJson(QJsonDocument::Indented);
    
    qint64 bytes_written = file.write(data);
    file.close();
    
    if (bytes_written != data.size()) {
        qWarning() << "[MainController] Failed to write complete workspace history data";
        return false;
    }
    
    qDebug() << "[MainController] Saved workspace history with" << workspace_history_.size() << "entries";
    return true;
}

/**
 * @brief 验证工作区路径是否有效
 * @param workspace_path 工作区路径
 * @return true 有效, false 无效
 */
bool MainController::ValidateWorkspacePath(const QString& workspace_path) const
{
    if (workspace_path.isEmpty()) {
        return false;
    }
    
    QDir dir(workspace_path);
    if (!dir.exists()) {
        // 尝试创建目录
        if (!dir.mkpath(workspace_path)) {
            qWarning() << "[MainController] Cannot create workspace directory:" << workspace_path;
            return false;
        }
    }
    
    // 检查是否可写
    QFileInfo dir_info(workspace_path);
    if (!dir_info.isWritable()) {
        qWarning() << "[MainController] Workspace directory is not writable:" << workspace_path;
        return false;
    }
    
    return true;
}

void MainController::CheckForUpdates()
{
    qDebug() << "=== CheckForUpdates 开始执行 === (PID:" << QCoreApplication::applicationPid() << ")";

    QString updater_dir = QCoreApplication::applicationDirPath();
    QString updater_path = updater_dir + "/updater.exe";

    qDebug() << "尝试启动更新程序，路径: " << updater_path;

    if (!QFile::exists(updater_path)) {
        qWarning() << "更新程序不存在: " << updater_path;
        // QMessageBox::critical(nullptr, tr("错误"), tr("未找到更新程序 (updater.exe)，请确保它与主程序在同一目录下。")); // 使用 nullptr 作为父对象
        return;
    }

    // 在启动更新程序之前，记录当前的系统和应用状态
    qDebug() << "系统信息:";
    qDebug() << "  操作系统:" << QSysInfo::productType() << QSysInfo::productVersion();
    qDebug() << "  CPU架构:" << QSysInfo::currentCpuArchitecture();
    qDebug() << "  应用程序目录:" << QCoreApplication::applicationDirPath();
    qDebug() << "  应用程序文件:" << QCoreApplication::applicationFilePath();

    // 检查更新程序的权限和属性
    QFileInfo updaterFileInfo(updater_path);
    qDebug() << "更新程序文件信息:";
    qDebug() << "  文件大小:" << updaterFileInfo.size() << "字节";
    qDebug() << "  是否可执行:" << updaterFileInfo.isExecutable();
    qDebug() << "  文件权限:"
             << (updaterFileInfo.permissions() & QFile::ReadOwner ? "可读 " : "")
             << (updaterFileInfo.permissions() & QFile::WriteOwner ? "可写 " : "")
             << (updaterFileInfo.permissions() & QFile::ExeOwner ? "可执行" : "");

    qDebug() << "即将以完全独立模式启动 updater.exe...";
    
    // 使用 WinAPI 创建一个完全独立的进程，脱离父进程的控制台
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // 准备命令行参数
    QString command = "\"" + updater_path + "\"";
    
    // 设置安全属性，允许子进程继承句柄
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    bool started = CreateProcessW(
        NULL,                           // 使用 lpCommandLine 中的程序名
        (LPWSTR)command.utf16(),        // 命令行
        &sa,                            // 进程安全属性
        NULL,                           // 线程安全属性
        TRUE,                           // 继承句柄
        CREATE_NEW_CONSOLE |
        NORMAL_PRIORITY_CLASS,          // 普通优先级
        NULL,                           // 使用父进程的环境变量
        (LPWSTR)updater_dir.utf16(),    // 工作目录
        &si,                            // STARTUPINFO
        &pi                             // PROCESS_INFORMATION
    );
    
    if (started) {
        qDebug() << "更新程序已成功启动，进程ID:" << pi.dwProcessId;
        
        // 关闭进程和线程句柄
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        qDebug() << "更新程序已成功启动，主程序即将退出";
        
        
        // QMessageBox::information(this, tr("更新"), tr("更新程序已启动，主程序将退出以完成更新过程。"));
        
        // 尝试优雅地退出应用程序
        QTimer::singleShot(100, this, [this]() {
            qDebug() << "正在尝试优雅退出...";
            QCoreApplication::quit();
        });
    } else {
        DWORD error = GetLastError();
        qWarning() << "启动更新程序失败，错误代码:" << error;
        
        // 获取详细的错误信息
        LPVOID lpMsgBuf;
        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR) &lpMsgBuf,
            0, NULL
        );

        QString errorMsg = QString::fromWCharArray((LPCWSTR)lpMsgBuf);
        LocalFree(lpMsgBuf);

        qWarning() << "详细错误信息:" << errorMsg;
        

    }

    qDebug() << "=== CheckForUpdates 执行完毕 ===";
}
