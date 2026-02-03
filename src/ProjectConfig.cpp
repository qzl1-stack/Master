#include "ProjectConfig.h"
#include <QDir>
#include <QFile>
#include <QJsonParseError>
#include <QStandardPaths>
#include <QDebug>
#include <QDateTime>
#include <QFileInfo>
#include <QCoreApplication>
// 静态成员初始化
std::unique_ptr<ProjectConfig> ProjectConfig::instance_ = nullptr;
QMutex ProjectConfig::instance_mutex_;

ProjectConfig::ProjectConfig(QObject *parent)
    : QObject(parent)
    , file_watcher_(nullptr)
    , config_loaded_(false)
    , hot_update_enabled_(true)
{
    // 初始化文件系统监视器
    file_watcher_ = new QFileSystemWatcher(this);
    connect(file_watcher_, &QFileSystemWatcher::fileChanged,
            this, &ProjectConfig::handleConfigFileChanged);
}

ProjectConfig::~ProjectConfig()
{
    if (file_watcher_) {
        file_watcher_->deleteLater();
    }
}

ProjectConfig& ProjectConfig::getInstance()
{
    QMutexLocker locker(&instance_mutex_);
    if (!instance_) {
        instance_ = std::unique_ptr<ProjectConfig>(new ProjectConfig());
    }
    return *instance_;
}

bool ProjectConfig::initialize(const QString& configFilePath)
{
    QMutexLocker locker(&config_mutex_);

    config_file_path_ = configFilePath;

    // 确保配置目录和文件存在
    if (!ensureConfigFileExists(config_file_path_)) {
        return false;
    }
    
    // 尝试加载配置文件
    if (!loadConfig(config_file_path_)) {
        qWarning() << "[ProjectConfig] 加载配置文件失败，创建默认配置";
        
        // 创建默认配置
        config_ = createDefaultConfig();
        qWarning() << "[ProjectConfig] 创建默认配置成功";
        // 不再在此处保存，由调用者决定何时保存
    }
    
    // 添加文件监视
    if (!file_watcher_->files().contains(config_file_path_)) {
        file_watcher_->addPath(config_file_path_);
    }
    
    // config_loaded_ = false;
    
    return true;
}

bool ProjectConfig::loadConfig(const QString& filePath)
{
    QString path = filePath.isEmpty() ? config_file_path_ : filePath;
    
    QFile file(path);
    if (!file.exists()) {
        qWarning() << "Config file does not exist:" << path;
        return false;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open config file:" << path << file.errorString();
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse config JSON:" << parseError.errorString();
        return false;
    }
    
    if (!doc.isObject()) {
        qWarning() << "Config file is not a JSON object";
        return false;
    }
    
    QJsonObject newConfig = doc.object();
    
    // 验证配置格式
    if (!validateConfig(newConfig)) {
        qWarning() << "Config validation failed";
        return false;
    }
    
    config_ = newConfig;
    qInfo() << "Config loaded successfully from:" << path;
    config_loaded_ = true;
    
    return true;
}

// ProjectConfig.cpp 需要修改的保存逻辑
bool ProjectConfig::saveConfig(const QString& filePath)
{
    QString path = filePath.isEmpty() ? config_file_path_ : filePath;
    
    // 目录应该已经在 ensureConfigFileExists 中创建，这里不需要再次创建
    QFileInfo fileInfo(path);
    QDir configDir = fileInfo.absoluteDir();


    // 创建并打开文件
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) { // 强制截断现有文件
        qCritical() << "[ProjectConfig] 创建配置文件失败: "
                   << path
                   << "错误信息: " << file.errorString();
        return false;
    }
    
    // 写入内容
    QJsonDocument doc(config_);
    QByteArray jsonData = doc.toJson(QJsonDocument::Indented); // 使用缩进格式，方便调试
    if(file.write(jsonData) == -1) {
        qCritical() << "[ProjectConfig] 写入配置文件失败: "
                   << path
                   << "错误信息: " << file.errorString();
        file.close(); // 确保文件关闭
        return false;
    }
    file.close(); // 确保文件关闭
    qInfo() << "[ProjectConfig] 配置文件创建成功: " << path;
    return true;
}

bool ProjectConfig::hotUpdateConfig(const QJsonObject& newConfig)
{
    QMutexLocker locker(&config_mutex_);
    
    if (!hot_update_enabled_) {
        qWarning() << "Hot update is disabled";
        return false;
    }
    
    // 验证新配置
    if (!validateConfig(newConfig)) {
        qWarning() << "New config validation failed";
        emit hotUpdateCompleted(false);
        return false;
    }
    
    // 比较配置变化并发送信号
    for (auto it = newConfig.begin(); it != newConfig.end(); ++it) {
        const QString& key = it.key();
        const QJsonValue& newValue = it.value();
        const QJsonValue oldValue = config_.value(key);
        
        if (oldValue != newValue) {
            config_[key] = newValue;
            emitConfigUpdated(key, oldValue, newValue);
        }
    }
    
    // 保存到文件
    bool success = saveConfig();
    emit hotUpdateCompleted(success);
    
    qInfo() << "Hot update" << (success ? "completed successfully" : "failed");
    return success;
}

QStringList ProjectConfig::getIpTable() const
{
    QMutexLocker locker(&config_mutex_);
    
    QJsonArray ipArray = config_.value("ip_table").toArray();
    QStringList ipList;
    
    for (const QJsonValue& value : ipArray) {
        ipList.append(value.toString());
    }
    
    return ipList;
}

void ProjectConfig::setIpTable(const QStringList& ipList)
{
    QMutexLocker locker(&config_mutex_);
    
    QJsonArray ipArray;
    for (const QString& ip : ipList) {
        ipArray.append(ip);
    }
    
    QJsonValue oldValue = config_.value("ip_table");
    config_["ip_table"] = ipArray;
    
    emitConfigUpdated("ip_table", oldValue, ipArray);
}

QStringList ProjectConfig::getProcessList() const
{
    QMutexLocker locker(&config_mutex_);
    
    QJsonArray processArray = config_.value("process_list").toArray();
    QStringList processList;
    
    for (const QJsonValue& value : processArray) {
        processList.append(value.toString());
    }
    
    return processList;
}

void ProjectConfig::setProcessList(const QStringList& processList)
{
    QMutexLocker locker(&config_mutex_);
    
    QJsonArray processArray;
    for (const QString& process : processList) {
        processArray.append(process);
    }
    
    QJsonValue oldValue = config_.value("process_list");
    config_["process_list"] = processArray;
    
    emitConfigUpdated("process_list", oldValue, processArray);
}

QString ProjectConfig::getWorkDirectory() const
{
    QMutexLocker locker(&config_mutex_);
    return config_.value("work_directory").toString();
}

void ProjectConfig::setWorkDirectory(const QString& workDir)
{
    QMutexLocker locker(&config_mutex_);
    
    QJsonValue oldValue = config_.value("work_directory");
    config_["work_directory"] = workDir;
    
    emitConfigUpdated("work_directory", oldValue, workDir);
}

QJsonObject ProjectConfig::getNetworkParams() const
{
    QMutexLocker locker(&config_mutex_);
    return config_.value("network_params").toObject();
}

void ProjectConfig::setNetworkParams(const QJsonObject& params)
{
    QMutexLocker locker(&config_mutex_);
    
    QJsonValue oldValue = config_.value("network_params");
    config_["network_params"] = params;
    
    emitConfigUpdated("network_params", oldValue, params);
}

QString ProjectConfig::getConfigVersion() const
{
    QMutexLocker locker(&config_mutex_);
    return config_.value("config_version").toString();
}

void ProjectConfig::setConfigVersion(const QString& version)
{
    QMutexLocker locker(&config_mutex_);
    
    QJsonValue oldValue = config_.value("config_version");
    config_["config_version"] = version;
    
    emitConfigUpdated("config_version", oldValue, version);
}

QJsonValue ProjectConfig::getConfigValue(const QString& key, const QJsonValue& defaultValue) const
{
    QMutexLocker locker(&config_mutex_);
    // return config_.value(key, defaultValue);
    return config_.value(key);
}

void ProjectConfig::setConfigValue(const QString& key, const QJsonValue& value)
{
    QMutexLocker locker(&config_mutex_);
    
    QJsonValue oldValue = config_.value(key);
    config_[key] = value;
    emitConfigUpdated(key, oldValue, value);
}

QJsonObject ProjectConfig::getFullConfig() const
{
    QMutexLocker locker(&config_mutex_);
    return config_;
}

bool ProjectConfig::isConfigLoaded() const
{
    QMutexLocker locker(&config_mutex_);
    return config_loaded_;
}

void ProjectConfig::handleConfigFileChanged(const QString& filePath)
{
    qInfo() << "Config file changed:" << filePath;
    
    if (hot_update_enabled_) {
        // 重新加载配置文件
        if (loadConfig(filePath)) {
            emit configFileChanged(filePath);
            qInfo() << "Config reloaded due to file change";
        } else {
            qWarning() << "Failed to reload config after file change";
        }
    }
}

QJsonObject ProjectConfig::createDefaultConfig()
{
    QJsonObject defaultConfig;
    
    // 基本信息
    defaultConfig["config_version"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    defaultConfig["created"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    defaultConfig["last_modified"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    // IP表
    QJsonArray defaultIpTable;
    defaultConfig["ip_table"] = defaultIpTable;
    
    // 子进程列表
    QJsonArray defaultProcessList;
    defaultProcessList.append("文件传输");
    defaultProcessList.append("AGV分析");
    defaultConfig["process_list"] = defaultProcessList;

    // 子进程配置
    QJsonObject processesConfig;
    QJsonObject sftpAgentConfig;
    sftpAgentConfig["executable"] = QCoreApplication::applicationDirPath() + "/VTA.exe"; // 假设子进程的可执行文件
    sftpAgentConfig["arguments"] = QJsonArray(); // 空参数
    processesConfig["文件传输"] = sftpAgentConfig;

    QJsonObject logAgentConfig;
    QString logAgentExecutable = QCoreApplication::applicationDirPath() + "/appLog_analyzer.exe";
    logAgentConfig["executable"] = logAgentExecutable; // 假设log_agent的可执行文件
    logAgentConfig["arguments"] = QJsonArray(); // 空参数
    processesConfig["AGV分析"] = logAgentConfig;
    defaultConfig["processes"] = processesConfig;

    // QJsonObject serialAgentConfig;
    // QString serialAgentExecutable = QCoreApplication::applicationDirPath() + "/SerialMate.exe";
    // serialAgentConfig["executable"] = serialAgentExecutable; // 假设serial_agent的可执行文件
    // serialAgentConfig["arguments"] = QJsonArray(); // 空参数
    // processesConfig["串口分析"] = serialAgentConfig;
    // defaultConfig["processes"] = processesConfig;

    // QJsonObject uartAgentConfig;
    // QString uartAgentExecutable = QCoreApplication::applicationDirPath() + "/wu_xc_uart_tool.exe";
    // uartAgentConfig["executable"] = uartAgentExecutable; // 假设serial_agent的可执行文件
    // uartAgentConfig["arguments"] = QJsonArray(); // 空参数
    // processesConfig["吴工上位机"] = uartAgentConfig;
    // defaultConfig["processes"] = processesConfig;

    // QJsonObject agv_designConfig;
    // QString agvDesignExecutable = QCoreApplication::applicationDirPath() + "/agv_design_tool.exe";
    // agv_designConfig["executable"] = agvDesignExecutable; // 假设serial_agent的可执行文件
    // agv_designConfig["arguments"] = QJsonArray(); // 空参数
    // processesConfig["重庆AGV设计"] = agv_designConfig;
    // defaultConfig["processes"] = processesConfig;


    // 工作目录
    QString defaultWorkDir = QCoreApplication::applicationDirPath();
    defaultConfig["work_directory"] = defaultWorkDir;

    // 配置目录
    QString defaultConfigDir = config_file_path_;
    defaultConfig["config_directory"] = defaultConfigDir;
    
    // 网络参数
    QJsonObject networkParams;
    networkParams["ipc_server_name"] = "master_ipc_server";
    networkParams["heartbeat_interval"] = 5000;  // 5秒
    networkParams["connection_timeout"] = 30000; // 30秒
    defaultConfig["network_params"] = networkParams;
    
    // IPC配置
    QJsonObject ipcConfig;
    ipcConfig["type"] = "local_socket";
    ipcConfig["local_socket"] = QJsonObject{
        {"server_name", "master_ipc_server"},
        {"max_connections", 100}
    };
    defaultConfig["ipc"] = ipcConfig;
    
    // 日志存储配置
    QJsonObject logStoragesConfig;
    QJsonObject masterProcessLogConfig;
    masterProcessLogConfig["type"] = "file"; // 默认文件存储
    QJsonObject fileLogConfig;
    fileLogConfig["base_dir"] = QCoreApplication::applicationDirPath() + "/logs/master_process";
    fileLogConfig["max_file_size_bytes"] = 10 * 1024 * 1024; // 10MB
    fileLogConfig["max_days_to_keep"] = 30; // 保留30天
    masterProcessLogConfig["config"] = fileLogConfig;
    logStoragesConfig["master_process"] = masterProcessLogConfig;
    defaultConfig["log_storages"] = logStoragesConfig;
    
    return defaultConfig;
}

bool ProjectConfig::validateConfig(const QJsonObject& config)
{
    // 检查必需的配置项
    QStringList requiredKeys = {
        "config_version",
        "ip_table",
        "process_list",
        "config_directory",
        "network_params",
        "processes" // 添加对 processes 键的验证
    };
    
    for (const QString& key : requiredKeys) {
        if (!config.contains(key)) {
            qWarning() << "Missing required config key:" << key;
            return false;
        }
    }
    
    // 验证IP表格式
    if (!config.value("ip_table").isArray()) {
        qWarning() << "ip_table must be an array";
        return false;
    }
    
    // 验证进程列表格式
    if (!config.value("process_list").isArray()) {
        qWarning() << "process_list must be an array";
        return false;
    }
    
    // 验证工作目录格式
    if (!config.value("config_version").isString()) {
        qWarning() << "config_version must be a string";
        return false;
    }
    
    // 验证网络参数格式
    if (!config.value("network_params").isObject()) {
        qWarning() << "network_params must be an object";
        return false;
    }

    // 验证 processes 格式
    if (!config.value("processes").isObject()) {
        qWarning() << "processes must be an object";
        return false;
    }
    
    return true;
}

bool ProjectConfig::ensureConfigFileExists(const QString& filePath)
{
    // 确保配置目录存在
    QFileInfo fileInfo(filePath);
    QDir configDir = fileInfo.absoluteDir();

    // 使用递归方式创建目录
    if (!configDir.mkpath(configDir.absolutePath())) {
        qWarning() << "[ProjectConfig] 创建配置目录失败:"
                   << configDir.absolutePath();
        return false;
    }

    // 检查配置文件是否存在，不存在则创建一个空文件
    if (!QFile::exists(filePath)) {
        QFile emptyFile(filePath);
        if (emptyFile.open(QIODevice::WriteOnly)) {
            emptyFile.write("{}\n"); // 写入空JSON对象，防止解析失败
            emptyFile.close();
            qDebug() << "[ProjectConfig] 创建空配置文件:" << filePath;
            return true;
        } else {
            qWarning() << "[ProjectConfig] 创建空配置文件失败:" << filePath << emptyFile.errorString();
            return false;
        }
    }

    return true;
}

bool ProjectConfig::ensureConfigDirectory(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.absoluteDir();

    if (!dir.exists()) {
        qInfo() << "Attempting to create config directory:" << dir.absolutePath(); // 添加调试信息
        if (!dir.mkpath(dir.absolutePath())) { // 使用绝对路径确保创建正确
            qWarning() << "Failed to create config directory:" << dir.absolutePath(); // 打印更详细的错误
            return false;
        }
        qInfo() << "Created config directory:"<< dir.absolutePath() << "\" successfully.";
    }

    return true;
}

void ProjectConfig::emitConfigUpdated(const QString& key, const QJsonValue& oldValue, const QJsonValue& newValue)
{
    // 在解锁后发送信号，避免死锁
    QMetaObject::invokeMethod(this, [this, key, oldValue, newValue]() {
        emit configUpdated(key, oldValue, newValue);
    }, Qt::QueuedConnection);
}
