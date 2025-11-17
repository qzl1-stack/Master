#include "ProcessManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QCoreApplication>
#include <QThread>

// 静态成员初始化
std::unique_ptr<ProcessManager> ProcessManager::instance_ = nullptr;
QMutex ProcessManager::instance_mutex_;

ProcessManager::ProcessManager(QObject* parent)
    : QObject(parent)
    , heartbeat_timer_(nullptr)
    , monitor_timer_(nullptr)
    , heartbeat_timeout_ms_(30000)      // 默认30秒心跳超时
    , heartbeat_check_interval_ms_(10000)   // 默认10秒检查间隔
    , monitor_check_interval_ms_(5000)      // 默认5秒监控间隔
    , initialized_(false)
{
    qDebug() << "[ProcessManager] 构造函数调用";
}

ProcessManager::~ProcessManager()
{
    qDebug() << "[ProcessManager] 析构函数调用";
    
    // 停止所有进程
    // StopAllProcesses(5000);
    
    // 停止定时器
    if (heartbeat_timer_) {
        heartbeat_timer_->stop();
        heartbeat_timer_->deleteLater();
        heartbeat_timer_ = nullptr;
    }
    
    if (monitor_timer_) {
        monitor_timer_->stop();
        monitor_timer_->deleteLater();
        monitor_timer_ = nullptr;
    }
    
    // 清理所有进程信息
    QMutexLocker locker(&process_mutex_);
    for (auto it = process_info_map_.begin(); it != process_info_map_.end(); ++it) {
        if (it->process) {
            it->process->kill();
            it->process->waitForFinished(1000);
            it->process->deleteLater();
        }
    }
    process_info_map_.clear();
}

ProcessManager& ProcessManager::GetInstance()
{
    QMutexLocker locker(&instance_mutex_);
    if (!instance_) {
        instance_ = std::unique_ptr<ProcessManager>(new ProcessManager());
    }
    return *instance_;
}

bool ProcessManager::Initialize()
{
    QMutexLocker locker(&process_mutex_);
    
    if (initialized_) {
        qDebug() << "[ProcessManager] 已经初始化，跳过";
        return true;
    }
    
    qDebug() << "[ProcessManager] 开始初始化";
    
    // 启动心跳检查定时器
    StartHeartbeatTimer();
    
    // 启动进程监控定时器
    StartMonitorTimer();
    
    initialized_ = true;
    qDebug() << "[ProcessManager] 初始化完成";
    
    return true;
}

bool ProcessManager::AddProcess(const QString& process_id,
                                const QString& executable_path,
                                const QStringList& arguments,
                                const QString& working_directory,
                                bool auto_restart)
{
    QMutexLocker locker(&process_mutex_);

    if (process_info_map_.contains(process_id)) {
        qWarning() << "[ProcessManager] 添加进程失败: 进程ID已存在" << process_id;
        return false;
    }

    ProcessInfo info;
    info.process_id = process_id;
    info.executable_path = executable_path;
    info.arguments = arguments;
    info.working_directory = working_directory;
    info.auto_restart = auto_restart;
    info.status = kNotRunning;
    info.restart_count = 0;
    info.max_restart_count = 5; // 默认最大重启次数
    info.process = nullptr;

    process_info_map_.insert(process_id, info);
    qInfo() << "[ProcessManager] 已成功添加进程:" << process_id;
    return true;
}

bool ProcessManager::StartProcess(const QString& process_id, 
                                const QString& executable_path,
                                const QStringList& arguments,
                                const QString& working_directory,
                                bool auto_restart)
{
    QMutexLocker locker(&process_mutex_);
    
    
    // 检查进程是否已存在
    if (process_info_map_.contains(process_id)) {
        ProcessInfo& info = process_info_map_[process_id];
        if (info.status == kRunning || info.status == kStarting) {
            qWarning() << "[ProcessManager] 进程" << process_id << "已在运行或启动中";
            return false;
        }
    }
    
    if(sender_id_to_process_id_.contains(process_id)){
        qWarning() << "[ProcessManager] 进程" << process_id << "已存在";
        return false;
    }
    qDebug() << "[ProcessManager] 启动进程:" << process_id << "路径:" << executable_path;
    
    // 创建进程信息
    ProcessInfo info;
    info.process_id = process_id;
    info.executable_path = executable_path;
    info.arguments = arguments;
    info.working_directory = working_directory.isEmpty() ? QDir::currentPath() : working_directory;
    info.status = kStarting;
    info.start_time = QDateTime::currentDateTime();
    info.last_heartbeat = QDateTime::currentDateTime();
    info.restart_count = 0;
    info.auto_restart = auto_restart;
    info.max_restart_count = 3;    // 默认最大重启3次
    info.process = CreateQProcess(process_id);
    
    if (!info.process) {
        qWarning() << "[ProcessManager] 创建QProcess失败:" << process_id;
        return false;
    }
    
    // 设置工作目录
    info.process->setWorkingDirectory(info.working_directory);
    
    // 启动进程
    info.process->start(executable_path, arguments);
    
    if (!info.process->waitForStarted(5000)) {
        qWarning() << "[ProcessManager] 进程启动超时:" << process_id;
        info.process->deleteLater();
        UpdateProcessStatus(process_id, kError);
        return false;
    }
    info.pid = info.process->processId();
    // 保存进程信息
    process_info_map_[process_id] = info;
    
    qDebug() << "[ProcessManager] 进程启动成功:" << process_id << "PID:" << info.pid << "status:" << info.status;
    
    return true;
}

bool ProcessManager::StopProcess(const QString& process_id, bool force_kill, int timeout_ms)
{
    QMutexLocker locker(&process_mutex_);
    
    auto it = process_info_map_.find(process_id);
    if (it == process_info_map_.end()) {
        qWarning() << "[ProcessManager] 进程不存在:" << process_id;
        return false;
    }
    
    ProcessInfo& info = it.value();
    
    if (info.status == kNotRunning || info.status == kStopping) {
        qDebug() << "[ProcessManager] 进程已停止或正在停止:" << process_id;
        return true;
    }
    
    qDebug() << "[ProcessManager] 停止进程:" << process_id << "强制杀死:" << force_kill;
    
    UpdateProcessStatus(process_id, kStopping);
    
    if (!info.process) {
        qWarning() << "[ProcessManager] QProcess对象为空:" << process_id;
        UpdateProcessStatus(process_id, kNotRunning);
        return false;
    }
    
    // 禁用自动重启
    info.auto_restart = false;
    
    if (force_kill) {
        // 强制杀死进程
        info.process->kill();
    } else {
        // 优雅停止进程
        info.process->terminate();
    }
    
    // 等待进程结束
    bool finished = info.process->waitForFinished(timeout_ms);
    
    if (!finished && !force_kill) {
        qWarning() << "[ProcessManager] 进程优雅停止超时，强制杀死:" << process_id;
        info.process->kill();
        finished = info.process->waitForFinished(2000);
    }
    
    if (!finished) {
        qWarning() << "[ProcessManager] 进程强制杀死超时:" << process_id;
        return false;
    }
    
    qDebug() << "[ProcessManager] 进程停止成功:" << process_id;
    return true;
}

bool ProcessManager::RestartProcess(const QString& process_id)
{
    QMutexLocker locker(&process_mutex_);
    
    auto it = process_info_map_.find(process_id);
    if (it == process_info_map_.end()) {
        qWarning() << "[ProcessManager] 重启失败，进程不存在:" << process_id;
        return false;
    }
    
    ProcessInfo& info = it.value();
    
    qDebug() << "[ProcessManager] 重启进程:" << process_id;
    
    // 先停止进程（不修改process_mutex_的锁状态）
    if (info.process && info.status != kNotRunning) {
        info.auto_restart = false;  // 临时禁用自动重启
        info.process->terminate();
        if (!info.process->waitForFinished(5000)) {
            info.process->kill();
            info.process->waitForFinished(2000);
        }
    }
    
    // 重新启动
    locker.unlock(); // 释放锁，避免递归锁定
    bool success = StartProcess(process_id, info.executable_path, info.arguments, 
                              info.working_directory, true); // 重启时启用自动重启
    
    if (success) {
        locker.relock();
        auto restart_it = process_info_map_.find(process_id);
        if (restart_it != process_info_map_.end()) {
            restart_it->restart_count++;
            emit ProcessAutoRestarted(process_id, restart_it->restart_count);
        }
    }
    
    return success;
}

ProcessManager::ProcessStatus ProcessManager::GetProcessStatus(const QString& process_id) const
{
    QMutexLocker locker(&process_mutex_);
    
    auto it = process_info_map_.find(process_id);
    if (it == process_info_map_.end()) {
        return kNotRunning;
    }
    
    return it->status;
}

const ProcessManager::ProcessInfo* ProcessManager::GetProcessInfo(const QString& process_id) const
{
    QMutexLocker locker(&process_mutex_);
    
    auto it = process_info_map_.find(process_id);
    if (it == process_info_map_.end()) {
        return nullptr;
    }
    
    return &(it.value());
}

QStringList ProcessManager::GetProcessList() const
{
    QMutexLocker locker(&process_mutex_);
    return process_info_map_.keys();
}

QStringList ProcessManager::GetRunningProcessList() const
{
    QMutexLocker locker(&process_mutex_);
    
    QStringList running_processes;
    for (auto it = process_info_map_.constBegin(); it != process_info_map_.constEnd(); ++it) {
        if (it->status == kRunning) {
            running_processes.append(it.key());
        }
    }
    return running_processes;
}

bool ProcessManager::StopAllProcesses(int timeout_ms)
{
    qDebug() << "[ProcessManager] 停止所有进程";

    QStringList process_list;
    QList<QProcess*> proc_ptrs;

    // 1) 收集需要停止的进程，并发送 terminate（短锁）
    {
        QMutexLocker locker(&process_mutex_);
        for (auto it = process_info_map_.begin(); it != process_info_map_.end(); ++it) {
            if (it->status == kRunning && it->process) {
                process_list.append(it.key());
                it->auto_restart = false;
                it->process->terminate();
                UpdateProcessStatus(it.key(), kStopping);
                proc_ptrs.append(it->process);
            }
        }
    }

    if (process_list.isEmpty()) return true;
    qDebug() << "[ProcessManager] 停止所有进程列表:" << process_list;

    // 2) 等待优雅退出（不持锁）
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeout_ms) {
        bool all_stopped = true;
        for (const auto& proc : proc_ptrs) {
            if (proc && proc->state() != QProcess::NotRunning) {
                all_stopped = false;
                break;
            }
        }
        if (all_stopped) {
            qDebug() << "[ProcessManager] 所有进程已优雅停止";
            return true;
        }
        QCoreApplication::processEvents();
        QThread::msleep(100);
    }

    // 3) 超时后强杀仍在运行的进程（短锁）
    qWarning() << "[ProcessManager] 优雅停止超时，强制杀死剩余进程";
    {
        QMutexLocker locker(&process_mutex_);
        for (const QString& process_id : process_list) {
            auto it = process_info_map_.find(process_id);
            if (it != process_info_map_.end() && it->process) {
                if (it->process->state() != QProcess::NotRunning) {
                    it->process->kill();
                }
            }
        }
    }
    return true;
}

void ProcessManager::UpdateHeartbeat(const QString& sender_id)
{
    QMutexLocker locker(&process_mutex_);
    
    auto it = process_info_map_.find(sender_id);
    if (it != process_info_map_.end()) {
        it->last_heartbeat = QDateTime::currentDateTime();
    }
}

void ProcessManager::SetHeartbeatTimeout(int timeout_ms)
{
    QMutexLocker locker(&process_mutex_);
    heartbeat_timeout_ms_ = timeout_ms;
    qDebug() << "[ProcessManager] 设置心跳超时时间:" << timeout_ms << "ms";
}

int ProcessManager::GetHeartbeatTimeout() const
{
    QMutexLocker locker(&process_mutex_);
    return heartbeat_timeout_ms_;
}

void ProcessManager::SetMaxRestartCount(const QString& process_id, int max_count)
{
    QMutexLocker locker(&process_mutex_);
    
    auto it = process_info_map_.find(process_id);
    if (it != process_info_map_.end()) {
        it->max_restart_count = max_count;
        qDebug() << "[ProcessManager] 设置进程" << process_id << "最大重启次数:" << max_count;
    }
}

void ProcessManager::CleanupStoppedProcesses()
{
    QMutexLocker locker(&process_mutex_);
    
    QStringList to_remove;
    for (auto it = process_info_map_.begin(); it != process_info_map_.end(); ++it) {
        if (it->status == kNotRunning && it->process) {
            if (it->process->state() == QProcess::NotRunning) {
                to_remove.append(it.key());
            }
        }
    }
    
    for (const QString& process_id : to_remove) {
        CleanupProcessInfo(process_id);
        qDebug() << "[ProcessManager] 清理已停止进程:" << process_id;
    }
}

void ProcessManager::HandleProcessStarted()
{
    QProcess* process = qobject_cast<QProcess*>(sender());
    if (!process) {
        return;
    }
    
    QString process_id = GetProcessIdByQProcess(process);
    if (process_id.isEmpty()) {
        return;
    }
    
    qDebug() << "[ProcessManager] 进程启动成功:" << process_id << "PID:" << process->processId();
    
    UpdateProcessStatus(process_id, kRunning);
    emit ProcessStarted(process_id);
}

void ProcessManager::HandleProcessFinished(int exit_code, QProcess::ExitStatus exit_status)
{
    QProcess* process = qobject_cast<QProcess*>(sender());
    if (!process) {
        return;
    }
    
    QString process_id = GetProcessIdByQProcess(process);
    if (process_id.isEmpty()) {
        return;
    }
    
    qDebug() << "[ProcessManager] 进程结束:" << process_id << "退出码:" << exit_code 
             << "退出状态:" << (exit_status == QProcess::NormalExit ? "正常" : "崩溃");
    
    QMutexLocker locker(&process_mutex_);
    auto it = process_info_map_.find(process_id);
    if (it == process_info_map_.end()) {
        return;
    }
    
    ProcessInfo& info = it.value();
    
    if (exit_status == QProcess::CrashExit) {
        UpdateProcessStatus(process_id, kCrashed);
        emit ProcessCrashed(process_id, QString("进程崩溃，退出码: %1").arg(exit_code));
        
        // 检查是否需要自动重启
        if (info.auto_restart && info.restart_count < info.max_restart_count) {
            qDebug() << "[ProcessManager] 准备自动重启崩溃的进程:" << process_id;
            locker.unlock();
            ExecuteAutoRestart(process_id);
            return;
        }
    } else {
        UpdateProcessStatus(process_id, kNotRunning);
    }
    
    emit ProcessStopped(process_id, exit_code);
}

void ProcessManager::HandleProcessError(QProcess::ProcessError error)
{
    QProcess* process = qobject_cast<QProcess*>(sender());
    if (!process) {
        return;
    }
    
    QString process_id = GetProcessIdByQProcess(process);
    if (process_id.isEmpty()) {
        return;
    }
    
    QString error_string;
    switch (error) {
        case QProcess::FailedToStart:
            error_string = "启动失败";
            break;
        case QProcess::Crashed:
            error_string = "运行时崩溃";
            break;
        case QProcess::Timedout:
            error_string = "操作超时";
            break;
        case QProcess::WriteError:
            error_string = "写入错误";
            break;
        case QProcess::ReadError:
            error_string = "读取错误";
            break;
        case QProcess::UnknownError:
        default:
            error_string = "未知错误";
            break;
    }
    
    qWarning() << "[ProcessManager] 进程错误:" << process_id << error_string;
    
    UpdateProcessStatus(process_id, kError);
    emit ProcessCrashed(process_id, error_string);
    
}

void ProcessManager::HandleProcessStandardOutput()
{
    QProcess* process = qobject_cast<QProcess*>(sender());
    if (!process) {
        return;
    }
    
    QString process_id = GetProcessIdByQProcess(process);
    if (process_id.isEmpty()) {
        return;
    }
    
    QByteArray data = process->readAllStandardOutput();
    QString output = QString::fromUtf8(data);
    
    if (!output.isEmpty()) {
        emit ProcessOutput(process_id, output);
    }
}

void ProcessManager::HandleProcessStandardError()
{
    QProcess* process = qobject_cast<QProcess*>(sender());
    if (!process) {
        return;
    }
    
    QString process_id = GetProcessIdByQProcess(process);
    if (process_id.isEmpty()) {
        return;
    }
    
    QByteArray data = process->readAllStandardError();
    QString error_output = QString::fromUtf8(data);
    
    if (!error_output.isEmpty()) {
        emit ProcessErrorOutput(process_id, error_output);
    }
}

void ProcessManager::CheckHeartbeat()
{
    QMutexLocker locker(&process_mutex_);
    
    QDateTime current_time = QDateTime::currentDateTime();
    QStringList timeout_processes;
    
    for (auto it = process_info_map_.begin(); it != process_info_map_.end(); ++it) {
        if (it->status == kRunning) {
            qint64 elapsed_ms = it->last_heartbeat.msecsTo(current_time);
            qDebug() << "elapsed_ms:" << elapsed_ms;
            qDebug() << "heartbeat_timeout_ms_:" << heartbeat_timeout_ms_;
            if (elapsed_ms > heartbeat_timeout_ms_) {
                timeout_processes.append(it.key());
            }
        }
    }
    
    // 处理心跳超时的进程
    for (const QString& process_id : timeout_processes) {
        qWarning() << "[ProcessManager] 进程心跳超时:" << process_id;
        emit HeartbeatTimeout(process_id);
        
        auto it = process_info_map_.find(process_id);
        if (it != process_info_map_.end()) {
            ProcessInfo& info = it.value();
            
            // 如果启用自动重启且重启次数未超限，则重启进程
            if (info.auto_restart && info.restart_count < info.max_restart_count) {
                qDebug() << "[ProcessManager] 心跳超时，准备自动重启进程:" << process_id;
                locker.unlock();
                ExecuteAutoRestart(process_id);
                locker.relock();
            }
        }
    }
}

void ProcessManager::MonitorProcesses()
{
    QMutexLocker locker(&process_mutex_);
    
    for (auto it = process_info_map_.begin(); it != process_info_map_.end(); ++it) {
        if (it->process) {
            QProcess::ProcessState state = it->process->state();
            ProcessStatus current_status = it->status;
            
            // 根据QProcess状态更新进程状态
            switch (state) {
                case QProcess::NotRunning:
                    if (current_status != kNotRunning && current_status != kStopping) {
                        qDebug() << "[ProcessManager] 检测到进程意外停止:" << it.key();
                        UpdateProcessStatus(it.key(), kNotRunning);
                    }
                    break;
                case QProcess::Starting:
                    if (current_status != kStarting) {
                        UpdateProcessStatus(it.key(), kStarting);
                    }
                    break;
                case QProcess::Running:
                    if (current_status != kRunning) {
                        UpdateProcessStatus(it.key(), kRunning);
                    }
                    break;
            }
        }
    }
}

QString ProcessManager::GetProcessIdByQProcess(QProcess* process) const
{
    for (auto it = process_info_map_.constBegin(); it != process_info_map_.constEnd(); ++it) {
        if (it->process == process) {
            return it.key();
        }
    }
    return QString();
}

QProcess* ProcessManager::CreateQProcess(const QString& process_id)
{
    QProcess* process = new QProcess(this);
    
    // 连接信号槽
    connect(process, &QProcess::started, this, &ProcessManager::HandleProcessStarted);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProcessManager::HandleProcessFinished);
    connect(process, &QProcess::errorOccurred, this, &ProcessManager::HandleProcessError);
    connect(process, &QProcess::readyReadStandardOutput, this, &ProcessManager::HandleProcessStandardOutput);
    connect(process, &QProcess::readyReadStandardError, this, &ProcessManager::HandleProcessStandardError);
    
    qDebug() << "[ProcessManager] 创建QProcess对象:" << process_id;
    
    return process;
}

void ProcessManager::StartHeartbeatTimer()
{
    if (!heartbeat_timer_) {
        heartbeat_timer_ = new QTimer(this);
        connect(heartbeat_timer_, &QTimer::timeout, this, &ProcessManager::CheckHeartbeat);
    }
    
    heartbeat_timer_->start(heartbeat_check_interval_ms_);
    qDebug() << "[ProcessManager] 启动心跳检查定时器，间隔:" << heartbeat_check_interval_ms_ << "ms";
}

void ProcessManager::StartMonitorTimer()
{
    if (!monitor_timer_) {
        monitor_timer_ = new QTimer(this);
        connect(monitor_timer_, &QTimer::timeout, this, &ProcessManager::MonitorProcesses);
    }
    
    monitor_timer_->start(monitor_check_interval_ms_);
    qDebug() << "[ProcessManager] 启动进程监控定时器，间隔:" << monitor_check_interval_ms_ << "ms";
}

bool ProcessManager::ExecuteAutoRestart(const QString& process_id)
{
    QMutexLocker locker(&process_mutex_);
    
    auto it = process_info_map_.find(process_id);
    if (it == process_info_map_.end()) {
        qWarning() << "[ProcessManager] 自动重启失败，进程不存在:" << process_id;
        return false;
    }
    
    ProcessInfo& info = it.value();
    
    if (info.restart_count >= info.max_restart_count) {
        qWarning() << "[ProcessManager] 进程" << process_id << "重启次数已达上限:" << info.restart_count;
        return false;
    }
    
    qDebug() << "[ProcessManager] 执行自动重启:" << process_id << "当前重启次数:" << info.restart_count;
    
    QString executable_path = info.executable_path;
    QStringList arguments = info.arguments;
    QString working_directory = info.working_directory;
    bool auto_restart = info.auto_restart;
    
    locker.unlock();
    
    // 重启进程
    bool success = RestartProcess(process_id);
    
    if (success) {
        qDebug() << "[ProcessManager] 自动重启成功:" << process_id;
    } else {
        qWarning() << "[ProcessManager] 自动重启失败:" << process_id;
    }
    
    return success;
}

void ProcessManager::UpdateProcessStatus(const QString& process_id, ProcessStatus new_status)
{
    auto it = process_info_map_.find(process_id);
    if (it != process_info_map_.end()) {
        ProcessStatus old_status = it->status;
        if (old_status != new_status) {
            it->status = new_status;
            
            qDebug() << "[ProcessManager] 进程状态变化:" << process_id 
                     << "从" << old_status << "到" << new_status;
            
            emit ProcessStatusChanged(process_id, old_status, new_status);
        }
    }
}

void ProcessManager::CleanupProcessInfo(const QString& process_id)
{
    auto it = process_info_map_.find(process_id);
    if (it != process_info_map_.end()) {
        if (it->process) {
            it->process->deleteLater();
        }
        process_info_map_.erase(it);
        qDebug() << "[ProcessManager] 清理进程信息:" << process_id;
    }
}
