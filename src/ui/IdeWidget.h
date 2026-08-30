#ifndef IDEWIDGET_H
#define IDEWIDGET_H

#include <QWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QMap>

class IdeWidget : public QWidget {
    Q_OBJECT

public:
    explicit IdeWidget(QWidget *parent = nullptr);
    ~IdeWidget() override = default;

    void openFile(const QString &filePath);
    void appendLog(const QString &text);
    void setWorkspaceFolder(const QString &folderPath);

signals:
    void hideRequested();
    void workspaceChanged(const QString &folderPath);

public slots:
    void openFolderDialog();
    void openSelectedFileFromTree(const QModelIndex &index);
    void saveCurrentFile();
    void closeTab(int index);

private:
    void setupUi();
    void applyDarkTheme();

    QLabel *m_statusLabel{nullptr};
    QTreeView *m_fileTree{nullptr};
    QFileSystemModel *m_fileModel{nullptr};
    QTabWidget *m_editorTabs{nullptr};
    QTextEdit *m_consoleOutput{nullptr};
    QMap<QString, QPlainTextEdit*> m_openEditors;
    QString m_currentWorkspacePath;
};

#endif // IDEWIDGET_H
