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
    /**
     * @brief 私有构造函数（单例模式）
     * @param parent 父对象指针
     */
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
    
    /**
     * @brief 系统启动
     * 
     * 启动所有服务和子进程：
     * 1. 启动IPC服务端监听
     * 2. 根据配置启动业务子进程
     * 3. 启动系统监控和心跳检测
     * 4. 更新DataStore状态快照
     * 
     * @return true 启动成功，false 启动失败
     */
    Q_INVOKABLE bool Start();
    
    /**
     * @brief 系统停止
     * @param timeout_ms 停止超时时间（毫秒），默认10秒
     * @return true 停止成功，false 停止超时或失败
     */
    Q_INVOKABLE bool Stop(int timeout_ms = 5000);
    
    /**
     * @brief 系统重启
     * @param config_file_path 配置文件路径（可选）
     * @return true 重启成功，false 重启失败
     */
    Q_INVOKABLE bool Restart(const QString& config_file_path = QString());
    
    /**
     * @brief 获取当前初始化状态
     * @return 初始化状态枚举值
     */
    Q_INVOKABLE InitializationState GetInitializationState() const;
    
    /**
     * @brief 获取当前系统状态
     * @return 系统状态枚举值
     */
    Q_INVOKABLE SystemStatus GetSystemStatus() const;
    
    /**
     * @brief 检查系统是否健康运行
     * @return true 系统健康，false 存在问题
     */
    Q_INVOKABLE bool IsSystemHealthy() const;
    
    /**
     * @brief 获取系统运行统计信息
     * @return 包含系统统计信息的JSON对象
     */
    Q_INVOKABLE QJsonObject GetSystemStatistics() const;
    
    // ==================== 模块访问接口 ====================
    
    /**
     * @brief 获取ProcessManager实例
     * @return ProcessManager指针，如果未初始化则返回nullptr
     */
    ProcessManager* GetProcessManager() const;
    
    /**
     * @brief 获取ProjectConfig实例
     * @return ProjectConfig指针，如果未初始化则返回nullptr
     */
    ProjectConfig* GetProjectConfig() const;
    
    /**
     * @brief 获取DataStore实例
     * @return DataStore指针，如果未初始化则返回nullptr
     */
    DataStore* GetDataStore() const;
    
    /**
     * @brief 获取LogAggregator实例
     * @return LogAggregator指针，如果未初始化则返回nullptr
     */
    LogAggregator* GetLogAggregator() const;
    
    /**
     * @brief 获取IpcContext实例
     * @return IpcContext指针，如果未初始化则返回nullptr
     */
    IpcContext* GetIpcContext() const;
    
    /**
     * @brief 获取UpdateChecker实例
     * @return UpdateChecker指针，如果未初始化则返回nullptr
     */
    UpdateChecker* GetUpdateChecker() const;
    
    // ==================== 业务流程接口 ====================
    
    /**
     * @brief 启动指定子进程
     * @param process_id 进程ID
     * @param force_restart 是否强制重启（如果已运行）
     * @return true 启动成功，false 启动失败
     */
    Q_INVOKABLE bool StartSubProcess(const QString& process_id, bool force_restart = false);
    
    /**
     * @brief 停止指定子进程
     * @param process_id 进程ID
     * @param timeout_ms 停止超时时间（毫秒）
     * @return true 停止成功，false 停止失败
     */
    Q_INVOKABLE bool StopSubProcess(const QString& process_id, int timeout_ms = 5000);
    
    /**
     * @brief 重启指定子进程
     * @param process_id 进程ID
     * @return true 重启成功，false 重启失败
     */
    Q_INVOKABLE bool RestartSubProcess(const QString& process_id);
    
    /**
     * @brief 获取所有子进程状态
     * @return 包含所有进程状态的JSON对象
     */
    Q_INVOKABLE QJsonObject GetAllProcessInfo() const;
    
    /**
     * @brief 向指定子进程发送命令
     * @param process_id 进程ID
     * @param command 命令名称
     * @param parameters 命令参数
     * @param timeout_ms 响应超时时间（毫秒）
     * @return 命令响应结果的JSON对象
     */
    Q_INVOKABLE QJsonObject SendCommandToProcess(const QString& process_id, 
                                   const QString& command, 
                                   const QJsonObject& parameters = QJsonObject(),
                                   int timeout_ms = 10000);
    
    /**
     * @brief 广播命令到所有子进程
     * @param command 命令名称
     * @param parameters 命令参数
     * @return 包含所有进程响应的JSON对象
     */
    Q_INVOKABLE QJsonObject BroadcastCommand(const QString& command, 
                               const QJsonObject& parameters = QJsonObject());
    
    /**
     * @brief 获取配置的进程名称列表
     * @return 包含所有配置进程名称的QVariantList
     */
    Q_INVOKABLE QVariantList GetConfiguredProcessNames() const;

    Q_INVOKABLE QStringList GetIpListFromDataStore() const;
    
    // ==================== 窗口嵌入相关方法 ====================
    
    Q_INVOKABLE bool EmbedProcessWindow(const QString& process_id, QObject* container_item);
    Q_INVOKABLE bool UpdateEmbeddedWindowGeometry(const QString& process_id, QObject* container_item);
    Q_INVOKABLE void startEmbeddingProcess(const QString& process_name);
    Q_INVOKABLE void finishEmbeddingProcess(const QString& process_name);

    
    // ==================== 配置管理接口 ====================
    
    /**
     * @brief 重新加载配置文件
     * @param config_file_path 配置文件路径（可选）
     * @return true 加载成功，false 加载失败
     */
    Q_INVOKABLE bool ReloadConfiguration(const QString& config_file_path = QString());
    
    /**
     * @brief 热更新配置到所有子进程
     * @param updated_config 更新的配置内容
     * @return true 更新成功，false 更新失败
     */
    Q_INVOKABLE bool HotUpdateConfiguration(const QJsonObject& updated_config);
    
    /**
     * @brief 获取当前配置快照
     * @return 当前配置的JSON对象
     */
    Q_INVOKABLE QJsonObject GetConfigurationSnapshot() const;
    
    /**
     * @brief 通知所有子进程当前选择的IP地址
     * @param selected_ip 被选中的IP地址
     * @return true 通知已发送, false 发送失败
     */
    Q_INVOKABLE bool SelectIpAndNotify(const QString& selected_ip);
    
    // ==================== 工作区管理接口 ====================
    
    /**
     * @brief 设置当前工作目录
     * @param workspace_path 工作目录路径
     * @return true 设置成功, false 设置失败
     */
    Q_INVOKABLE bool SetWorkspaceDirectory(const QString& workspace_path);
    
    /**
     * @brief 获取当前工作目录
     * @return 当前工作目录路径
     */
    Q_INVOKABLE QString GetWorkspaceDirectory() const;
    
    /**
     * @brief 添加工作区到历史记录
     * @param workspace_path 工作区路径
     * @return true 添加成功, false 添加失败
     */
    Q_INVOKABLE bool AddToWorkspaceHistory(const QString& workspace_path);
    
    /**
     * @brief 从历史记录中移除工作区
     * @param workspace_path 工作区路径
     * @return true 移除成功, false 移除失败
     */
    Q_INVOKABLE bool RemoveFromWorkspaceHistory(const QString& workspace_path);
    
    /**
     * @brief 获取工作区历史记录
     * @return 工作区历史记录的JSON数组
     */
    Q_INVOKABLE QJsonArray GetWorkspaceHistory() const;
    
    /**
     * @brief 清空工作区历史记录
     * @return true 清空成功, false 清空失败
     */
    Q_INVOKABLE bool ClearWorkspaceHistory();


signals:
    // ==================== 系统状态信号 ====================
    
    /**
     * @brief 系统初始化状态变化信号
     * @param old_state 旧状态
     * @param new_state 新状态
     */
    void InitializationStateChanged(InitializationState old_state, InitializationState new_state);
    
    /**
     * @brief 系统运行状态变化信号
     * @param old_status 旧状态
     * @param new_status 新状态
     */
    void SystemStatusChanged(SystemStatus old_status, SystemStatus new_status);
    
    /**
     * @brief 系统健康状态变化信号
     * @param is_healthy 是否健康
     * @param error_message 错误信息（如果不健康）
     */
    void SystemHealthChanged(bool is_healthy, const QString& error_message);
    
    // ==================== 进程管理信号 ====================
    
    /**
     * @brief 子进程启动信号
     * @param process_id 进程ID
     * @param process_info 进程信息
     */
    void SubProcessStarted(const QString& process_id, const QJsonObject& process_info);
    
    /**
     * @brief 子进程停止信号
     * @param process_id 进程ID
     * @param exit_code 退出码
     */
    void SubProcessStopped(const QString& process_id, int exit_code);
    
    /**
     * @brief 子进程崩溃信号
     * @param process_id 进程ID
     * @param error_message 错误信息
     */
    void SubProcessCrashed(const QString& process_id, const QString& error_message);
    
    /**
     * @brief 子进程自动重启信号
     * @param process_id 进程ID
     * @param restart_count 重启次数
     */
    void SubProcessAutoRestarted(const QString& process_id, int restart_count);
    
    // ==================== IPC通信信号 ====================
    
    /**
     * @brief 新的IPC连接建立信号
     * @param client_id 客户端ID
     * @param client_info 客户端信息
     */
    void IpcClientConnected(const QString& client_id, const QJsonObject& client_info);
    
    /**
     * @brief IPC连接断开信号
     * @param client_id 客户端ID
     * @param reason 断开原因
     */
    void IpcClientDisconnected(const QString& client_id, const QString& reason);
    
    /**
     * @brief 收到IPC消息信号
     * @param message 消息内容
     */
    void IpcMessageReceived(const IpcMessage& message);
    
    // ==================== 配置管理信号 ====================
    
    /**
     * @brief 配置文件变化信号
     * @param config_path 配置文件路径
     * @param change_type 变化类型
     */
    void ConfigurationFileChanged(const QString& config_path, const QString& change_type);
    
    /**
     * @brief 配置热更新完成信号
     * @param updated_keys 更新的配置键列表
     * @param success_count 成功更新的进程数
     * @param total_count 总进程数
     */
    void ConfigurationHotUpdateCompleted(const QStringList& updated_keys, 
                                       int success_count, 
                                       int total_count);

    // ==================== 业务逻辑信号 ====================

    /**
     * @brief IP选择通知完成信号
     * @param selected_ip 选择的IP地址
     * @param success_count 成功通知的进程数
     * @param total_count 总进程数
     */
    void IpSelectionNotified(const QString& selected_ip, int success_count, int total_count);

private slots:
    // ==================== 内部事件处理槽 ====================
    
    /**
     * @brief 处理进程状态变化
     */
    void HandleProcessStatusChanged(const QString& process_id, int old_status, int new_status);
    
    /**
     * @brief 处理进程心跳超时
     */
    void HandleProcessHeartbeatTimeout(const QString& process_id);
    
    /**
     * @brief 处理IPC消息
     */
    void HandleIpcMessage(const IpcMessage& message);
    
    /**
     * @brief 处理IPC连接事件
     */
    void HandleIpcConnectionEvent(const QString& client_id, bool connected);
    
    /**
     * @brief 处理配置文件变化
     */
    void HandleConfigurationFileChanged(const QString& file_path);
    
    /**
     * @brief 处理日志消息
     */
    void HandleLogMessage(const LogEntry& log_entry);
    
    /**
     * @brief 系统健康检查定时器
     */
    void PerformSystemHealthCheck();
    
    /**
     * @brief 系统统计更新定时器
     */
    void UpdateSystemStatistics();

private:
    // ==================== 内部辅助方法 ====================

    /**
     * @brief 初始化核心模块
     * @return true 成功，false 失败
     */
    bool InitializeCoreModules();

    /**
     * @brief 从配置中初始化日志存储
     * @return true 成功，false 失败
     */
    bool InitializeLogStorageFromConfig();
    
    /**
     * @brief 平台特定的窗口嵌入实现
     * @param process_id 进程ID
     * @param container_window_id 容器窗口ID
     * @return 是否成功嵌入
     */
    bool EmbedProcessWindowImpl(const QString& process_id, qulonglong container_window_id, const QRect& geometry);
    
    /**
     * @brief 查找进程的主窗口句柄（带重试机制）
     * @param process_id 进程ID
     * @param max_retries 最大重试次数，默认3次
     * @param retry_delay_ms 每次重试的延迟时间（毫秒），默认200ms
     * @return 窗口句柄，失败返回0
     */
    qulonglong FindProcessMainWindow(const QString& process_id, int max_retries = 10, int retry_delay_ms = 400);

    /**
     * @brief 从配置中初始化IPC
     * @return true 成功，false 失败
     */
    bool InitializeIpcFromConfig();

    /**
     * @brief 连接模块间信号槽
     */
    void ConnectModuleSignals();

    /**
     * @brief 启动系统监控
     */
    void StartSystemMonitoring();

    /**
     * @brief 停止系统监控
     */
    void StopSystemMonitoring();

    /**
     * @brief 启动配置的进程
     */
    void StartConfiguredProcesses();

    /**
     * @brief 触发事件回调
     * @param event_type 事件类型
     * @param event_data 事件数据
     */
    void TriggerEventCallback(const QString& event_type, const QJsonObject& event_data);

    // ==================== IPC消息处理辅助方法 ====================

    void HandleHelloMessage(const IpcMessage& message);

    /**
     * @brief 处理心跳消息
     * @param message IPC消息
     */
    void HandleHeartbeatMessage(const IpcMessage& message);


    /**
     * @brief 处理日志消息
     * @param message IPC消息
     */
    void HandleLogMessage(const IpcMessage& message);

    /**
     * @brief 处理错误报告消息
     * @param message IPC消息
     */
    void HandleErrorReportMessage(const IpcMessage& message);

    /**
     * @brief 处理命令响应消息
     * @param message IPC消息
     */
    void HandleCommandResponseMessage(const IpcMessage& message);

    /**
     * @brief 更新初始化状态
     * @param new_state 新状态
     */
    void UpdateInitializationState(InitializationState new_state);
    
    /**
     * @brief 更新系统状态
     * @param new_status 新状态
     */
    void UpdateSystemStatus(SystemStatus new_status);
    
    /**
     * @brief 同步配置到DataStore
     */
    void SyncConfigurationToDataStore();
    
    /**
     * @brief 处理系统错误
     * @param error_message 错误信息
     * @param is_fatal 是否为致命错误
     */
    void HandleSystemError(const QString& error_message, bool is_fatal = false);
    
    /**
     * @brief 清理系统资源
     */
    void CleanupSystemResources();
    
    /**
     * @brief 检查模块依赖关系
     * @return true 依赖满足，false 依赖缺失
     */
    bool CheckModuleDependencies() const;
    
    // ==================== 工作区管理辅助方法 ====================
    
    /**
     * @brief 加载工作区历史记录
     * @return true 加载成功, false 加载失败
     */
    bool LoadWorkspaceHistory();
    
    /**
     * @brief 保存工作区历史记录
     * @return true 保存成功, false 保存失败
     */
    bool SaveWorkspaceHistory() const;
    
    /**
     * @brief 验证工作区路径是否有效
     * @param workspace_path 工作区路径
     * @return true 有效, false 无效
     */
    bool ValidateWorkspacePath(const QString& workspace_path) const;


private:
    // ==================== 单例相关 ====================
    static std::unique_ptr<MainController> instance_;
    static QMutex instance_mutex_;
    
    // ==================== 核心模块指针 ====================
    ProcessManager* process_manager_; 
    ProjectConfig* project_config_;    // 单例，不使用智能指针管理
    DataStore* data_store_;           
    std::unique_ptr<LogAggregator> log_aggregator_;
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
