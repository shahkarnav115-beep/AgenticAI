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
#include <QLineEdit>
#include <QMap>
#include <QStackedWidget>

class CodeEditor;

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

private slots:
    void onSearchFilterChanged(const QString &text);
    void onEditorCursorChanged(int line, int column);
    void onCurrentTabChanged(int index);
    void updateBreadcrumb();
    void clearConsole();

private:
    void setupUi();
    void applyDarkTheme();
    void showWelcomeState();
    void hideWelcomeState();
    QString fileTypeEmoji(const QString &suffix) const;
    QString detectLanguage(const QString &suffix) const;

    // Header bar
    QLabel *m_statusLabel{nullptr};
    QLabel *m_breadcrumbLabel{nullptr};

    // File explorer
    QTreeView *m_fileTree{nullptr};
    QFileSystemModel *m_fileModel{nullptr};
    QLineEdit *m_searchInput{nullptr};

    // Editor area
    QTabWidget *m_editorTabs{nullptr};
    QStackedWidget *m_editorStack{nullptr};  // Stacks welcome vs. editor tabs
    QWidget *m_welcomeWidget{nullptr};

    // Console
    QTextEdit *m_consoleOutput{nullptr};
    QPushButton *m_clearConsoleBtn{nullptr};

    // Status bar
    QWidget *m_statusBar{nullptr};
    QLabel *m_cursorPosLabel{nullptr};
    QLabel *m_fileInfoLabel{nullptr};
    QLabel *m_encodingLabel{nullptr};
    QLabel *m_languageLabel{nullptr};

    // State
    QMap<QString, CodeEditor*> m_openEditors;
    QString m_currentWorkspacePath;
};

#endif // IDEWIDGET_H
