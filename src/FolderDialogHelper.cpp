#include "FolderDialogHelper.h"
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

FolderDialogHelper::FolderDialogHelper(QObject* parent)
    : QObject(parent)
    , selected_folder_()
{
}

void FolderDialogHelper::openDialog(const QString& title, const QString& startFolder)
{
    QString dialogTitle = title.isEmpty() ? "选择工作目录" : title;
    
    // 确定起始文件夹
    QString initialDir;
    if (!startFolder.isEmpty() && QDir(startFolder).exists()) {
        initialDir = startFolder;
    } else {
        // 使用用户文档目录作为默认起始位置
        initialDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        if (initialDir.isEmpty()) {
            initialDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        }
    }
    
    qDebug() << "[FolderDialogHelper] 打开文件夹选择对话框，标题:" << dialogTitle
             << "起始文件夹:" << initialDir;
    
    // 打开文件夹选择对话框
    QString selectedPath = QFileDialog::getExistingDirectory(
        nullptr,
        dialogTitle,
        initialDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (!selectedPath.isEmpty()) {
        selected_folder_ = QDir::toNativeSeparators(selectedPath);
        qDebug() << "[FolderDialogHelper] 用户选择了文件夹:" << selected_folder_;
        emit folderSelected(selected_folder_);
    } else {
        qDebug() << "[FolderDialogHelper] 用户取消了文件夹选择";
        emit dialogRejected();
    }
}

