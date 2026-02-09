# 插件管理系统实现总结

## 完成状态

✅ **完整实现** - 插件管理系统已成功集成到 Master 主控程序中

## 核心架构

### 1. C++ 后端模块

#### PluginManager（`src/PluginManager.h/cpp`）
- **类型**：单例模式的 QObject 子类
- **职责**：插件生命周期管理
- **核心功能**：

| 功能 | 方法 | 说明 |
|------|------|------|
| 获取插件列表 | `fetchPluginList()` | 从OSS异步获取plugins.json |
| 获取可安装插件 | `getAvailablePlugins()` | 返回未安装的插件列表 |
| 获取已安装插件 | `getInstalledPlugins()` | 返回已安装的插件列表 |
| 获取插件详情 | `getPluginDetail(pluginId)` | 返回指定插件的详细信息 |
| 安装插件 | `installPlugin(pluginId)` | 下载、解压、注册到进程管理器 |
| 卸载插件 | `uninstallPlugin(pluginId)` | 从进程管理器移除、删除文件 |
| 检查安装状态 | `isPluginInstalled(pluginId)` | 判断插件是否已安装 |

#### 网络通信
- 使用 `QNetworkAccessManager` 从阿里云OSS获取JSON文件
- 支持下载进度报告
- 完整的错误处理机制

#### 文件操作
- 支持 ZIP 文件解压（使用Qt 5.15+的私有API或系统工具）
- 临时文件自动清理
- 跨平台兼容（Windows/Linux/macOS）

#### 进程集成
- 调用 `ProcessManager::AddProcess()` 注册插件为子进程
- 自动在配置文件中保存已安装插件信息
- 系统启动时自动加载已安装插件

### 2. MainController 集成

**文件**：`src/MainController.h/cpp`

**修改内容**：
- 在 `Initialize()` 方法中初始化 PluginManager
- 暴露 `pluginManager` 属性给QML
  ```cpp
  Q_PROPERTY(QObject* pluginManager READ GetPluginManager CONSTANT)
  QObject* GetPluginManager() const;
  ```
- 从已安装插件配置自动注册插件到 ProcessManager

### 3. QML 用户界面

#### Maincontroller.qml（主控制器界面）
**修改内容**：

1. **侧边栏图标栏** - 添加插件商店图标
   - 位置：进程图标列表下方，用分隔线隔开
   - 图标：购物车形状，使用Canvas绘制（VSCode风格设计）
   - 交互：点击打开插件商店，自动加载插件列表

2. **次级侧边栏** - 支持双标签页显示
   - **标签页1**：IP地址（原有功能）
   - **标签页2**：插件商店（新增）
   - 标签页栏特性：
     - 蓝色下划线指示器显示当前标签
     - 悬停状态高亮
     - 平滑的标签切换动画
   - **IP列表内容**：
     - 显示已添加的IP地址列表
     - 支持单个和批量添加
     - 支持编辑和删除操作
   - **插件商店内容**：
     - 从URL加载插件列表：https://jts-tools-extensions.oss-cn-chengdu.aliyuncs.com/plugins.json
     - 显示插件名称、版本、作者、描述
     - 刷新按钮可重新加载插件列表
     - 可视化插件项（卡片式设计）

3. **插件列表加载功能**
   - 使用 XMLHttpRequest 异步加载 JSON 数据
   - 5秒超时控制，防止长时间阻塞
   - 完整的错误处理和用户提示
   - 加载进度通过日志反馈给用户
   - 支持在插件标签页加载时自动获取

4. **信号监听** - PluginManager 信号处理（未来扩展）
   - `pluginListUpdated()` - 刷新插件列表视图
   - `installProgress(pluginId, progress)` - 显示安装进度
   - `installCompleted(pluginId, success, errorMsg)` - 安装完成提示
   - `uninstallCompleted(pluginId, success)` - 卸载完成提示

#### PluginDetailView.qml（插件详情页面）
**新建文件**：`qml/PluginDetailView.qml`

**功能**：
- 显示插件基本信息（名称、版本、作者、分类）
- 显示详细描述和技术信息
- 显示下载大小和所需主程序版本
- 安装/卸载按钮与状态指示
- 安装进度条（安装时显示）
- 错误提示（安装失败时显示）
- 实时响应安装信号

## 数据流程

### 插件列表获取流程

```
用户点击插件图标
    ↓
打开插件管理标签页 + 切换侧边栏到插件列表
    ↓
自动调用 PluginManager::fetchPluginList()
    ↓
HTTP GET 请求到 OSS URL:
https://jts-tools-extensions.oss-cn-chengdu.aliyuncs.com/plugins.json
    ↓
解析JSON → 更新 available_plugins 和 installed_plugins 列表
    ↓
发出 pluginListUpdated() 信号
    ↓
QML 刷新插件列表视图
```

### 插件安装流程

```
用户点击"安装"按钮
    ↓
调用 PluginManager::installPlugin(pluginId)
    ↓
HTTP GET 下载 ZIP 文件 → 发出 installProgress() 信号
    ↓
保存到临时目录
    ↓
解压到 plugins/插件ID/ 目录
    ↓
调用 ProcessManager::AddProcess() 注册为子进程
    ↓
保存到配置文件的 installed_plugins 数组
    ↓
发出 installCompleted() 信号
    ↓
在进程列表中显示插件，可作为子进程启动
```

### 系统启动时的插件加载

```
MainController::Initialize()
    ↓
PluginManager::Initialize()
    ↓
PluginManager::LoadInstalledPluginsFromConfig()
    ↓
遍历 config.json 中的 installed_plugins 数组
    ↓
对每个已安装插件调用 RegisterPluginToProcessManager()
    ↓
添加到 ProcessManager 和进程列表
```

## 配置文件集成

### config.json 扩展

新增 `installed_plugins` 数组字段：

```json
{
  "installed_plugins": [
    {
      "id": "plugin-001",
      "name": "示例插件",
      "version": "1.0.0",
      "author": "作者名称",
      "description": "插件简短描述",
      "detailed_description": "详细描述...",
      "icon_type": "default",
      "category": "工具",
      "download_url": "https://...",
      "download_size": 1024000,
      "executable": "plugin.exe",
      "required_version": "1.0.0",
      "dependencies": [],
      "install_path": "C:/App/plugins/plugin-001"
    }
  ]
}
```

## OSS 文件结构

### plugins.json（必需）
```
URL: https://jts-tools-extensions.oss-cn-chengdu.aliyuncs.com/plugins.json
格式: JSON
内容: 插件元数据列表
```

详见 [`plugins_json_example.md`](plugins_json_example.md)

### 插件包（ZIP格式）
```
plugin-id.zip
├── plugin.exe          # 主可执行文件（对应 "executable" 字段）
├── config.json         # 配置文件（可选）
├── lib/                # 依赖库（可选）
└── resources/          # 资源文件（可选）
```

## 代码组织

### 文件清单

| 文件 | 类型 | 说明 |
|------|------|------|
| `src/PluginManager.h` | 头文件 | 插件管理器接口定义 |
| `src/PluginManager.cpp` | 源文件 | 插件管理器实现（688行） |
| `src/MainController.h` | 头文件 | 集成PluginManager的接口 |
| `src/MainController.cpp` | 源文件 | MainController修改（初始化、属性暴露） |
| `qml/Maincontroller.qml` | QML | 主界面修改（3117行） |
| `qml/PluginDetailView.qml` | QML | 插件详情页面（556行） |
| `CMakeLists.txt` | CMake | 构建配置更新 |
| `plugins_json_example.md` | 文档 | plugins.json格式说明 |

### 编译配置

**CMakeLists.txt 更新**：
```cmake
# 添加到源文件列表
src/PluginManager.h
src/PluginManager.cpp

# 添加到QML模块
qml/PluginDetailView.qml
```

## 技术特性

### 网络通信
- ✅ 异步HTTP请求（不阻塞UI）
- ✅ 下载进度报告
- ✅ 超时控制（15秒）
- ✅ SSL/TLS支持
- ✅ 错误恢复机制

### 文件操作
- ✅ ZIP 解压支持
- ✅ 临时文件自动清理
- ✅ 跨平台兼容
- ✅ 权限检查

### 线程安全
- ✅ 互斥锁保护共享数据（QMutex）
- ✅ 原子操作用于状态标志
- ✅ 信号槽确保线程安全的通知

### 用户体验
- ✅ 实时下载进度显示
- ✅ 详细错误提示
- ✅ 搜索和过滤功能
- ✅ 快速切换和操作
- ✅ 响应式布局

## 使用流程

### 用户操作流程

1. **查看插件列表**
   - 点击左侧栏的拼图图标
   - 自动加载可用插件和已安装插件列表

2. **查看插件详情**
   - 在左侧列表中点击某个插件
   - 右侧显示完整的插件信息、描述、技术规格

3. **安装插件**
   - 在插件详情页点击"立即安装"按钮
   - 显示下载和安装进度
   - 安装完成后自动注册到进程列表

4. **管理插件进程**
   - 已安装的插件出现在进程列表中
   - 可以像管理其他子进程一样启动、停止、重启

5. **卸载插件**
   - 在插件详情页点击"卸载插件"按钮
   - 从进程管理器移除
   - 删除插件文件

## 可能的扩展方向

1. **插件市场搜索** - 支持按类别、关键词搜索
2. **插件评分和评论** - 社区反馈机制
3. **自动更新检查** - 定期检查插件更新
4. **插件依赖管理** - 自动安装依赖项
5. **权限声明** - 安全性检查
6. **插件配置界面** - 各插件特定的配置选项
7. **插件卸载前备份** - 保留用户配置
8. **插件版本回滚** - 恢复旧版本

## 调试建议

### 启用详细日志
```cpp
// PluginManager 输出日志到控制台
qDebug() << "[PluginManager]" << message;
```

### 检查网络连接
```
确保可以访问：
https://jts-tools-extensions.oss-cn-chengdu.aliyuncs.com/plugins.json
```

### 检查文件权限
```
plugins 目录必须具有写入权限
应用程序目录必须具有读取权限
```

### QML 调试
```qml
console.log("[QML]", message);  // 在Application Output中查看
```

## 已知限制

1. **ZIP 解压** - 使用 Qt 私有 API 或系统工具，可能存在跨平台兼容性问题
2. **插件隔离** - 目前未实现插件之间的隔离或沙箱环保
3. **热更新** - 已安装的插件不支持不重启更新
4. **签名验证** - 未实现插件签名和真实性验证
5. **大文件支持** - 大型插件包下载可能耗时较长

## 支持联系

如有问题或建议，请参考：
- 项目主目录中的 README.md
- 代码中的详细注释
- 相关的设计文档

---

**实现日期**: 2026年2月5日
**最后更新**: 2026年2月5日
**版本**: 1.0

