#ifndef FOLDER_DIALOG_HELPER_H
#define FOLDER_DIALOG_HELPER_H

#include <QObject>
#include <QString>

/**
 * @brief FolderDialogHelper 文件夹选择对话框助手类
 * 
 * 用于在 QML 中调用原生文件夹选择对话框
 */
class FolderDialogHelper : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString selectedFolder READ selectedFolder NOTIFY folderSelected)

public:
    explicit FolderDialogHelper(QObject* parent = nullptr);
    ~FolderDialogHelper() override = default;

    /**
     * @brief 获取选中的文件夹路径
     * @return 文件夹路径
     */
    QString selectedFolder() const { return selected_folder_; }

    /**
     * @brief 打开文件夹选择对话框
     * @param title 对话框标题
     * @param startFolder 起始文件夹路径（可选）
     */
    Q_INVOKABLE void openDialog(const QString& title = QString(), const QString& startFolder = QString());

signals:
    /**
     * @brief 文件夹选择完成信号
     * @param folderPath 选择的文件夹路径
     */
    void folderSelected(const QString& folderPath);

    /**
     * @brief 对话框被取消信号
     */
    void dialogRejected();

private:
    QString selected_folder_;
};

#endif // FOLDER_DIALOG_HELPER_H

