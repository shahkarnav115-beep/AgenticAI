#include "IdeWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QShortcut>
#include <QKeySequence>
#include <QDir>
#include <QHeaderView>
#include <QMessageBox>
#include <QFontDatabase>
#include <QFileDialog>

IdeWidget::IdeWidget(QWidget *parent)
    : QWidget(parent),
      m_currentWorkspacePath(QDir::homePath())
{
    setupUi();
    applyDarkTheme();
    emit workspaceChanged(m_currentWorkspacePath);
}

void IdeWidget::setupUi() {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header Toolbar Frame
    auto *topBar = new QWidget(this);
    topBar->setFixedHeight(42);
    topBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    topBar->setStyleSheet("background-color: #161622; border-bottom: 1px solid #292e42;");
    auto *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(10, 6, 10, 6);
    topLayout->setSpacing(10);

    m_statusLabel = new QLabel("🚀 AgenticAI Workspace: " + QFileInfo(m_currentWorkspacePath).fileName(), topBar);
    m_statusLabel->setStyleSheet("color: #7aa2f7; font-weight: bold; font-size: 11pt;");
    topLayout->addWidget(m_statusLabel);

    topLayout->addStretch();

    auto *openFolderBtn = new QPushButton("📂 Open Folder", topBar);
    openFolderBtn->setCursor(Qt::PointingHandCursor);
    openFolderBtn->setStyleSheet("background-color: #24283b; color: #7aa2f7; border: 1px solid #3b4261; font-weight: bold; border-radius: 4px; padding: 5px 12px;");
    connect(openFolderBtn, &QPushButton::clicked, this, &IdeWidget::openFolderDialog);
    topLayout->addWidget(openFolderBtn);

    auto *saveBtn = new QPushButton("💾 Save File", topBar);
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveBtn->setStyleSheet("background-color: #7aa2f7; color: #15161e; font-weight: bold; border-radius: 4px; padding: 5px 12px;");
    connect(saveBtn, &QPushButton::clicked, this, &IdeWidget::saveCurrentFile);
    topLayout->addWidget(saveBtn);

    auto *hideBtn = new QPushButton("✖ Hide IDE", topBar);
    hideBtn->setCursor(Qt::PointingHandCursor);
    hideBtn->setStyleSheet("background-color: #292e42; color: #f7768e; font-weight: bold; border-radius: 4px; padding: 5px 12px;");
    connect(hideBtn, &QPushButton::clicked, this, &IdeWidget::hideRequested);
    topLayout->addWidget(hideBtn);

    layout->addWidget(topBar);

    // Main split layout: File Tree (Left) vs Editor + Console (Right)
    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal, this);

    // Left Panel: Project File Explorer
    m_fileModel = new QFileSystemModel(this);
    m_fileModel->setRootPath(m_currentWorkspacePath);

    m_fileTree = new QTreeView(mainSplitter);
    m_fileTree->setModel(m_fileModel);
    m_fileTree->setRootIndex(m_fileModel->index(m_currentWorkspacePath));
    m_fileTree->hideColumn(1); // Hide size
    m_fileTree->hideColumn(2); // Hide type
    m_fileTree->hideColumn(3); // Hide date
    m_fileTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    connect(m_fileTree, &QTreeView::doubleClicked, this, &IdeWidget::openSelectedFileFromTree);

    // Right Panel Splitter: Editor Tabs (Top) vs Console Log (Bottom)
    QSplitter *rightSplitter = new QSplitter(Qt::Vertical, mainSplitter);

    m_editorTabs = new QTabWidget(rightSplitter);
    m_editorTabs->setTabsClosable(true);
    connect(m_editorTabs, &QTabWidget::tabCloseRequested, this, &IdeWidget::closeTab);

    // Console Output Panel
    m_consoleOutput = new QTextEdit(rightSplitter);
    m_consoleOutput->setReadOnly(true);
    m_consoleOutput->setPlaceholderText("IDE Output Console...");
    m_consoleOutput->append("⚡ AgenticAI IDE initialized. Double click files on left to open in editor.");

    rightSplitter->setStretchFactor(0, 4);
    rightSplitter->setStretchFactor(1, 1);

    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 3);

    layout->addWidget(mainSplitter, 1);

    // Ctrl+S shortcut to save current file
    new QShortcut(QKeySequence::Save, this, SLOT(saveCurrentFile()));
}

void IdeWidget::openFolderDialog() {
    QString folderPath = QFileDialog::getExistingDirectory(this, "Select Workspace Directory", m_currentWorkspacePath);
    if (!folderPath.isEmpty()) {
        setWorkspaceFolder(folderPath);
    }
}

void IdeWidget::setWorkspaceFolder(const QString &folderPath) {
    if (folderPath.isEmpty() || !QDir(folderPath).exists()) return;

    m_currentWorkspacePath = folderPath;
    m_fileModel->setRootPath(m_currentWorkspacePath);
    m_fileTree->setRootIndex(m_fileModel->index(m_currentWorkspacePath));

    if (m_statusLabel) {
        QFileInfo fi(m_currentWorkspacePath);
        m_statusLabel->setText("🚀 AgenticAI Workspace: " + fi.fileName());
    }

    appendLog("📂 Switched IDE Workspace folder to: " + m_currentWorkspacePath);
    emit workspaceChanged(m_currentWorkspacePath);
}

void IdeWidget::openSelectedFileFromTree(const QModelIndex &index) {
    if (!index.isValid()) return;
    if (m_fileModel->isDir(index)) return;

    QString filePath = m_fileModel->filePath(index);
    openFile(filePath);
}

void IdeWidget::openFile(const QString &filePath) {
    if (filePath.isEmpty()) return;

    // Check if file is already open
    if (m_openEditors.contains(filePath)) {
        m_editorTabs->setCurrentWidget(m_openEditors[filePath]);
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        appendLog("⚠️ Error opening file: " + filePath);
        return;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    auto *editor = new QPlainTextEdit();
    QFont font("Consolas", 11);
    font.setStyleHint(QFont::Monospace);
    editor->setFont(font);
    editor->setPlainText(content);
    editor->setProperty("filePath", filePath);

    QFileInfo fi(filePath);
    int tabIndex = m_editorTabs->addTab(editor, fi.fileName());
    m_editorTabs->setTabToolTip(tabIndex, filePath);
    m_editorTabs->setCurrentIndex(tabIndex);

    m_openEditors[filePath] = editor;
    appendLog("📂 Opened file: " + fi.fileName());
}

void IdeWidget::saveCurrentFile() {
    int currIndex = m_editorTabs->currentIndex();
    if (currIndex == -1) return;

    auto *editor = qobject_cast<QPlainTextEdit*>(m_editorTabs->widget(currIndex));
    if (!editor) return;

    QString filePath = editor->property("filePath").toString();
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        appendLog("❌ Failed to save file: " + filePath);
        return;
    }

    QTextStream out(&file);
    out << editor->toPlainText();
    file.close();

    QFileInfo fi(filePath);
    appendLog("💾 Saved file: " + fi.fileName());
}

void IdeWidget::closeTab(int index) {
    QWidget *widget = m_editorTabs->widget(index);
    auto *editor = qobject_cast<QPlainTextEdit*>(widget);
    if (editor) {
        QString filePath = editor->property("filePath").toString();
        m_openEditors.remove(filePath);
    }
    m_editorTabs->removeTab(index);
    delete widget;
}

void IdeWidget::appendLog(const QString &text) {
    if (m_consoleOutput) {
        m_consoleOutput->append(text);
    }
}

void IdeWidget::applyDarkTheme() {
    setStyleSheet(R"(
        QWidget {
            background-color: #1a1b26;
            color: #c0caf5;
        }
        QTreeView {
            background-color: #161622;
            color: #c0caf5;
            border: 1px solid #292e42;
        }
        QTreeView::item:selected {
            background-color: #2f334d;
            color: #7aa2f7;
        }
        QTabWidget::pane {
            border: 1px solid #292e42;
            background-color: #1a1b26;
        }
        QTabBar::tab {
            background-color: #161622;
            color: #a9b1d6;
            padding: 8px 14px;
            border: 1px solid #292e42;
            border-bottom: none;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
        }
        QTabBar::tab:selected {
            background-color: #1a1b26;
            color: #7aa2f7;
            font-weight: bold;
        }
        QPlainTextEdit {
            background-color: #1a1b26;
            color: #c0caf5;
            border: none;
            selection-background-color: #2f334d;
        }
        QTextEdit {
            background-color: #161622;
            color: #9ece6a;
            font-family: 'Consolas', monospace;
            font-size: 10pt;
            border: 1px solid #292e42;
        }
        QSplitter::handle {
            background-color: #292e42;
        }
    )");
}
