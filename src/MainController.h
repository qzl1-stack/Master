#ifndef MAIN_CONTROLLER_H
#define MAIN_CONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QHash>
#include <QQuickItem>
#include <QQuickWindow>
#include <QWindow>
#include <memory>
#include <QQmlApplicationEngine>

// 前置声明
class ProcessManager;
class ProjectConfig;
class DataStore;
class LogAggregator;
class IpcContext;
class UpdateChecker;
class PluginManager;
struct IpcMessage;
struct LogEntry;

/**
 * @brief MainController 主控调度器类
 * 
 * 负责统一管理和协调所有核心功能模块，MainController 负责模块的初始化、启动、停止、
 * 全局事件分发和业务流程驱动，是主进程的"大脑"和总入口，确保各模块协同工作、
 * 数据流转和系统整体有序运行。
 * 
 * 主要职责：
 * 1. 系统初始化和启动流程管理
 * 2. 核心模块的生命周期管理和依赖注入
 * 3. 全局事件分发和业务流程协调
 * 4. IPC消息路由和处理
 * 5. 配置热更新和同步
 * 6. 异常处理和系统恢复
 * 
 * 设计模式：单例模式（Singleton Pattern）
 * 线程安全：是
 */
class MainController : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(QObject* pluginManager READ GetPluginManager CONSTANT)
    
public:
    /**
     * @brief 系统初始化状态枚举
     */
    enum InitializationState {
        kNotInitialized = 0,    // 未初始化
        kInitializing,          // 初始化中
        kInitialized,          // 已初始化
        kStarted,              // 已启动
        kStopping,             // 停止中
        kStopped,              // 已停止
        kError                 // 错误状态
    };
    
    /**
     * @brief 系统运行状态枚举
     */
    enum SystemStatus {
        kSystemIdle = 0,        // 系统空闲
        kSystemRunning,         // 系统运行中
        kSystemBusy,            // 系统繁忙
        kSystemError,           // 系统错误
        kSystemMaintenance      // 系统维护中
    };

private:
    explicit MainController(QObject* parent = nullptr);
    
    /**
     * @brief 友元类，用于单例创建
     */
    friend class MainControllerDeleter;
    
public:
    static MainController& GetInstance();
    ~MainController();
    // 禁用拷贝构造和赋值操作
    MainController(const MainController&) = delete;
    MainController& operator=(const MainController&) = delete;

    Q_INVOKABLE void CheckForUpdates();
    /**
     * @param config_file_path 配置文件路径（可选，默认使用标准路径）
     */
    Q_INVOKABLE bool Initialize(const QString& config_file_path = QString());
    Q_INVOKABLE bool Start();
    Q_INVOKABLE bool Stop(int timeout_ms = 5000);
    Q_INVOKABLE bool Restart(const QString& config_file_path = QString());
    
    Q_INVOKABLE InitializationState GetInitializationState() const;
    
    Q_INVOKABLE SystemStatus GetSystemStatus() const;

    Q_INVOKABLE bool IsSystemHealthy() const;
    
    Q_INVOKABLE QJsonObject GetSystemStatistics() const;
    
    // ==================== 模块访问接口 ====================
    ProcessManager* GetProcessManager() const;
    ProjectConfig* GetProjectConfig() const;
    DataStore* GetDataStore() const;
    LogAggregator* GetLogAggregator() const;
    IpcContext* GetIpcContext() const;
    UpdateChecker* GetUpdateChecker() const;
    QObject* GetPluginManager() const;
    
    // ==================== 业务流程接口 ====================
    
    
    Q_INVOKABLE bool StartSubProcess(const QString& process_id, bool force_restart = false);
    
    Q_INVOKABLE bool StopSubProcess(const QString& process_id, int timeout_ms = 5000);
    
    
    Q_INVOKABLE QJsonObject GetAllProcessInfo() const;
    
    Q_INVOKABLE QJsonObject SendCommandToProcess(const QString& process_id, 
                                   const QString& command, 
                                   const QJsonObject& parameters = QJsonObject(),
                                   int timeout_ms = 10000);
    
    Q_INVOKABLE QJsonObject BroadcastCommand(const QString& command, 
                               const QJsonObject& parameters = QJsonObject());
    Q_INVOKABLE QVariantList GetConfiguredProcessNames() const;
    
    Q_INVOKABLE int GetProcessStatus(const QString& process_id) const;

    Q_INVOKABLE QStringList GetIpListFromDataStore() const;
    
    // ==================== 窗口嵌入相关方法 ====================
    
    Q_INVOKABLE bool EmbedProcessWindow(const QString& process_id, QObject* container_item);
    Q_INVOKABLE bool UpdateEmbeddedWindowGeometry(const QString& process_id, QObject* container_item);
    Q_INVOKABLE bool SetEmbeddedProcessWindowVisible(const QString& process_id,
                                                    bool visible);
    Q_INVOKABLE void startEmbeddingProcess(const QString& process_name);
    Q_INVOKABLE void finishEmbeddingProcess(const QString& process_name);

    
    // ==================== 配置管理接口 ====================
    
    Q_INVOKABLE bool ReloadConfiguration(const QString& config_file_path = QString());
    
    Q_INVOKABLE bool HotUpdateConfiguration(const QJsonObject& updated_config);
    
    Q_INVOKABLE QJsonObject GetConfigurationSnapshot() const;
    
    Q_INVOKABLE bool SelectIpAndNotify(const QString& selected_ip);
    
    // ==================== 工作区管理接口 ====================
    
    Q_INVOKABLE bool SetWorkspaceDirectory(const QString& workspace_path);
    
    Q_INVOKABLE QString GetWorkspaceDirectory() const;
    
    Q_INVOKABLE bool AddToWorkspaceHistory(const QString& workspace_path);
    
    Q_INVOKABLE bool RemoveFromWorkspaceHistory(const QString& workspace_path);
    
    Q_INVOKABLE QJsonArray GetWorkspaceHistory() const;
    
    Q_INVOKABLE bool ClearWorkspaceHistory();


signals:
    // ==================== 系统状态信号 ====================
    
    void InitializationStateChanged(InitializationState old_state, InitializationState new_state);
    
    void SystemStatusChanged(SystemStatus old_status, SystemStatus new_status);
    
    void SystemHealthChanged(bool is_healthy, const QString& error_message);
    
    // ==================== 进程管理信号 ====================
    
    void SubProcessStarted(const QString& process_id, const QJsonObject& process_info);
    void SubProcessStopped(const QString& process_id, int exit_code);
    void SubProcessCrashed(const QString& process_id, const QString& error_message);
    
    // ==================== IPC通信信号 ====================
    
    void IpcClientConnected(const QString& client_id, const QJsonObject& client_info);
    
    void IpcClientDisconnected(const QString& client_id, const QString& reason);
    void IpcMessageReceived(const IpcMessage& message);
    
    // ==================== 配置管理信号 ====================
    void ConfigurationFileChanged(const QString& config_path, const QString& change_type);
    void ConfigurationHotUpdateCompleted(const QStringList& updated_keys, 
                                       int success_count, 
                                       int total_count);

    // ==================== 业务逻辑信号 ====================

    void IpSelectionNotified(const QString& selected_ip, int success_count, int total_count);

private slots:
    // ==================== 内部事件处理槽 ====================
    
    void HandleProcessStatusChanged(const QString& process_id, int old_status, int new_status);
    
    void HandleProcessHeartbeatTimeout(const QString& process_id);
    
    void HandleIpcMessage(const IpcMessage& message);
    
    void HandleIpcConnectionEvent(const QString& client_id, bool connected);
    
    void HandleConfigurationFileChanged(const QString& file_path);
    
    void PerformSystemHealthCheck();
    
    void UpdateSystemStatistics();

private:
    // ==================== 内部辅助方法 ====================


    bool InitializeCoreModules();

    
    bool EmbedProcessWindowImpl(const QString& process_id, qulonglong container_window_id, const QRect& geometry);
    qulonglong FindProcessMainWindow(const QString& process_id, int max_retries = 10, int retry_delay_ms = 400);
    bool InitializeIpcFromConfig();
    void ConnectModuleSignals();

    void StartSystemMonitoring();
    void StopSystemMonitoring();


    /**
     * @brief 触发事件回调
     * @param event_type 事件类型
     * @param event_data 事件数据
     */
    void TriggerEventCallback(const QString& event_type, const QJsonObject& event_data);

    // ==================== IPC消息处理辅助方法 ====================

    void HandleHelloMessage(const IpcMessage& message);

    void HandleHeartbeatMessage(const IpcMessage& message);

    void UpdateInitializationState(InitializationState new_state);
    void UpdateSystemStatus(SystemStatus new_status);
    void SyncConfigurationToDataStore();
    
    /**
     * @brief 处理系统错误
     * @param error_message 错误信息
     * @param is_fatal 是否为致命错误
     */
    void HandleSystemError(const QString& error_message, bool is_fatal = false);
    

    void CleanupSystemResources();
    
    /**
     * @brief 检查模块依赖关系
     * @return true 依赖满足，false 依赖缺失
     */
    bool CheckModuleDependencies() const;
    
    // ==================== 工作区管理辅助方法 ====================
    
    bool LoadWorkspaceHistory();
    
    bool SaveWorkspaceHistory() const;
    
    bool ValidateWorkspacePath(const QString& workspace_path) const;


private:
    // ==================== 单例相关 ====================
    static std::unique_ptr<MainController> instance_;
    static QMutex instance_mutex_;
    
    // ==================== 核心模块指针 ====================
    ProcessManager* process_manager_; 
    ProjectConfig* project_config_;    // 单例，不使用智能指针管理
    DataStore* data_store_;           
    std::unique_ptr<IpcContext> ipc_context_;
    std::unique_ptr<UpdateChecker> update_checker_;
    
    // ==================== 状态管理 ====================
    mutable QMutex state_mutex_;
    InitializationState initialization_state_;
    SystemStatus system_status_;
    bool is_system_healthy_;
    QString last_error_message_;
    QDateTime startup_time_;
    
    // ==================== 系统监控 ====================
    std::unique_ptr<QTimer> health_check_timer_;
    std::unique_ptr<QTimer> statistics_timer_;
    int health_check_interval_ms_;
    int statistics_update_interval_ms_;
    
    // ==================== 配置管理 ====================
    QString current_config_file_path_;
    QDateTime last_config_update_time_;
    
    // ==================== 工作区管理 ====================
    QString current_workspace_path_;
    QJsonArray workspace_history_;
    QString workspace_history_file_path_;
    QJsonObject last_config_update_params_;
    mutable QMutex workspace_mutex_;
    
    // ==================== 事件回调 ====================
    QHash<QString, std::function<void(const QJsonObject&)>> event_callbacks_;
    mutable QMutex callbacks_mutex_;
    
    // ==================== 窗口嵌入状态 ====================
    mutable QMutex embedding_mutex_;
    QHash<QString, bool> embedding_in_progress_;
    QHash<QString, bool> embedding_cancelled_;
    QHash<QString, qulonglong> embedded_window_handles_; // 缓存已嵌入的窗口句柄

    // ==================== 系统统计 ====================
    struct SystemStatistics {
        qint64 total_messages_processed;
        qint64 total_commands_executed;
        qint64 total_config_updates;
        qint64 total_process_restarts;
        QDateTime last_statistics_update;
        
        SystemStatistics() 
            : total_messages_processed(0)
            , total_commands_executed(0)
            , total_config_updates(0)
            , total_process_restarts(0)
            , last_statistics_update(QDateTime::currentDateTime())
        {}
    } system_statistics_;
    mutable QMutex statistics_mutex_;
};

/**
 * @brief MainController单例删除器（友元类）
 */
class MainControllerDeleter {
public:
    void operator()(MainController* ptr) {
        delete ptr;
    }
};

#endif // MAIN_CONTROLLER_H
