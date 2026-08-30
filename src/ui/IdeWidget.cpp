#include "IdeWidget.h"
#include "CodeEditor.h"
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
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QDateTime>
#include <QStackedWidget>
#include <QScrollBar>
#include <QSortFilterProxyModel>

IdeWidget::IdeWidget(QWidget *parent)
    : QWidget(parent),
      m_currentWorkspacePath(QDir::homePath())
{
    setupUi();
    applyDarkTheme();
    emit workspaceChanged(m_currentWorkspacePath);
}

// ============================================================================
// File Type Helpers
// ============================================================================

QString IdeWidget::fileTypeEmoji(const QString &suffix) const {
    static const QMap<QString, QString> emojiMap = {
        {"cpp", "⚙️"}, {"cxx", "⚙️"}, {"cc", "⚙️"}, {"c", "⚙️"},
        {"h", "📋"}, {"hpp", "📋"}, {"hxx", "📋"},
        {"py", "🐍"}, {"pyw", "🐍"},
        {"js", "🟨"}, {"ts", "🔷"}, {"jsx", "⚛️"}, {"tsx", "⚛️"},
        {"html", "🌐"}, {"htm", "🌐"}, {"css", "🎨"}, {"scss", "🎨"},
        {"json", "📦"}, {"xml", "📄"}, {"yaml", "📄"}, {"yml", "📄"},
        {"md", "📝"}, {"txt", "📄"}, {"log", "📋"},
        {"rs", "🦀"}, {"go", "🐹"}, {"java", "☕"}, {"kt", "🟣"},
        {"rb", "💎"}, {"php", "🐘"}, {"swift", "🍎"},
        {"sh", "🖥️"}, {"bash", "🖥️"}, {"ps1", "🖥️"}, {"bat", "🖥️"},
        {"cmake", "🔧"}, {"makefile", "🔧"},
        {"png", "🖼️"}, {"jpg", "🖼️"}, {"jpeg", "🖼️"}, {"gif", "🖼️"}, {"svg", "🖼️"},
        {"sql", "🗃️"}, {"db", "🗃️"},
        {"toml", "⚙️"}, {"ini", "⚙️"}, {"cfg", "⚙️"}, {"conf", "⚙️"},
        {"gitignore", "🚫"}, {"dockerignore", "🚫"},
        {"dockerfile", "🐳"}, {"gguf", "🧠"},
    };
    return emojiMap.value(suffix.toLower(), "📄");
}

QString IdeWidget::detectLanguage(const QString &suffix) const {
    static const QMap<QString, QString> langMap = {
        {"cpp", "C++"}, {"cxx", "C++"}, {"cc", "C++"}, {"c", "C"},
        {"h", "C/C++ Header"}, {"hpp", "C++ Header"},
        {"py", "Python"}, {"js", "JavaScript"}, {"ts", "TypeScript"},
        {"jsx", "React JSX"}, {"tsx", "React TSX"},
        {"html", "HTML"}, {"css", "CSS"}, {"scss", "SCSS"},
        {"json", "JSON"}, {"xml", "XML"}, {"yaml", "YAML"}, {"yml", "YAML"},
        {"md", "Markdown"}, {"txt", "Plain Text"},
        {"rs", "Rust"}, {"go", "Go"}, {"java", "Java"}, {"kt", "Kotlin"},
        {"rb", "Ruby"}, {"php", "PHP"}, {"swift", "Swift"},
        {"sh", "Shell"}, {"bash", "Bash"}, {"ps1", "PowerShell"}, {"bat", "Batch"},
        {"cmake", "CMake"}, {"sql", "SQL"},
        {"toml", "TOML"}, {"ini", "INI"},
    };
    return langMap.value(suffix.toLower(), "Plain Text");
}

// ============================================================================
// UI Setup
// ============================================================================

void IdeWidget::setupUi() {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ═══════════════════════════════════════════════════════════════════
    // HEADER BAR — Premium gradient toolbar
    // ═══════════════════════════════════════════════════════════════════
    auto *headerFrame = new QFrame(this);
    headerFrame->setObjectName("ideHeaderFrame");
    headerFrame->setFixedHeight(48);
    auto *headerLayout = new QHBoxLayout(headerFrame);
    headerLayout->setContentsMargins(14, 0, 14, 0);
    headerLayout->setSpacing(10);

    // Workspace label
    m_statusLabel = new QLabel("✦ " + QFileInfo(m_currentWorkspacePath).fileName(), headerFrame);
    m_statusLabel->setObjectName("ideWorkspaceLabel");
    headerLayout->addWidget(m_statusLabel);

    // Breadcrumb
    m_breadcrumbLabel = new QLabel("", headerFrame);
    m_breadcrumbLabel->setObjectName("ideBreadcrumb");
    headerLayout->addWidget(m_breadcrumbLabel);

    headerLayout->addStretch();

    // Open Folder button
    auto *openFolderBtn = new QPushButton("📂 Open Folder", headerFrame);
    openFolderBtn->setObjectName("ideOpenFolderBtn");
    openFolderBtn->setCursor(Qt::PointingHandCursor);
    connect(openFolderBtn, &QPushButton::clicked, this, &IdeWidget::openFolderDialog);
    headerLayout->addWidget(openFolderBtn);

    // Save button
    auto *saveBtn = new QPushButton("💾 Save", headerFrame);
    saveBtn->setObjectName("ideSaveBtn");
    saveBtn->setCursor(Qt::PointingHandCursor);
    connect(saveBtn, &QPushButton::clicked, this, &IdeWidget::saveCurrentFile);
    headerLayout->addWidget(saveBtn);

    // Hide IDE button
    auto *hideBtn = new QPushButton("✕", headerFrame);
    hideBtn->setObjectName("ideHideBtn");
    hideBtn->setCursor(Qt::PointingHandCursor);
    hideBtn->setFixedSize(32, 32);
    hideBtn->setToolTip("Hide IDE Panel");
    connect(hideBtn, &QPushButton::clicked, this, &IdeWidget::hideRequested);
    headerLayout->addWidget(hideBtn);

    layout->addWidget(headerFrame);

    // ═══════════════════════════════════════════════════════════════════
    // MAIN SPLIT: File Tree (Left) | Editor + Console (Right)
    // ═══════════════════════════════════════════════════════════════════
    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainSplitter->setObjectName("ideMainSplitter");

    // ─── Left Panel: File Explorer ───
    auto *leftPanel = new QWidget(mainSplitter);
    leftPanel->setObjectName("ideLeftPanel");
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);

    // Explorer header
    auto *explorerHeader = new QWidget(leftPanel);
    explorerHeader->setObjectName("ideExplorerHeader");
    explorerHeader->setFixedHeight(34);
    auto *explorerHeaderLayout = new QHBoxLayout(explorerHeader);
    explorerHeaderLayout->setContentsMargins(12, 0, 12, 0);
    explorerHeaderLayout->setSpacing(6);

    auto *explorerLabel = new QLabel("EXPLORER", explorerHeader);
    explorerLabel->setObjectName("ideExplorerLabel");
    explorerHeaderLayout->addWidget(explorerLabel);
    explorerHeaderLayout->addStretch();

    leftLayout->addWidget(explorerHeader);

    // Search/filter bar
    m_searchInput = new QLineEdit(leftPanel);
    m_searchInput->setObjectName("ideSearchInput");
    m_searchInput->setPlaceholderText("🔍  Filter files...");
    m_searchInput->setClearButtonEnabled(true);
    connect(m_searchInput, &QLineEdit::textChanged, this, &IdeWidget::onSearchFilterChanged);
    leftLayout->addWidget(m_searchInput);

    // File tree
    m_fileModel = new QFileSystemModel(this);
    m_fileModel->setRootPath(m_currentWorkspacePath);

    m_fileTree = new QTreeView(leftPanel);
    m_fileTree->setObjectName("ideFileTree");
    m_fileTree->setModel(m_fileModel);
    m_fileTree->setRootIndex(m_fileModel->index(m_currentWorkspacePath));
    m_fileTree->hideColumn(1); // Hide size
    m_fileTree->hideColumn(2); // Hide type
    m_fileTree->hideColumn(3); // Hide date
    m_fileTree->header()->setVisible(false);
    m_fileTree->setAnimated(true);
    m_fileTree->setIndentation(16);
    m_fileTree->setHeaderHidden(true);
    connect(m_fileTree, &QTreeView::doubleClicked, this, &IdeWidget::openSelectedFileFromTree);

    leftLayout->addWidget(m_fileTree, 1);

    // ─── Right Panel: Editor Tabs + Console ───
    QSplitter *rightSplitter = new QSplitter(Qt::Vertical, mainSplitter);
    rightSplitter->setObjectName("ideRightSplitter");

    // Editor area — stacked widget (welcome vs. tabs)
    m_editorStack = new QStackedWidget(rightSplitter);

    // Welcome widget (shown when no tabs are open)
    m_welcomeWidget = new QWidget(m_editorStack);
    m_welcomeWidget->setObjectName("ideWelcomeWidget");
    auto *welcomeLayout = new QVBoxLayout(m_welcomeWidget);
    welcomeLayout->setAlignment(Qt::AlignCenter);
    welcomeLayout->setSpacing(16);

    auto *welcomeIcon = new QLabel("✦", m_welcomeWidget);
    welcomeIcon->setObjectName("ideWelcomeIcon");
    welcomeIcon->setAlignment(Qt::AlignCenter);
    welcomeLayout->addWidget(welcomeIcon);

    auto *welcomeTitle = new QLabel("AgenticAI IDE Studio", m_welcomeWidget);
    welcomeTitle->setObjectName("ideWelcomeTitle");
    welcomeTitle->setAlignment(Qt::AlignCenter);
    welcomeLayout->addWidget(welcomeTitle);

    auto *welcomeSubtitle = new QLabel("Open a file from the explorer or use shortcuts below", m_welcomeWidget);
    welcomeSubtitle->setObjectName("ideWelcomeSubtitle");
    welcomeSubtitle->setAlignment(Qt::AlignCenter);
    welcomeLayout->addWidget(welcomeSubtitle);

    welcomeLayout->addSpacing(12);

    // Keyboard shortcuts card
    auto *shortcutsCard = new QFrame(m_welcomeWidget);
    shortcutsCard->setObjectName("ideShortcutsCard");
    auto *shortcutsLayout = new QVBoxLayout(shortcutsCard);
    shortcutsLayout->setContentsMargins(24, 18, 24, 18);
    shortcutsLayout->setSpacing(10);

    auto *shortcutsTitle = new QLabel("⌨️  Keyboard Shortcuts", shortcutsCard);
    shortcutsTitle->setObjectName("ideShortcutsTitleLabel");
    shortcutsLayout->addWidget(shortcutsTitle);

    struct Shortcut { QString key; QString desc; };
    QList<Shortcut> shortcuts = {
        {"Ctrl+S", "Save current file"},
        {"Ctrl+O", "Open folder"},
        {"Double-Click", "Open file from explorer"},
    };
    for (const auto &sc : shortcuts) {
        auto *row = new QHBoxLayout();
        row->setSpacing(12);
        auto *keyLabel = new QLabel(sc.key, shortcutsCard);
        keyLabel->setObjectName("ideShortcutKey");
        keyLabel->setFixedWidth(110);
        auto *descLabel = new QLabel(sc.desc, shortcutsCard);
        descLabel->setObjectName("ideShortcutDesc");
        row->addWidget(keyLabel);
        row->addWidget(descLabel);
        row->addStretch();
        shortcutsLayout->addLayout(row);
    }

    welcomeLayout->addWidget(shortcutsCard, 0, Qt::AlignCenter);
    welcomeLayout->addStretch();

    m_editorStack->addWidget(m_welcomeWidget); // index 0

    // Tab widget for editors
    m_editorTabs = new QTabWidget(m_editorStack);
    m_editorTabs->setObjectName("ideEditorTabs");
    m_editorTabs->setTabsClosable(true);
    m_editorTabs->setMovable(true);
    m_editorTabs->setDocumentMode(true);
    connect(m_editorTabs, &QTabWidget::tabCloseRequested, this, &IdeWidget::closeTab);
    connect(m_editorTabs, &QTabWidget::currentChanged, this, &IdeWidget::onCurrentTabChanged);

    m_editorStack->addWidget(m_editorTabs); // index 1
    m_editorStack->setCurrentIndex(0); // show welcome by default

    rightSplitter->addWidget(m_editorStack);

    // ─── Console Output Panel ───
    auto *consoleFrame = new QWidget(rightSplitter);
    consoleFrame->setObjectName("ideConsoleFrame");
    auto *consoleLayout = new QVBoxLayout(consoleFrame);
    consoleLayout->setContentsMargins(0, 0, 0, 0);
    consoleLayout->setSpacing(0);

    // Console header
    auto *consoleHeader = new QWidget(consoleFrame);
    consoleHeader->setObjectName("ideConsoleHeader");
    consoleHeader->setFixedHeight(30);
    auto *consoleHeaderLayout = new QHBoxLayout(consoleHeader);
    consoleHeaderLayout->setContentsMargins(12, 0, 8, 0);
    consoleHeaderLayout->setSpacing(6);

    auto *consoleLabel = new QLabel("TERMINAL", consoleHeader);
    consoleLabel->setObjectName("ideConsoleLabel");
    consoleHeaderLayout->addWidget(consoleLabel);
    consoleHeaderLayout->addStretch();

    m_clearConsoleBtn = new QPushButton("🗑", consoleHeader);
    m_clearConsoleBtn->setObjectName("ideClearConsoleBtn");
    m_clearConsoleBtn->setCursor(Qt::PointingHandCursor);
    m_clearConsoleBtn->setFixedSize(24, 24);
    m_clearConsoleBtn->setToolTip("Clear console");
    connect(m_clearConsoleBtn, &QPushButton::clicked, this, &IdeWidget::clearConsole);
    consoleHeaderLayout->addWidget(m_clearConsoleBtn);

    consoleLayout->addWidget(consoleHeader);

    m_consoleOutput = new QTextEdit(consoleFrame);
    m_consoleOutput->setObjectName("ideConsoleOutput");
    m_consoleOutput->setReadOnly(true);
    m_consoleOutput->setPlaceholderText("Output will appear here...");
    consoleLayout->addWidget(m_consoleOutput, 1);

    rightSplitter->addWidget(consoleFrame);

    rightSplitter->setStretchFactor(0, 4);
    rightSplitter->setStretchFactor(1, 1);

    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 3);

    layout->addWidget(mainSplitter, 1);

    // ═══════════════════════════════════════════════════════════════════
    // STATUS BAR — VS Code style
    // ═══════════════════════════════════════════════════════════════════
    m_statusBar = new QWidget(this);
    m_statusBar->setObjectName("ideStatusBar");
    m_statusBar->setFixedHeight(26);
    auto *statusBarLayout = new QHBoxLayout(m_statusBar);
    statusBarLayout->setContentsMargins(12, 0, 12, 0);
    statusBarLayout->setSpacing(16);

    m_cursorPosLabel = new QLabel("Ln 1, Col 1", m_statusBar);
    m_cursorPosLabel->setObjectName("ideStatusItem");
    statusBarLayout->addWidget(m_cursorPosLabel);

    m_encodingLabel = new QLabel("UTF-8", m_statusBar);
    m_encodingLabel->setObjectName("ideStatusItem");
    statusBarLayout->addWidget(m_encodingLabel);

    m_languageLabel = new QLabel("Plain Text", m_statusBar);
    m_languageLabel->setObjectName("ideStatusItem");
    statusBarLayout->addWidget(m_languageLabel);

    statusBarLayout->addStretch();

    m_fileInfoLabel = new QLabel("No file open", m_statusBar);
    m_fileInfoLabel->setObjectName("ideStatusItem");
    statusBarLayout->addWidget(m_fileInfoLabel);

    layout->addWidget(m_statusBar);

    // Keyboard shortcuts
    new QShortcut(QKeySequence::Save, this, SLOT(saveCurrentFile()));
    new QShortcut(QKeySequence::Open, this, SLOT(openFolderDialog()));

    // Initial log message
    appendLog("⚡ AgenticAI IDE Studio initialized. Double-click files in the explorer to open them.");
}

// ============================================================================
// Welcome State Management
// ============================================================================

void IdeWidget::showWelcomeState() {
    if (m_editorStack) {
        m_editorStack->setCurrentIndex(0);
    }
    if (m_cursorPosLabel) m_cursorPosLabel->setText("Ln 1, Col 1");
    if (m_languageLabel) m_languageLabel->setText("Plain Text");
    if (m_fileInfoLabel) m_fileInfoLabel->setText("No file open");
    if (m_breadcrumbLabel) m_breadcrumbLabel->setText("");
}

void IdeWidget::hideWelcomeState() {
    if (m_editorStack) {
        m_editorStack->setCurrentIndex(1);
    }
}

// ============================================================================
// Breadcrumb Update
// ============================================================================

void IdeWidget::updateBreadcrumb() {
    if (!m_editorTabs || m_editorTabs->currentIndex() == -1) {
        if (m_breadcrumbLabel) m_breadcrumbLabel->setText("");
        return;
    }

    auto *editor = qobject_cast<CodeEditor*>(m_editorTabs->currentWidget());
    if (!editor) return;

    QString filePath = editor->filePath();
    if (filePath.isEmpty()) return;

    // Build breadcrumb relative to workspace
    QString relativePath = filePath;
    if (!m_currentWorkspacePath.isEmpty() && filePath.startsWith(m_currentWorkspacePath)) {
        relativePath = filePath.mid(m_currentWorkspacePath.length());
        if (relativePath.startsWith('/') || relativePath.startsWith('\\'))
            relativePath = relativePath.mid(1);
    }

    // Replace path separators with arrows
    QString breadcrumb = relativePath;
    breadcrumb.replace('\\', " › ");
    breadcrumb.replace('/', " › ");

    if (m_breadcrumbLabel) {
        m_breadcrumbLabel->setText("  ›  " + breadcrumb);
    }
}

// ============================================================================
// File Search / Filter
// ============================================================================

void IdeWidget::onSearchFilterChanged(const QString &text) {
    if (!m_fileModel) return;

    if (text.isEmpty()) {
        m_fileModel->setNameFilters(QStringList());
        m_fileModel->setNameFilterDisables(true);
    } else {
        m_fileModel->setNameFilters(QStringList() << ("*" + text + "*"));
        m_fileModel->setNameFilterDisables(false); // Hide non-matching files
    }
}

// ============================================================================
// Cursor / Tab Changed
// ============================================================================

void IdeWidget::onEditorCursorChanged(int line, int column) {
    if (m_cursorPosLabel) {
        m_cursorPosLabel->setText(QString("Ln %1, Col %2").arg(line).arg(column));
    }
}

void IdeWidget::onCurrentTabChanged(int index) {
    if (index == -1) {
        showWelcomeState();
        return;
    }

    hideWelcomeState();

    auto *editor = qobject_cast<CodeEditor*>(m_editorTabs->widget(index));
    if (!editor) return;

    QString filePath = editor->filePath();
    QFileInfo fi(filePath);

    // Update status bar
    if (m_languageLabel) {
        m_languageLabel->setText(detectLanguage(fi.suffix()));
    }
    if (m_fileInfoLabel) {
        qint64 bytes = fi.size();
        QString sizeStr;
        if (bytes >= 1024 * 1024) {
            sizeStr = QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
        } else if (bytes >= 1024) {
            sizeStr = QString::number(bytes / 1024.0, 'f', 1) + " KB";
        } else {
            sizeStr = QString::number(bytes) + " B";
        }
        m_fileInfoLabel->setText(fi.fileName() + " — " + sizeStr);
    }

    updateBreadcrumb();
}

// ============================================================================
// Console
// ============================================================================

void IdeWidget::clearConsole() {
    if (m_consoleOutput) {
        m_consoleOutput->clear();
        appendLog("🗑️ Console cleared.");
    }
}

// ============================================================================
// Folder / File Operations
// ============================================================================

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
        m_statusLabel->setText("✦ " + fi.fileName());
    }

    appendLog("📂 Workspace changed to: " + m_currentWorkspacePath);
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
        hideWelcomeState();
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

    auto *editor = new CodeEditor();
    editor->setPlainText(content);
    editor->setFilePath(filePath);

    // Connect cursor position updates
    connect(editor, &CodeEditor::cursorPositionInfo, this, &IdeWidget::onEditorCursorChanged);

    QFileInfo fi(filePath);
    QString emoji = fileTypeEmoji(fi.suffix());
    int tabIndex = m_editorTabs->addTab(editor, emoji + " " + fi.fileName());
    m_editorTabs->setTabToolTip(tabIndex, filePath);
    m_editorTabs->setCurrentIndex(tabIndex);

    m_openEditors[filePath] = editor;
    hideWelcomeState();

    appendLog("📂 Opened: " + fi.fileName() + " (" + detectLanguage(fi.suffix()) + ")");
}

void IdeWidget::saveCurrentFile() {
    int currIndex = m_editorTabs->currentIndex();
    if (currIndex == -1) return;

    auto *editor = qobject_cast<CodeEditor*>(m_editorTabs->widget(currIndex));
    if (!editor) return;

    QString filePath = editor->filePath();
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        appendLog("❌ Failed to save: " + filePath);
        return;
    }

    QTextStream out(&file);
    out << editor->toPlainText();
    file.close();

    QFileInfo fi(filePath);
    appendLog("💾 Saved: " + fi.fileName());
}

void IdeWidget::closeTab(int index) {
    QWidget *widget = m_editorTabs->widget(index);
    auto *editor = qobject_cast<CodeEditor*>(widget);
    if (editor) {
        m_openEditors.remove(editor->filePath());
    }
    m_editorTabs->removeTab(index);
    delete widget;

    if (m_editorTabs->count() == 0) {
        showWelcomeState();
    }
}

// ============================================================================
// Console Log — with timestamps and color coding
// ============================================================================

void IdeWidget::appendLog(const QString &text) {
    if (!m_consoleOutput) return;

    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");

    // Color-code based on content
    QString color = "#9ece6a"; // default: green for info
    if (text.contains("⚠️") || text.contains("Warning", Qt::CaseInsensitive)) {
        color = "#e0af68"; // amber for warnings
    } else if (text.contains("❌") || text.contains("Error", Qt::CaseInsensitive)) {
        color = "#f7768e"; // red for errors
    } else if (text.contains("📂") || text.contains("💾")) {
        color = "#7aa2f7"; // blue for file ops
    } else if (text.contains("🗑️")) {
        color = "#565f89"; // dim for meta
    }

    m_consoleOutput->append(
        "<span style='color: #3d4066; font-size: 10px;'>[" + timestamp + "]</span> "
        "<span style='color: " + color + ";'>" + text + "</span>"
    );
}

// ============================================================================
// Premium Dark Theme — Tokyo Night + Glassmorphism
// ============================================================================

void IdeWidget::applyDarkTheme() {
    setStyleSheet(R"(
        QWidget {
            background-color: #0d0e15;
            color: #c0caf5;
            font-family: 'Segoe UI', 'Inter', Arial, sans-serif;
            font-size: 13px;
        }

        /* ── Header Bar ── */
        #ideHeaderFrame {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #131520, stop:1 #151728);
            border-bottom: 1px solid #1e2036;
        }

        #ideWorkspaceLabel {
            color: #7aa2f7;
            font-weight: 700;
            font-size: 13px;
            border: none;
            background: transparent;
        }

        #ideBreadcrumb {
            color: #565f89;
            font-size: 12px;
            border: none;
            background: transparent;
        }

        #ideOpenFolderBtn {
            background-color: #1a1b2e;
            color: #c0caf5;
            border: 1px solid #2a2d4a;
            border-radius: 6px;
            padding: 5px 14px;
            font-weight: 600;
            font-size: 12px;
        }
        #ideOpenFolderBtn:hover {
            background-color: #242640;
            color: #7aa2f7;
            border: 1px solid #7aa2f7;
        }

        #ideSaveBtn {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #7aa2f7, stop:1 #5d85d4);
            color: #0d0e15;
            font-weight: bold;
            border: none;
            border-radius: 6px;
            padding: 5px 14px;
            font-size: 12px;
        }
        #ideSaveBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #89b4fa, stop:1 #7a9de0);
        }

        #ideHideBtn {
            background-color: transparent;
            color: #565f89;
            border: 1px solid transparent;
            border-radius: 6px;
            font-size: 16px;
            font-weight: bold;
        }
        #ideHideBtn:hover {
            background-color: rgba(247, 118, 142, 0.12);
            color: #f7768e;
            border: 1px solid rgba(247, 118, 142, 0.3);
        }

        /* ── Left Panel / Explorer ── */
        #ideLeftPanel {
            background-color: #0a0b12;
            border-right: 1px solid #1a1c30;
        }

        #ideExplorerHeader {
            background-color: #0f1018;
            border-bottom: 1px solid #1a1c30;
        }

        #ideExplorerLabel {
            color: #565f89;
            font-size: 10px;
            font-weight: 700;
            letter-spacing: 1.5px;
            border: none;
            background: transparent;
        }

        #ideSearchInput {
            background-color: #131520;
            color: #c0caf5;
            border: 1px solid #1e2036;
            border-left: none;
            border-right: none;
            padding: 6px 12px;
            font-size: 12px;
        }
        #ideSearchInput:focus {
            border-color: rgba(122, 162, 247, 0.4);
            background-color: #161622;
        }

        #ideFileTree {
            background-color: #0a0b12;
            color: #a6adc8;
            border: none;
            outline: none;
            font-size: 12px;
        }
        #ideFileTree::item {
            padding: 3px 6px;
            border-radius: 4px;
            margin: 0px 4px;
        }
        #ideFileTree::item:selected {
            background-color: rgba(122, 162, 247, 0.12);
            color: #7aa2f7;
        }
        #ideFileTree::item:hover:!selected {
            background-color: rgba(122, 162, 247, 0.06);
            color: #c0caf5;
        }
        #ideFileTree::branch {
            background: transparent;
        }
        #ideFileTree::branch:has-children:!has-siblings:closed,
        #ideFileTree::branch:closed:has-children:has-siblings {
            image: none;
            border-image: none;
        }
        #ideFileTree::branch:open:has-children:!has-siblings,
        #ideFileTree::branch:open:has-children:has-siblings {
            image: none;
            border-image: none;
        }

        /* ── Editor Tabs ── */
        #ideEditorTabs::pane {
            border: none;
            background-color: #0d0e15;
        }

        #ideEditorTabs > QTabBar::tab {
            background-color: #0f1018;
            color: #565f89;
            padding: 8px 16px;
            border: none;
            border-bottom: 2px solid transparent;
            border-right: 1px solid #1a1c30;
            font-size: 12px;
            min-width: 100px;
        }
        #ideEditorTabs > QTabBar::tab:selected {
            background-color: #0d0e15;
            color: #c0caf5;
            border-bottom: 2px solid #7aa2f7;
            font-weight: 600;
        }
        #ideEditorTabs > QTabBar::tab:hover:!selected {
            background-color: #131520;
            color: #a6adc8;
        }
        #ideEditorTabs > QTabBar::close-button {
            image: none;
            subcontrol-position: right;
        }
        #ideEditorTabs > QTabBar::close-button:hover {
            background: rgba(247, 118, 142, 0.15);
            border-radius: 4px;
        }

        /* ── Code Editor (QPlainTextEdit inside CodeEditor) ── */
        CodeEditor {
            background-color: #0d0e15;
            color: #c0caf5;
            border: none;
            selection-background-color: rgba(122, 162, 247, 0.25);
            selection-color: #e0e5ff;
        }

        /* ── Console Frame ── */
        #ideConsoleFrame {
            background-color: #0a0b12;
        }

        #ideConsoleHeader {
            background-color: #0f1018;
            border-top: 1px solid #1a1c30;
            border-bottom: 1px solid #1a1c30;
        }

        #ideConsoleLabel {
            color: #565f89;
            font-size: 10px;
            font-weight: 700;
            letter-spacing: 1.5px;
            border: none;
            background: transparent;
        }

        #ideClearConsoleBtn {
            background: transparent;
            border: none;
            color: #565f89;
            font-size: 12px;
            border-radius: 4px;
        }
        #ideClearConsoleBtn:hover {
            background-color: rgba(122, 162, 247, 0.1);
            color: #7aa2f7;
        }

        #ideConsoleOutput {
            background-color: #0a0b12;
            color: #9ece6a;
            font-family: 'Consolas', 'JetBrains Mono', monospace;
            font-size: 11px;
            border: none;
            padding: 8px 12px;
        }

        /* ── Status Bar ── */
        #ideStatusBar {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #131520, stop:1 #151728);
            border-top: 1px solid #1e2036;
        }

        #ideStatusItem {
            color: #565f89;
            font-size: 11px;
            border: none;
            background: transparent;
        }

        /* ── Splitter Handles ── */
        QSplitter::handle {
            background-color: #1a1c30;
        }
        QSplitter::handle:horizontal {
            width: 1px;
        }
        QSplitter::handle:vertical {
            height: 1px;
        }

        /* ── Scrollbars ── */
        QScrollBar:vertical {
            background: transparent;
            width: 6px;
            margin: 4px 1px;
        }
        QScrollBar::handle:vertical {
            background: #2a2d4a;
            min-height: 30px;
            border-radius: 3px;
        }
        QScrollBar::handle:vertical:hover {
            background: #3d4066;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: transparent;
        }
        QScrollBar:horizontal {
            background: transparent;
            height: 6px;
            margin: 1px 4px;
        }
        QScrollBar::handle:horizontal {
            background: #2a2d4a;
            min-width: 30px;
            border-radius: 3px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #3d4066;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: transparent;
        }

        /* ── Welcome Widget ── */
        #ideWelcomeWidget {
            background-color: #0d0e15;
        }

        #ideWelcomeIcon {
            color: #7aa2f7;
            font-size: 48px;
            border: none;
            background: transparent;
        }

        #ideWelcomeTitle {
            color: #c0caf5;
            font-size: 22px;
            font-weight: 700;
            border: none;
            background: transparent;
        }

        #ideWelcomeSubtitle {
            color: #565f89;
            font-size: 13px;
            border: none;
            background: transparent;
        }

        #ideShortcutsCard {
            background-color: #131520;
            border: 1px solid #1e2036;
            border-radius: 12px;
            max-width: 360px;
        }

        #ideShortcutsTitleLabel {
            color: #a6adc8;
            font-weight: 600;
            font-size: 13px;
            border: none;
            background: transparent;
            margin-bottom: 4px;
        }

        #ideShortcutKey {
            color: #7aa2f7;
            background-color: #1a1b2e;
            border: 1px solid #2a2d4a;
            border-radius: 4px;
            padding: 3px 8px;
            font-family: 'Consolas', monospace;
            font-size: 11px;
            font-weight: 600;
        }

        #ideShortcutDesc {
            color: #a6adc8;
            font-size: 12px;
            border: none;
            background: transparent;
        }
    )");
}
