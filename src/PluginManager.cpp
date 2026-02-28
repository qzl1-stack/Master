#include "PluginManager.h"
#include "MainController.h"
#include "ProcessManager.h"
#include "ProjectConfig.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QJsonValue>
#include <QNetworkRequest>
#include <QProcess>
#include <QStandardPaths>


// 引入Qt私有API进行ZIP解压（如果可用）
// 注意：这是Qt的私有API，可能在不同版本中变化
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
#include <private/qzipreader_p.h>
#endif

const QString PluginManager::kOssPluginListUrl =
    "https://jts-tools-extensions.oss-cn-chengdu.aliyuncs.com/plugins.json";

PluginManager::PluginManager(QObject *parent)
    : QObject(parent), network_manager_(nullptr),
      current_download_reply_(nullptr), is_initialized_(false) {
  qDebug() << "[PluginManager] 构造函数调用";
  network_manager_ = new QNetworkAccessManager(this);
}

PluginManager::~PluginManager() {
  if (current_download_reply_) {
    current_download_reply_->abort();
    current_download_reply_->deleteLater();
  }
}

PluginManager &PluginManager::GetInstance() {
  static PluginManager instance;
  return instance;
}

bool PluginManager::Initialize() {
  if (is_initialized_) {
    qDebug() << "[PluginManager] 已经初始化";
    return true;
  }

  qDebug() << "[PluginManager] 开始初始化插件管理器";

  // 创建插件安装目录
  QString home_path = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
  QString jt_studio_dir = home_path + "/.jt_studio";
  plugins_dir = jt_studio_dir + "/plugins";

  QDir dir;
  if (!dir.exists(jt_studio_dir)) {
    if (!dir.mkpath(jt_studio_dir)) {
      qWarning() << "[PluginManager] 创建 .jt_studio 目录失败:" << jt_studio_dir;
      return false;
    }
  }

  if (!dir.exists(plugins_dir)) {
    if (!dir.mkpath(plugins_dir)) {
      qWarning() << "[PluginManager] 创建插件目录失败:" << plugins_dir;
      return false;
    }
  }

  // 从配置加载已安装插件
  LoadInstalledPluginsFromConfig();

  is_initialized_ = true;
  qDebug() << "[PluginManager] 插件管理器初始化完成";

  return true;
}

void PluginManager::fetchPluginList() {
  qDebug() << "[PluginManager] 开始获取插件列表，URL:" << kOssPluginListUrl;
  emit logMessage("正在加载工具列表...");

  QNetworkRequest request;
  request.setUrl(QUrl(kOssPluginListUrl));
  request.setRawHeader("User-Agent", "JT-Studio-PluginManager/1.0");
  request.setTransferTimeout(15000); // 15秒超时

  QNetworkReply *reply = network_manager_->get(request);

  connect(reply, &QNetworkReply::finished, this,
          &PluginManager::OnPluginListReply);
  connect(
      reply,
      QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
      this, [this](QNetworkReply::NetworkError error) {
        qWarning() << "[PluginManager] 网络请求错误:" << error;
        emit logMessage("加载工具列表网络错误");
      });
}

void PluginManager::OnPluginListReply() {
  QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
  if (!reply) {
    qWarning() << "[PluginManager] 无效的网络回复对象";
    return;
  }

  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError) {
    qWarning() << "[PluginManager] 获取插件列表失败:" << reply->errorString();
    emit logMessage("加载工具列表失败，错误: " + reply->errorString());
    return;
  }

  QByteArray response_data = reply->readAll();
  qDebug() << "[PluginManager] 插件列表响应大小:" << response_data.size()
           << "字节";

  QJsonDocument json_doc = QJsonDocument::fromJson(response_data);
  if (json_doc.isNull() || !json_doc.isObject()) {
    qWarning() << "[PluginManager] 插件列表JSON解析失败";
    emit logMessage("工具列表解析失败");
    return;
  }

  ParsePluginListJson(json_doc.object());

  {
    QMutexLocker locker(&plugins_mutex_);
    emit logMessage("工具列表加载成功，共 " +
                    QString::number(available_plugins_.size()) + " 个插件");
  }

  emit pluginListUpdated();
}

void PluginManager::ParsePluginListJson(const QJsonObject &json_obj) {
  QMutexLocker locker(&plugins_mutex_);

  available_plugins_.clear();

  QString version = json_obj.value("version").toString();
  QString last_update = json_obj.value("last_update").toString();

  qDebug() << "[PluginManager] 插件列表版本:" << version
           << "，最后更新:" << last_update;

  QJsonArray plugins_array = json_obj.value("plugins").toArray();
  qDebug() << "[PluginManager] 找到" << plugins_array.size() << "个插件";

  for (const QJsonValue &plugin_value : plugins_array) {
    if (!plugin_value.isObject()) {
      continue;
    }

    PluginInfo info = ParsePluginInfo(plugin_value.toObject());

    // 检查是否已安装
    bool is_installed = false;
    for (const PluginInfo &installed : installed_plugins_) {
      if (installed.id == info.id) {
        is_installed = true;
        info.status = kInstalled;
        info.install_path = installed.install_path;
        break;
      }
    }

    if (!is_installed) {
      info.status = kNotInstalled;
    }

    available_plugins_.append(info);
  }

  qDebug() << "[PluginManager] 插件列表解析完成，共"
           << available_plugins_.size() << "个插件";
}

PluginManager::PluginInfo
PluginManager::ParsePluginInfo(const QJsonObject &json_obj) {
  PluginInfo info;
  info.id = json_obj.value("id").toString();
  info.name = json_obj.value("name").toString();
  info.version = json_obj.value("version").toString();
  info.author = json_obj.value("author").toString();
  info.description = json_obj.value("description").toString();
  info.detailed_description = json_obj.value("detailed_description").toString();
  info.icon_type = json_obj.value("icon_type").toString("default");
  info.category = json_obj.value("category").toString();
  info.download_url = json_obj.value("download_url").toString();
  info.download_size = json_obj.value("download_size").toInteger();
  info.executable = json_obj.value("executable").toString();
  info.required_version = json_obj.value("required_version").toString();

  QJsonArray deps_array = json_obj.value("dependencies").toArray();
  for (const QJsonValue &dep : deps_array) {
    info.dependencies.append(dep.toString());
  }

  QJsonArray screenshots_array = json_obj.value("screenshots").toArray();
  for (const QJsonValue &screenshot : screenshots_array) {
    info.screenshots.append(screenshot.toString());
  }

  info.status = kNotInstalled;

  return info;
}

QVariantList PluginManager::getAvailablePlugins() {
  QMutexLocker locker(&plugins_mutex_);

  QVariantList result;
  for (const PluginInfo &info : available_plugins_) {
    result.append(PluginInfoToVariantMap(info));
  }

  return result;
}

QVariantList PluginManager::getInstalledPlugins() {
  QMutexLocker locker(&plugins_mutex_);

  QVariantList result;
  for (const PluginInfo &info : installed_plugins_) {
    result.append(PluginInfoToVariantMap(info));
  }

  return result;
}

QVariantMap PluginManager::getPluginDetail(const QString &plugin_id) {
  QMutexLocker locker(&plugins_mutex_);

  // 先在可用插件中查找
  for (const PluginInfo &info : available_plugins_) {
    if (info.id == plugin_id) {
      return PluginInfoToVariantMap(info);
    }
  }

  // 再在已安装插件中查找
  for (const PluginInfo &info : installed_plugins_) {
    if (info.id == plugin_id) {
      return PluginInfoToVariantMap(info);
    }
  }

  return QVariantMap();
}

void PluginManager::installPlugin(const QString &plugin_id) {
  qDebug() << "[PluginManager] 开始安装插件:" << plugin_id;

  // 查找插件信息
  PluginInfo *plugin_info = nullptr;
  {
    QMutexLocker locker(&plugins_mutex_);
    for (PluginInfo &info : available_plugins_) {
      if (info.id == plugin_id) {
        plugin_info = &info;
        break;
      }
    }
  }

  if (!plugin_info) {
    qWarning() << "[PluginManager] 未找到插件:" << plugin_id;
    emit installCompleted(plugin_id, false, "未找到插件信息");
    return;
  }

  if (plugin_info->status == kInstalled) {
    qWarning() << "[PluginManager] 插件已安装:" << plugin_id;
    emit installCompleted(plugin_id, false, "插件已安装");
    return;
  }

  // 开始下载
  plugin_info->status = kDownloading;
  current_download_plugin_id_ = plugin_id;

  QNetworkRequest request;
  request.setUrl(QUrl(plugin_info->download_url));
  request.setRawHeader("User-Agent", "JT-Studio-PluginManager/1.0");

  current_download_reply_ = network_manager_->get(request);

  connect(current_download_reply_, &QNetworkReply::downloadProgress, this,
          &PluginManager::OnDownloadProgress);
  connect(current_download_reply_, &QNetworkReply::finished, this,
          &PluginManager::OnDownloadFinished);

  qDebug() << "[PluginManager] 开始下载插件:" << plugin_id
           << "，URL:" << plugin_info->download_url;
}

void PluginManager::OnDownloadProgress(qint64 bytes_received,
                                       qint64 bytes_total) {
  if (bytes_total > 0) {
    int progress = static_cast<int>((bytes_received * 100) / bytes_total);
    emit installProgress(current_download_plugin_id_, progress);

    qDebug() << "[PluginManager] 下载进度:" << current_download_plugin_id_
             << progress << "%"
             << "(" << bytes_received << "/" << bytes_total << ")";
  }
}

void PluginManager::OnDownloadFinished() {
  QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
  if (!reply) {
    qWarning() << "[PluginManager] 无效的下载回复对象";
    return;
  }

  QString plugin_id = current_download_plugin_id_;
  reply->deleteLater();
  current_download_reply_ = nullptr;

  if (reply->error() != QNetworkReply::NoError) {
    qWarning() << "[PluginManager] 下载插件失败:" << plugin_id
               << "，错误:" << reply->errorString();
    emit installCompleted(plugin_id, false,
                          "下载失败: " + reply->errorString());
    return;
  }

  qDebug() << "[PluginManager] 插件下载完成:" << plugin_id;


  // 保存下载的文件
  QString temp_dir =
      QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  QString zip_file_path = temp_dir + "/" + plugin_id + ".zip";

  QFile file(zip_file_path);
  if (!file.open(QIODevice::WriteOnly)) {
    qWarning() << "[PluginManager] 无法保存下载文件:" << zip_file_path;
    emit installCompleted(plugin_id, false, "无法保存下载文件");
    return;
  }

  file.write(reply->readAll());
  file.close();

  qDebug() << "[PluginManager] 插件文件已保存:" << zip_file_path;

  // 解压插件
  QString extract_path = this->plugins_dir;

  if (!ExtractPlugin(zip_file_path, extract_path)) {
    qWarning() << "[PluginManager] 解压插件失败:" << plugin_id;
    emit installCompleted(plugin_id, false, "解压失败");

    // 删除临时文件
    QFile::remove(zip_file_path);
    return;
  }

  qDebug() << "[PluginManager] 插件解压完成:" << extract_path;

  // 删除临时文件
  QFile::remove(zip_file_path);

  // 更新插件信息
  {
    QMutexLocker locker(&plugins_mutex_);
    for (PluginInfo &info : available_plugins_) {
      if (info.id == plugin_id) {
        info.status = kInstalled;
        info.install_path = extract_path;

        // 添加到已安装列表
        installed_plugins_.append(info);

        // 注册到进程管理器
        if (!RegisterPluginToProcessManager(info)) {
          qWarning() << "[PluginManager] 注册插件到进程管理器失败:"
                     << plugin_id;
        }

        break;
      }
    }
  }

  // 保存已安装插件配置
  SaveInstalledPluginsToConfig();

  qDebug() << "[PluginManager] 插件安装完成:" << plugin_id;
  emit installCompleted(plugin_id, true, "");
  emit pluginListUpdated();
}

bool PluginManager::ExtractPlugin(const QString &zip_file_path,
                                  const QString &extract_path) {
  qDebug() << "[PluginManager] 开始解压:" << zip_file_path << "到"
           << extract_path;

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  QZipReader zip_reader(zip_file_path);

  if (!zip_reader.isReadable()) {
    qWarning() << "[PluginManager] 无法读取ZIP文件:" << zip_file_path;
    return false;
  }

  // 创建目标目录
  QDir dir;
  if (!dir.exists(extract_path)) {
    if (!dir.mkpath(extract_path)) {
      qWarning() << "[PluginManager] 创建解压目录失败:" << extract_path;
      return false;
    }
  }

  // 解压所有文件
  if (!zip_reader.extractAll(extract_path)) {
    qWarning() << "[PluginManager] 解压文件失败";
    return false;
  }

  zip_reader.close();
  qDebug() << "[PluginManager] 解压成功";
  return true;
#else
  // 如果Qt版本不支持私有API，使用外部工具解压
  qWarning() << "[PluginManager] Qt版本不支持内置ZIP解压，尝试使用系统工具";

  QProcess unzip_process;
  QStringList args;

#ifdef Q_OS_WIN
  // Windows使用PowerShell解压
  args << "-Command"
       << QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force")
              .arg(zip_file_path)
              .arg(extract_path);
  unzip_process.start("powershell.exe", args);
#else
  // Linux/Mac使用unzip命令
  args << zip_file_path << "-d" << extract_path;
  unzip_process.start("unzip", args);
#endif

  if (!unzip_process.waitForFinished(30000)) {
    qWarning() << "[PluginManager] 解压超时";
    return false;
  }

  if (unzip_process.exitCode() != 0) {
    qWarning() << "[PluginManager] 解压失败:"
               << unzip_process.readAllStandardError();
    return false;
  }

  qDebug() << "[PluginManager] 使用系统工具解压成功";
  return true;
#endif
}

bool PluginManager::RegisterPluginToProcessManager(
    const PluginInfo &plugin_info) {
  qDebug() << "[PluginManager] 注册插件到进程管理器:" << plugin_info.id;

  // 获取ProcessManager实例
  ProcessManager &process_manager = ProcessManager::GetInstance();

  // 构建可执行文件完整路径
  QString executable_path =
      plugin_info.install_path + "/" + plugin_info.executable;

  // 检查可执行文件是否存在
  if (!QFile::exists(executable_path)) {
    qWarning() << "[PluginManager] 插件可执行文件不存在:" << executable_path;
    return false;
  }

  // 添加到进程管理器
  bool success =
      process_manager.AddProcess(plugin_info.name, // 进程ID（使用插件名称）
                                 executable_path,  // 可执行文件路径
                                 QStringList(),    // 启动参数
                                 plugin_info.install_path);

  if (!success) {
    qWarning() << "[PluginManager] 添加插件到进程管理器失败:" << plugin_info.id;
    return false;
  }

  qDebug() << "[PluginManager] 插件已注册到进程管理器:" << plugin_info.name;
  return true;
}

void PluginManager::uninstallPlugin(const QString &plugin_id) {
  qDebug() << "[PluginManager] 开始卸载插件:" << plugin_id;

  QMutexLocker locker(&plugins_mutex_);

  // 查找已安装的插件
  int index = -1;
  for (int i = 0; i < installed_plugins_.size(); ++i) {
    if (installed_plugins_[i].id == plugin_id) {
      index = i;
      break;
    }
  }

  if (index == -1) {
    qWarning() << "[PluginManager] 插件未安装:" << plugin_id;
    emit uninstallCompleted(plugin_id, false);
    return;
  }

  PluginInfo plugin_info = installed_plugins_[index];

  // 从进程管理器中移除
  ProcessManager &process_manager = ProcessManager::GetInstance();
  process_manager.StopProcess(plugin_info.name, true);

  // 删除插件文件
  QDir plugin_dir(plugin_info.install_path);
  if (plugin_dir.exists()) {
    if (!plugin_dir.removeRecursively()) {
      qWarning() << "[PluginManager] 删除插件目录失败:"
                 << plugin_info.install_path;
    }
  }

  // 从已安装列表中移除
  installed_plugins_.removeAt(index);

  // 更新可用列表中的状态
  for (PluginInfo &info : available_plugins_) {
    if (info.id == plugin_id) {
      info.status = kNotInstalled;
      info.install_path.clear();
      break;
    }
  }

  // 保存配置
  SaveInstalledPluginsToConfig();

  qDebug() << "[PluginManager] 插件卸载完成:" << plugin_id;
  emit uninstallCompleted(plugin_id, true);
  emit pluginListUpdated();
}

bool PluginManager::isPluginInstalled(const QString &plugin_id) const {
  // 从配置文件查询已安装的插件，确保持久化存储的准确性
  ProjectConfig &config = ProjectConfig::getInstance();
  QJsonValue installed_value = config.getConfigValue("installed_plugins");

  if (!installed_value.isArray()) {
    return false;
  }

  QJsonArray installed_array = installed_value.toArray();
  for (const QJsonValue &value : installed_array) {
    if (value.isObject()) {
      QJsonObject plugin_obj = value.toObject();
      if (plugin_obj.value("id").toString() == plugin_id) {
        return true;
      }
    }
  }

  return false;
}

QString PluginManager::GetPluginInstallPath(const QString &plugin_id) const {
  QMutexLocker locker(&plugins_mutex_);

  for (const PluginInfo &info : installed_plugins_) {
    if (info.id == plugin_id) {
      return info.install_path;
    }
  }

  return QString();
}

QList<PluginManager::PluginInfo> PluginManager::GetAllPlugins() const {
  QMutexLocker locker(&plugins_mutex_);
  return available_plugins_;
}

void PluginManager::LoadInstalledPluginsFromConfig() {
  qDebug() << "[PluginManager] 从配置加载已安装插件";

  ProjectConfig &config = ProjectConfig::getInstance();
  QJsonValue installed_value = config.getConfigValue("installed_plugins");

  if (!installed_value.isArray()) {
    qDebug() << "[PluginManager] 配置中没有已安装插件信息";
    return;
  }

  QJsonArray installed_array = installed_value.toArray();

  QMutexLocker locker(&plugins_mutex_);
  installed_plugins_.clear();

  for (const QJsonValue &value : installed_array) {
    if (!value.isObject()) {
      continue;
    }

    PluginInfo info = ParsePluginInfo(value.toObject());
    info.status = kInstalled;
    info.install_path = value.toObject().value("install_path").toString();

    installed_plugins_.append(info);

    // 注册到进程管理器
    RegisterPluginToProcessManager(info);
  }

  qDebug() << "[PluginManager] 加载了" << installed_plugins_.size()
           << "个已安装插件";
}

void PluginManager::SaveInstalledPluginsToConfig() {
  qDebug() << "[PluginManager] 保存已安装插件到配置";

  QJsonArray installed_array;
  QJsonArray process_list;
  QJsonObject processes;

  ProjectConfig &config = ProjectConfig::getInstance();

  // 获取现有的 process_list 和 processes 配置
  QJsonValue existing_process_list = config.getConfigValue("process_list");
  QJsonValue existing_processes = config.getConfigValue("processes");

  if (existing_process_list.isArray()) {
    process_list = existing_process_list.toArray();
  }
  if (existing_processes.isObject()) {
    processes = existing_processes.toObject();
  }

  QMutexLocker locker(&plugins_mutex_);
  for (const PluginInfo &info : installed_plugins_) {
    QJsonObject plugin_obj;
    plugin_obj["name"] = info.name;
    plugin_obj["version"] = info.version;
    QString executable_dir = plugins_dir + "/" + info.id + "/" + info.executable;
    plugin_obj["executable_dir"] = executable_dir;

    installed_array.append(plugin_obj);

    // 更新 process_list 和 processes
    bool found = false;
    for (const QJsonValue &value : process_list) {
      if (value.toString() == info.name) {
        found = true;
        break;
      }
    }
    if (!found) {
      process_list.append(info.name);
    }

    QJsonObject process_obj;
    process_obj["executable_dir"] = executable_dir;
    process_obj["version"] = info.version;
    processes[info.name] = process_obj;
  }

  // 保存所有配置
  config.setConfigValue("installed_plugins", installed_array);
  config.setConfigValue("process_list", process_list);
  config.setConfigValue("processes", processes);
  config.saveConfig();

  qDebug() << "[PluginManager] 已安装插件配置已保存";
}

QVariantMap
PluginManager::PluginInfoToVariantMap(const PluginInfo &info) const {
  QVariantMap map;
  map["id"] = info.id;
  map["name"] = info.name;
  map["version"] = info.version;
  map["author"] = info.author;
  map["description"] = info.description;
  map["detailed_description"] = info.detailed_description;
  map["icon_type"] = info.icon_type;
  map["category"] = info.category;
  map["download_url"] = info.download_url;
  map["download_size"] = static_cast<qint64>(info.download_size);
  map["executable"] = info.executable;
  map["required_version"] = info.required_version;
  map["dependencies"] = info.dependencies;
  map["screenshots"] = info.screenshots;
  map["status"] = static_cast<int>(info.status);
  map["install_path"] = info.install_path;

  // 添加状态文本
  QString status_text;
  switch (info.status) {
  case kNotInstalled:
    status_text = "未安装";
    break;
  case kDownloading:
    status_text = "下载中";
    break;
  case kInstalling:
    status_text = "安装中";
    break;
  case kInstalled:
    status_text = "已安装";
    break;
  case kUpdateAvailable:
    status_text = "有更新";
    break;
  case kError:
    status_text = "错误";
    break;
  }
  map["status_text"] = status_text;

  return map;
}

QString PluginManager::FormatFileSize(qint64 bytes) const {
  if (bytes <= 0) {
    return QStringLiteral("0 B");
  }

  static const QStringList kSizeUnits = {"B", "KB", "MB", "GB", "TB"};
  const double k = 1024.0;

  int unit_index = 0;
  double size = static_cast<double>(bytes);

  while (size >= k && unit_index < kSizeUnits.size() - 1) {
    size /= k;
    ++unit_index;
  }

  return QString::number(size, 'f', 2) + " " + kSizeUnits[unit_index];
}
