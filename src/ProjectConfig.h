#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

#include <QObject>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QFileSystemWatcher>
#include <QMutex>
#include <QMutexLocker>
#include <memory>

/**
 * @brief ProjectConfig 静态配置管理类
 * 
 * 负责全局配置（如IP表、工作目录、进程列表等）的加载与保存，采用JSON格式，
 * 支持配置热更新和同步。采用单例模式确保配置管理全局唯一。
 * 
 * 主要功能：
 * - JSON格式配置文件的加载和保存
 * - 配置热更新机制
 * - 配置版本管理
 * - 线程安全的配置访问
 */
class ProjectConfig : public QObject
{
    Q_OBJECT

public:
    static ProjectConfig& getInstance();
    ProjectConfig(const ProjectConfig&) = delete;
    ProjectConfig& operator=(const ProjectConfig&) = delete;
    bool initialize(const QString& configFilePath = "config/project.json");
    bool loadConfig(const QString& filePath = "");
    bool saveConfig(const QString& filePath = "");

    /**
     * @brief 热更新配置
     * @param newConfig 新的配置JSON对象
     * @return 更新是否成功
     */
    bool hotUpdateConfig(const QJsonObject& newConfig);

    QStringList getIpTable() const;
    void setIpTable(const QStringList& ipList);
    QStringList getProcessList() const;
    void setProcessList(const QStringList& processList);       
    QString getWorkDirectory() const;
    void setWorkDirectory(const QString& workDir);
    QJsonObject getNetworkParams() const;
    void setNetworkParams(const QJsonObject& params);
    QString getConfigVersion() const;
    void setConfigVersion(const QString& version);

    // === 通用配置访问 ===
    /**
     * @brief 获取配置值
     * @param key 配置键
     * @param defaultValue 默认值
     * @return 配置值
     */
    QJsonValue getConfigValue(const QString& key, const QJsonValue& defaultValue = QJsonValue()) const;

    /**
     * @brief 设置配置值
     * @param key 配置键
     * @param value 配置值
     */
    void setConfigValue(const QString& key, const QJsonValue& value);

    /**
     * @brief 获取完整配置对象
     * @return 配置JSON对象
     */
    QJsonObject getFullConfig() const;

    /**
     * @brief 检查配置是否已加载
     * @return 配置是否已加载
     */
    bool isConfigLoaded() const;

signals:
    /**
     * @brief 配置已更新信号
     * @param configKey 更新的配置键
     * @param oldValue 旧值
     * @param newValue 新值
     */
    void configUpdated(const QString& configKey, const QJsonValue& oldValue, const QJsonValue& newValue);

    /**
     * @brief 配置热更新完成信号
     * @param success 热更新是否成功
     */
    void hotUpdateCompleted(bool success);

    /**
     * @brief 配置文件改变信号
     * @param filePath 改变的文件路径
     */
    void configFileChanged(const QString& filePath);

private slots:
    /**
     * @brief 处理配置文件变化
     * @param filePath 变化的文件路径
     */
    void handleConfigFileChanged(const QString& filePath);

private:
    /**
     * @brief 私有构造函数（单例模式）
     */
    explicit ProjectConfig(QObject *parent = nullptr);

    /**
     * @brief 析构函数 
     */
    ~ProjectConfig() override;

    // 友元类，允许unique_ptr的默认删除器访问私有析构函数
    friend class std::default_delete<ProjectConfig>;

    /**
     * @brief 创建默认配置
     * @return 默认配置JSON对象
     */
    QJsonObject createDefaultConfig();

    /**
     * @brief 验证配置格式
     * @param config 配置JSON对象
     * @return 配置是否有效
     */
    bool validateConfig(const QJsonObject& config);

    /**
     * @brief 确保配置目录和文件存在
     * @param filePath 配置文件路径
     * @return 目录和文件创建是否成功
     */
    bool ensureConfigFileExists(const QString& filePath);

    /**
     * @brief 确保配置目录存在
     * @param filePath 配置文件路径
     * @return 目录创建是否成功
     */
    bool ensureConfigDirectory(const QString& filePath);

    /**
     * @brief 发送配置更新信号
     * @param key 配置键
     * @param oldValue 旧值
     * @param newValue 新值
     */
    void emitConfigUpdated(const QString& key, const QJsonValue& oldValue, const QJsonValue& newValue);

private:
    static std::unique_ptr<ProjectConfig> instance_;    ///< 单例实例

    mutable QMutex config_mutex_;                       ///< 配置访问互斥锁
    QJsonObject config_;                                ///< 配置数据
    QString config_file_path_;                          ///< 配置文件路径
    QFileSystemWatcher* file_watcher_;                  ///< 文件系统监视器
    bool config_loaded_;                                ///< 配置是否已加载
    bool hot_update_enabled_;                           ///< 热更新是否启用
};

#endif // PROJECT_CONFIG_H
