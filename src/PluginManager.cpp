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
#include <QThread>


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
  // LoadInstalledPluginsFromConfig();

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

void PluginManager::uninstallPlugin(const QString &plugin_name) {
  qDebug() << "[PluginManager] 开始卸载插件:" << plugin_name;

  // 从配置文件查询已安装的插件，确保数据最新（避免程序重启后内存不同步）
  ProjectConfig &config = ProjectConfig::getInstance();
  
  // 从 process_list 中查找插件
  QJsonValue process_list_value = config.getConfigValue("process_list");
  if (!process_list_value.isArray()) {
    qWarning() << "[PluginManager] 配置中没有进程列表";
    emit uninstallCompleted(plugin_name, false);
    return;
  }

  QJsonArray process_list = process_list_value.toArray();
  
  // 检查插件是否存在于 process_list 中
  bool found = false;
  for (const QJsonValue &value : process_list) {
    if (value.toString() == plugin_name) {
      found = true;
      break;
    }
  }

  if (!found) {
    qWarning() << "[PluginManager] 插件未安装:" << plugin_name;
    emit uninstallCompleted(plugin_name, false);
    return;
  }

  // 从 processes 对象中获取该插件的配置
  QJsonValue processes_value = config.getConfigValue("processes");
  if (!processes_value.isObject()) {
    qWarning() << "[PluginManager] 配置中没有进程详细信息";
    emit uninstallCompleted(plugin_name, false);
    return;
  }

  QJsonObject processes = processes_value.toObject();
  QJsonObject plugin_config = processes.value(plugin_name).toObject();
  
  if (plugin_config.isEmpty()) {
    qWarning() << "[PluginManager] 找不到插件配置:" << plugin_name;
    emit uninstallCompleted(plugin_name, false);
    return;
  }

  // 从 executable_dir 提取安装目录路径
  QString executable_dir = plugin_config.value("executable_dir").toString();
  if (executable_dir.isEmpty()) {
    qWarning() << "[PluginManager] 插件没有可执行文件路径:" << plugin_name;
    emit uninstallCompleted(plugin_name, false);
    return;
  }

  // 提取目录部分（去掉可执行文件名）
  QFileInfo file_info(executable_dir);
  QString install_path = file_info.absolutePath();

  QMutexLocker locker(&plugins_mutex_);

  // 删除插件目录（带重试机制，处理 Windows 文件锁）
  QDir plugin_dir(install_path);
  if (plugin_dir.exists()) {
    bool delete_success = false;
    const int max_retries = 3;
    const int retry_delay_ms = 100;
    
    for (int attempt = 0; attempt < max_retries; ++attempt) {
      if (plugin_dir.removeRecursively()) {
        delete_success = true;
        qDebug() << "[PluginManager] 插件目录已删除:" << install_path;
        break;
      } else {
        qWarning() << "[PluginManager] 删除插件目录失败（第" << (attempt + 1) << "次尝试）:" << install_path;
        // 等待后重试
        if (attempt < max_retries - 1) {
          QThread::msleep(retry_delay_ms);
        }
      }
    }
    
    if (!delete_success) {
      qWarning() << "[PluginManager] 删除插件目录最终失败，目录可能被占用:" << install_path;
    }
  }

  // 从内存中的已安装列表移除（如果存在）
  for (int i = 0; i < installed_plugins_.size(); ++i) {
    if (installed_plugins_[i].name == plugin_name) {
      installed_plugins_.removeAt(i);
      break;
    }
  }

  // 更新可用列表中的状态
  for (PluginInfo &info : available_plugins_) {
    if (info.name == plugin_name) {
      info.status = kNotInstalled;
      info.install_path.clear();
      break;
    }
  }

  // 从 process_list 中移除
  QJsonArray new_process_list;
  for (const QJsonValue &value : process_list) {
    if (value.toString() != plugin_name) {
      new_process_list.append(value);
    }
  }
  config.setConfigValue("process_list", new_process_list);

  // 从 processes 对象中移除
  processes.remove(plugin_name);
  config.setConfigValue("processes", processes);

  config.saveConfig();

  qDebug() << "[PluginManager] 插件卸载完成:" << plugin_name;
  
  // 在发射信号前释放互斥量，避免死锁
  locker.unlock();
  
  emit uninstallCompleted(plugin_name, true);
  emit pluginListUpdated();
}

bool PluginManager::isPluginInstalled(const QString &plugin_id) const {
  // 从配置文件查询已安装的插件，确保持久化存储的准确性
  ProjectConfig &config = ProjectConfig::getInstance();
  QJsonValue process_list_value = config.getConfigValue("process_list");

  if (!process_list_value.isArray()) {
    return false;
  }

  QJsonArray process_list = process_list_value.toArray();
  for (const QJsonValue &value : process_list) {
    if (value.toString() == plugin_id) {
      return true;
    }
  }

  return false;
}


QList<PluginManager::PluginInfo> PluginManager::GetAllPlugins() const {
  QMutexLocker locker(&plugins_mutex_);
  return available_plugins_;
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
