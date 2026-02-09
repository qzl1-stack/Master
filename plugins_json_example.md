# 插件管理系统 - plugins.json 文件格式说明

## 概述

本文档说明插件管理系统所需的 `plugins.json` 文件的格式和结构。此文件应放置在阿里云OSS存储桶中，供主程序获取插件列表。

## OSS URL

插件列表文件URL：`https://jts-tools-extensions.oss-cn-chengdu.aliyuncs.com/plugins.json`

## JSON 文件结构

```json
{
  "version": "1.0",
  "last_update": "2026-02-05T10:00:00Z",
  "plugins": [
    {
      "id": "plugin-001",
      "name": "示例插件",
      "version": "1.0.0",
      "author": "作者名称",
      "description": "插件简短描述（显示在列表中）",
      "detailed_description": "详细的插件介绍，支持多行文本。\n\n这里可以描述插件的详细功能、使用方法等信息。\n\n支持换行符来组织内容。",
      "icon_type": "default",
      "category": "工具",
      "download_url": "https://jts-tools-extensions.oss-cn-chengdu.aliyuncs.com/plugins/plugin-001.zip",
      "download_size": 1024000,
      "executable": "plugin_name.exe",
      "required_version": "1.0.0",
      "dependencies": [],
      "screenshots": []
    },
    {
      "id": "plugin-002",
      "name": "另一个插件",
      "version": "2.1.5",
      "author": "开发者团队",
      "description": "另一个功能强大的插件",
      "detailed_description": "## 功能特性\n\n1. 特性一：详细说明\n2. 特性二：详细说明\n3. 特性三：详细说明\n\n## 使用方法\n\n安装后可在进程列表中启动。",
      "icon_type": "default",
      "category": "分析",
      "download_url": "https://jts-tools-extensions.oss-cn-chengdu.aliyuncs.com/plugins/plugin-002.zip",
      "download_size": 2048000,
      "executable": "analyzer.exe",
      "required_version": "1.0.0",
      "dependencies": ["vcredist_x64"],
      "screenshots": [
        "https://example.com/screenshot1.png",
        "https://example.com/screenshot2.png"
      ]
    }
  ]
}
```

## 字段说明

### 顶层字段

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `version` | String | 是 | 插件列表版本号 |
| `last_update` | String | 是 | 最后更新时间（ISO 8601格式） |
| `plugins` | Array | 是 | 插件信息数组 |

### 插件对象字段

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `id` | String | 是 | 插件唯一标识符（建议格式：plugin-xxx） |
| `name` | String | 是 | 插件显示名称 |
| `version` | String | 是 | 插件版本号（语义化版本） |
| `author` | String | 是 | 插件作者/开发者 |
| `description` | String | 是 | 插件简短描述（建议不超过100字） |
| `detailed_description` | String | 是 | 插件详细描述（支持换行符\n） |
| `icon_type` | String | 否 | 图标类型（当前仅支持"default"） |
| `category` | String | 是 | 插件分类（如：工具、分析、监控等） |
| `download_url` | String | 是 | 插件压缩包下载地址（完整的OSS URL） |
| `download_size` | Number | 是 | 下载文件大小（字节） |
| `executable` | String | 是 | 可执行文件名（相对于插件安装目录） |
| `required_version` | String | 是 | 所需主程序最低版本号 |
| `dependencies` | Array | 否 | 依赖项列表（字符串数组） |
| `screenshots` | Array | 否 | 截图URL列表（字符串数组） |

## 插件压缩包要求

1. **格式**：ZIP压缩包
2. **结构**：
   ```
   plugin-001.zip
   ├── plugin_name.exe          # 主可执行文件
   ├── config.json              # 配置文件（可选）
   ├── lib/                     # 依赖库目录（可选）
   │   ├── library1.dll
   │   └── library2.dll
   └── resources/               # 资源文件目录（可选）
       └── images/
   ```
3. **可执行文件**：必须与 `executable` 字段指定的文件名一致
4. **路径**：所有文件使用相对路径
5. **权限**：确保可执行文件在解压后具有执行权限

## 安装流程

1. 用户在界面中点击"安装"按钮
2. 系统从OSS下载ZIP文件到临时目录
3. 解压到 `应用程序目录/plugins/插件ID/` 目录
4. 注册插件到进程管理器（使用插件名称作为进程ID）
5. 保存安装信息到配置文件
6. 插件出现在进程列表中，可以像其他子进程一样管理

## 配置文件集成

已安装的插件信息会保存到主程序的 `config.json` 文件中：

```json
{
  "installed_plugins": [
    {
      "id": "plugin-001",
      "name": "示例插件",
      "version": "1.0.0",
      "install_path": "C:/Program/plugins/plugin-001",
      "executable": "plugin_name.exe",
      "...": "其他字段"
    }
  ]
}
```

## 示例完整文件

完整的示例文件已在上方"JSON 文件结构"部分提供。

## 注意事项

1. **URL编码**：确保所有URL正确编码
2. **文件大小**：`download_size` 应准确，用于显示下载进度
3. **版本控制**：使用语义化版本号（如：1.0.0）
4. **唯一性**：每个插件的 `id` 必须全局唯一
5. **安全性**：确保下载URL使用HTTPS协议
6. **兼容性**：`required_version` 用于检查主程序版本兼容性

## 更新插件列表

要添加新插件或更新现有插件：

1. 准备插件ZIP压缩包
2. 上传到OSS存储桶（建议路径：`plugins/插件ID.zip`）
3. 更新 `plugins.json` 文件
4. 上传更新后的 `plugins.json` 到OSS根目录
5. 用户点击"刷新插件列表"即可看到更新




