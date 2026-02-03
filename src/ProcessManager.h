#ifndef MASTER_SRC_PROCESS_MANAGER_H_
#define MASTER_SRC_PROCESS_MANAGER_H_

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QString>
#include <QStringList>
#include <QMutex>
#include <QMutexLocker>
#include <QHash>
#include <QDateTime>
#include <memory>

/**
 * @brief ProcessManager 进程生命周期管理类
 * 
 * 负责子进程的生命周期管理，包括启动、停止、重启、状态监控。
 * 通过QProcess实现，支持异常检测与自动重启。
 * 采用单例模式确保进程管理全局唯一。
 * 
 * 主要功能：
 * - 子进程启动、停止、重启管理
 * - 进程状态监控和心跳检测
 * - 异常检测与自动重启机制
 * - 进程资源监控
 * - 优雅的进程退出机制
 */
class ProcessManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 进程状态枚举
     */
    enum ProcessStatus {
        kNotRunning = 0,    ///< 未运行
        kStarting,          ///< 启动中
        kRunning,           ///< 运行中
        kStopping,          ///< 停止中
        kError,             ///< 错误状态
        kCrashed            ///< 已崩溃
    };

    /**
     * @brief 进程信息结构体
     */
    struct ProcessInfo {
        QString process_id;             ///< 子进程名称（自定义标识符）
        QString executable_path;        ///< 可执行文件路径
        QStringList arguments;          ///< 启动参数
        QString working_directory;      ///< 工作目录
        ProcessStatus status;           ///< 进程状态
        QDateTime start_time;           ///< 启动时间
        QDateTime last_heartbeat;       ///< 最后心跳时间
        int restart_count;              ///< 重启次数
        bool auto_restart;              ///< 是否自动重启
        int max_restart_count;          ///< 最大重启次数
        QProcess* process;              ///< QProcess对象指针
        int pid;                         ///< 进程ID
    };

public: 
    static ProcessManager& GetInstance();

    // 禁用拷贝构造和赋值操作
    ProcessManager(const ProcessManager&) = delete;
    ProcessManager& operator=(const ProcessManager&) = delete;

    bool Initialize();

    bool AddProcess(const QString& process_id,
                    const QString& executable_path,
                    const QStringList& arguments = QStringList(),
                    const QString& working_directory = QString(),
                    bool auto_restart = true);

    /**
     * @brief 启动进程
     * @param process_id 进程标识符
     * @param executable_path 可执行文件路径
     * @param arguments 启动参数
     */
    bool StartProcess(const QString& process_id, 
                     const QString& executable_path,
                     const QStringList& arguments = QStringList(),
                     const QString& working_directory = QString(),
                     bool auto_restart = false);

    /**
     * @brief 停止进程
     * @param process_id 进程标识符
     * @param force_kill 是否强制杀死进程
     * @param timeout_ms 超时时间（毫秒）
     * @return 停止是否成功
     */
    bool StopProcess(const QString& process_id, bool force_kill = false, int timeout_ms = 5000);

    /**
     * @brief 重启进程
     * @param process_id 进程标识符
     * @return 重启是否成功
     */
    bool RestartProcess(const QString& process_id);

    /**
     * @brief 获取进程状态
     * @param process_id 进程标识符
     * @return 进程状态
     */
    ProcessStatus GetProcessStatus(const QString& process_id) const;

    /**
     * @brief 获取进程信息
     * @param process_id 进程标识符
     * @return 进程信息（如果进程不存在则返回空指针）
     */
    const ProcessInfo* GetProcessInfo(const QString& process_id) const;

    /**
     * @brief 获取所有进程列表
     * @return 进程ID列表
     */
    QStringList GetProcessList() const;

    /**
     * @brief 获取运行中的进程列表
     * @return 运行中进程ID列表
     */
    QStringList GetRunningProcessList() const;

    /**
     * @brief 停止所有进程
     * @param timeout_ms 超时时间（毫秒）
     * @return 停止是否成功
     */
    bool StopAllProcesses(int timeout_ms = 10000);

    /**
     * @brief 更新进程心跳
     * @param process_id 进程标识符
     */
    void UpdateHeartbeat(const QString& sender_id);

    /**
     * @brief 设置心跳超时时间
     * @param timeout_ms 超时时间（毫秒）
     */
    void SetHeartbeatTimeout(int timeout_ms);

    /**
     * @brief 获取心跳超时时间
     * @return 心跳超时时间（毫秒）
     */
    int GetHeartbeatTimeout() const;

    /**
     * @brief 设置自动重启最大次数
     * @param process_id 进程标识符
     * @param max_count 最大重启次数
     */
    void SetMaxRestartCount(const QString& process_id, int max_count);

    /**
     * @brief 清理已停止的进程
     */
    void CleanupStoppedProcesses();

signals:
    /**
     * @brief 进程状态改变信号
     * @param process_id 进程标识符
     * @param old_status 旧状态
     * @param new_status 新状态
     */
    void ProcessStatusChanged(const QString& process_id, ProcessStatus old_status, ProcessStatus new_status);

    /**
     * @brief 进程启动信号
     * @param process_id 进程标识符
     */
    void ProcessStarted(const QString& process_id);

    /**
     * @brief 进程停止信号
     * @param process_id 进程标识符
     * @param exit_code 退出码
     */
    void ProcessStopped(const QString& process_id, int exit_code);

    /**
     * @brief 进程崩溃信号
     * @param process_id 进程标识符
     * @param error 错误信息
     */
    void ProcessCrashed(const QString& process_id, const QString& error);

    /**
     * @brief 进程自动重启信号
     * @param process_id 进程标识符
     * @param restart_count 重启次数
     */
    void ProcessAutoRestarted(const QString& process_id, int restart_count);

    /**
     * @brief 心跳超时信号
     * @param process_id 进程标识符
     */
    void HeartbeatTimeout(const QString& process_id);

    /**
     * @brief 进程输出信号
     * @param process_id 进程标识符
     * @param output 输出内容
     */
    void ProcessOutput(const QString& process_id, const QString& output);

    /**
     * @brief 进程错误输出信号
     * @param process_id 进程标识符
     * @param error_output 错误输出内容
     */
    void ProcessErrorOutput(const QString& process_id, const QString& error_output);

private slots:
    /**
     * @brief 处理进程启动
     */
    void HandleProcessStarted();

    /**
     * @brief 处理进程结束
     * @param exit_code 退出码
     * @param exit_status 退出状态
     */
    void HandleProcessFinished(int exit_code, QProcess::ExitStatus exit_status);

    /**
     * @brief 处理进程错误
     * @param error 错误类型
     */
    void HandleProcessError(QProcess::ProcessError error);

    /**
     * @brief 处理进程标准输出
     */
    void HandleProcessStandardOutput();

    /**
     * @brief 处理进程错误输出
     */
    void HandleProcessStandardError();

    /**
     * @brief 心跳检查定时器槽函数
     */
    void CheckHeartbeat();

    /**
     * @brief 进程监控定时器槽函数
     */
    void MonitorProcesses();

private:
    /**
     * @brief 私有构造函数（单例模式）
     */
    explicit ProcessManager(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ProcessManager() override;

    // 友元类，允许unique_ptr的默认删除器访问私有析构函数
    friend class std::default_delete<ProcessManager>;

    /**
     * @brief 获取进程ID通过QProcess对象
     * @param process QProcess对象指针
     * @return 进程ID（如果未找到则返回空字符串）
     */
    QString GetProcessIdByQProcess(QProcess* process) const;

    /**
     * @brief 创建QProcess对象
     * @param process_id 进程标识符
     * @return QProcess对象指针
     */
    QProcess* CreateQProcess(const QString& process_id);

    /**
     * @brief 启动心跳检查定时器
     */
    void StartHeartbeatTimer();

    /**
     * @brief 启动进程监控定时器
     */
    void StartMonitorTimer();

    /**
     * @brief 执行自动重启
     * @param process_id 进程标识符
     * @return 重启是否成功
     */
    bool ExecuteAutoRestart(const QString& process_id);

    /**
     * @brief 更新进程状态
     * @param process_id 进程标识符
     * @param new_status 新状态
     */
    void UpdateProcessStatus(const QString& process_id, ProcessStatus new_status);

    /**
     * @brief 清理进程信息
     * @param process_id 进程标识符
     */
    void CleanupProcessInfo(const QString& process_id);

private:
    static std::unique_ptr<ProcessManager> instance_;     ///< 单例实例
    static QMutex instance_mutex_;                        ///< 单例创建互斥锁

    mutable QMutex process_mutex_;                        ///< 进程信息访问互斥锁
    QHash<QString, ProcessInfo> process_info_map_;        ///< config文件中进程ID与ProcessInfo的映射表
    QHash<QString, QString> sender_id_to_process_id_;     ///< 发送者ID与进程ID的映射表

    QTimer* heartbeat_timer_;                             ///< 心跳检查定时器
    QTimer* monitor_timer_;                               ///< 进程监控定时器

    int heartbeat_timeout_ms_;                            ///< 心跳超时时间（毫秒）
    int heartbeat_check_interval_ms_;                     ///< 心跳检查间隔（毫秒）
    int monitor_check_interval_ms_;                       ///< 监控检查间隔（毫秒）

    bool initialized_;                                    ///< 是否已初始化
};

#endif // MASTER_SRC_PROCESS_MANAGER_H_
