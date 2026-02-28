#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <memory>


/**
 * @brief PluginManager 插件管理类
 *
 * 负责插件的获取、安装、卸载和管理。
 * 从阿里云OSS获取插件列表，支持插件下载、解压和注册到进程管理器。
 * 采用单例模式确保插件管理全局唯一。
 *
 * 主要功能：
 * - 从OSS获取插件列表
 * - 插件下载和安装
 * - 插件卸载
 * - 已安装插件管理
 * - 插件状态监控
 */
class PluginManager final : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(PluginManager)

public:
  /**
   * @brief 插件状态枚举
   */
  enum PluginStatus {
    kNotInstalled = 0, // 未安装
    kDownloading,      // 下载中
    kInstalling,       // 安装中
    kInstalled,        // 已安装
    kUpdateAvailable,  // 有更新可用
    kError             // 错误状态
  };

  /**
   * @brief 插件信息结构体
   */
  struct PluginInfo {
    QString id;                   // 插件ID
    QString name;                 // 插件名称
    QString version;              // 版本号
    QString author;               // 作者
    QString description;          // 简短描述
    QString detailed_description; // 详细描述
    QString icon_type;            // 图标类型
    QString category;             // 分类
    QString download_url;         // 下载地址
    qint64 download_size;         // 下载大小
    QString executable;           // 可执行文件名
    QString required_version;     // 所需主程序版本
    QStringList dependencies;     // 依赖项
    QStringList screenshots;      // 截图
    PluginStatus status;          // 插件状态
    QString install_path;         // 安装路径
  };

private:
  explicit PluginManager(QObject *parent = nullptr);

public:
  static PluginManager &GetInstance();
  ~PluginManager();

  /**
   * @brief 初始化插件管理器
   * @return 初始化是否成功
   */
  bool Initialize();

  /**
   * @brief 从OSS获取插件列表
   */
  Q_INVOKABLE void fetchPluginList();

  /**
   * @brief 获取可安装插件列表
   * @return 可安装插件的变体列表
   */
  Q_INVOKABLE QVariantList getAvailablePlugins();

  /**
   * @brief 获取已安装插件列表
   * @return 已安装插件的变体列表
   */
  Q_INVOKABLE QVariantList getInstalledPlugins();

  /**
   * @brief 获取插件详情
   * @param plugin_id 插件ID
   * @return 插件详情的变体映射
   */
  Q_INVOKABLE QVariantMap getPluginDetail(const QString &plugin_id);

  /**
   * @brief 安装插件
   * @param plugin_id 插件ID
   */
  Q_INVOKABLE void installPlugin(const QString &plugin_id);

  /**
   * @brief 卸载插件
   * @param plugin_id 插件ID
   */
  Q_INVOKABLE void uninstallPlugin(const QString &plugin_id);

  /**
   * @brief 检查插件是否已安装
   * @param plugin_id 插件ID
   * @return 插件是否已安装
   */
  Q_INVOKABLE bool isPluginInstalled(const QString &plugin_id) const;

  /**
   * @brief 获取插件安装路径
   * @param plugin_id 插件ID
   * @return 插件安装路径
   */
  QString GetPluginInstallPath(const QString &plugin_id) const;

  /**
   * @brief 格式化文件大小为易读的形式
   * @param bytes 字节数
   * @return 格式化的文件大小字符串，如 "1.23 MB"
   */
  Q_INVOKABLE QString FormatFileSize(qint64 bytes) const;

  /**
   * @brief 获取所有插件信息
   * @return 插件信息列表
   */
  QList<PluginInfo> GetAllPlugins() const;

signals:
  /**
   * @brief 插件列表已更新信号
   */
  void pluginListUpdated();

  /**
   * @brief 日志消息信号，用于通知 QML 层输出日志
   * @param message 日志内容
   */
  void logMessage(const QString &message);

  /**
   * @brief 插件安装进度信号
   * @param plugin_id 插件ID
   * @param progress 进度百分比（0-100）
   */
  void installProgress(const QString &plugin_id, int progress);

  /**
   * @brief 插件安装完成信号
   * @param plugin_id 插件ID
   * @param success 是否成功
   * @param error_message 错误消息（如果失败）
   */
  void installCompleted(const QString &plugin_id, bool success,
                        const QString &error_message);

  /**
   * @brief 插件卸载完成信号
   * @param plugin_id 插件ID
   * @param success 是否成功
   */
  void uninstallCompleted(const QString &plugin_id, bool success);

private slots:
  /**
   * @brief 处理插件列表网络响应
   */
  void OnPluginListReply();

  /**
   * @brief 处理插件下载进度
   * @param bytes_received 已接收字节数
   * @param bytes_total 总字节数
   */
  void OnDownloadProgress(qint64 bytes_received, qint64 bytes_total);

  /**
   * @brief 处理插件下载完成
   */
  void OnDownloadFinished();

private:

  void ParsePluginListJson(const QJsonObject &json_obj);

  /**
   * @brief 从JSON对象解析插件信息
   * @param json_obj JSON对象
   * @return 插件信息结构体
   */
  PluginInfo ParsePluginInfo(const QJsonObject &json_obj);

  /**
   * @brief 解压插件文件
   * @param zip_file_path 压缩文件路径
   * @param extract_path 解压目标路径
   * @return 解压是否成功
   */
  bool ExtractPlugin(const QString &zip_file_path, const QString &extract_path);

  /**
   * @brief 注册插件到进程管理器
   * @param plugin_info 插件信息
   * @return 注册是否成功
   */
  bool RegisterPluginToProcessManager(const PluginInfo &plugin_info);

  /**
   * @brief 从配置加载已安装插件
   */
  void LoadInstalledPluginsFromConfig();

  /**
   * @brief 保存已安装插件到配置
   */
  void SaveInstalledPluginsToConfig();

  /**
   * @brief 将PluginInfo转换为QVariantMap
   * @param info 插件信息
   * @return 变体映射
   */
  QVariantMap PluginInfoToVariantMap(const PluginInfo &info) const;

private:
  static const QString kOssPluginListUrl;  // OSS插件列表URL
  // static const QString kPluginsInstallDir; // 插件安装目录
  QString plugins_dir; // 插件安装目录

  QNetworkAccessManager *network_manager_; // 网络访问管理器
  QNetworkReply *current_download_reply_;  // 当前下载回复
  QString current_download_plugin_id_;     // 当前下载的插件ID

  QList<PluginInfo> available_plugins_; // 可用插件列表
  QList<PluginInfo> installed_plugins_; // 已安装插件列表

  mutable QMutex plugins_mutex_; // 插件列表互斥锁
  bool is_initialized_;          // 是否已初始化
};

#endif // PLUGIN_MANAGER_H
