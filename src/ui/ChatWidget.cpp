#include "ChatWidget.h"
#include "ConversationSidebar.h"
#include "SettingsDialog.h"
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QScrollBar>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QDesktopServices>
#include <QUrl>
#include <QPixmap>
#include <QKeyEvent>
#include <QPropertyAnimation>
#include <QDateTime>
#include <QRegularExpression>
#include <QApplication>
#include <QClipboard>

// ============================================================================
// Constructor
// ============================================================================

ChatWidget::ChatWidget(QWidget *parent)
    : QWidget(parent),
      m_llamaClient(new LlamaClient(this)),
      m_llamaManager(new LlamaManager(this)),
      m_toolRegistry(new ToolRegistry(this)),
      m_workspaceIndexer(new WorkspaceIndexer(this)),
      m_ideWidget(new IdeWidget(nullptr))
{
    setupUi();
    applyTheme();

    m_llamaClient->setToolRegistry(m_toolRegistry);
    m_llamaClient->setWorkspaceIndexer(m_workspaceIndexer);

    // Index the default workspace on startup
    m_workspaceIndexer->setWorkspacePath(m_toolRegistry->workspacePath());
    m_llamaClient->setWorkspacePath(m_toolRegistry->workspacePath());

    connect(m_llamaClient, &LlamaClient::tokenReceived, this, &ChatWidget::onTokenReceived);
    connect(m_llamaClient, &LlamaClient::toolCallDetected, this, &ChatWidget::onToolCallDetected);
    connect(m_llamaClient, &LlamaClient::modelPropertiesLoaded, this, &ChatWidget::onModelPropertiesLoaded);
    connect(m_llamaClient, &LlamaClient::completionFinished, this, &ChatWidget::onCompletionFinished);
    connect(m_llamaClient, &LlamaClient::errorOccurred, this, &ChatWidget::onErrorOccurred);

    connect(m_llamaManager, &LlamaManager::serverStarted, this, &ChatWidget::onServerStarted);
    connect(m_llamaManager, &LlamaManager::serverError, this, &ChatWidget::onServerError);

    // Thinking animation timer
    m_thinkingAnimTimer = new QTimer(this);
    m_thinkingAnimTimer->setInterval(400);
    connect(m_thinkingAnimTimer, &QTimer::timeout, this, &ChatWidget::updateThinkingAnimation);

    // Input event filter for Enter/Shift+Enter
    m_inputEdit->installEventFilter(this);

    // Initialize first conversation
    m_activeConversationId = QString::number(QDateTime::currentMSecsSinceEpoch());

    updateHeaderContextBar();
}

// ============================================================================
// Event Filter — Enter to send, Shift+Enter for newline, Escape to stop
// ============================================================================

bool ChatWidget::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_inputEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (keyEvent->modifiers() & Qt::ShiftModifier) {
                return false; // Shift+Enter: insert newline
            }
            onSendClicked();
            return true; // Enter: send message
        }
        if (keyEvent->key() == Qt::Key_Escape && m_isGenerating) {
            onStopClicked();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ============================================================================
// UI Setup
// ============================================================================

void ChatWidget::setupUi() {
    auto *outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // --- Conversation Sidebar (collapsible, hidden by default) ---
    m_sidebar = new ConversationSidebar(this);
    m_sidebar->setVisible(false);
    connect(m_sidebar, &ConversationSidebar::newChatRequested, this, &ChatWidget::onNewChatRequested);
    connect(m_sidebar, &ConversationSidebar::conversationSelected, this, &ChatWidget::onConversationSelected);
    connect(m_sidebar, &ConversationSidebar::conversationDeleteRequested, this, &ChatWidget::onConversationDeleteRequested);
    outerLayout->addWidget(m_sidebar);

    // --- Main area (vertical splitter: IDE on top, chat on bottom) ---
    auto *mainArea = new QWidget(this);
    auto *mainAreaLayout = new QVBoxLayout(mainArea);
    mainAreaLayout->setContentsMargins(0, 0, 0, 0);
    mainAreaLayout->setSpacing(0);

    m_mainSplitter = new QSplitter(Qt::Vertical, mainArea);
    m_mainSplitter->setChildrenCollapsible(false);

    // Upper Half: IDE Studio Widget (hidden by default)
    if (!m_ideWidget) {
        m_ideWidget = new IdeWidget(m_mainSplitter);
    }
    m_ideWidget->setVisible(false);
    connect(m_ideWidget, &IdeWidget::hideRequested, this, [this]() {
        m_ideWidget->setVisible(false);
    });
    connect(m_ideWidget, &IdeWidget::workspaceChanged, this, [this](const QString &folderPath) {
        if (m_toolRegistry) m_toolRegistry->setWorkspacePath(folderPath);
        if (m_llamaClient) m_llamaClient->setWorkspacePath(folderPath);
        if (m_workspaceIndexer) m_workspaceIndexer->setWorkspacePath(folderPath);

        // Clear any stale system/tool history referencing old build folders
        m_chatHistory.clear();
        appendAssistantMessage("📂 **Active Workspace Updated**: Locked onto directory `" + folderPath + "` (" + QString::number(m_workspaceIndexer ? m_workspaceIndexer->fileCount() : 0) + " files indexed).");
    });
    m_mainSplitter->addWidget(m_ideWidget);

    // Lower Half: Chat Container
    auto *chatContainer = new QWidget(m_mainSplitter);
    chatContainer->setMinimumHeight(150); // Chat pane can shrink to ~20% but not smaller
    auto *chatMainLayout = new QVBoxLayout(chatContainer);
    chatMainLayout->setContentsMargins(0, 0, 0, 0);
    chatMainLayout->setSpacing(0);

    // ═══════════════════════════════════════════════════════════════════
    // HEADER BAR
    // ═══════════════════════════════════════════════════════════════════
    auto *headerFrame = new QFrame(chatContainer);
    headerFrame->setObjectName("headerFrame");
    auto *headerLayout = new QHBoxLayout(headerFrame);
    headerLayout->setContentsMargins(10, 7, 14, 7);
    headerLayout->setSpacing(8);

    // Sidebar toggle button
    m_sidebarToggle = new QPushButton("☰", headerFrame);
    m_sidebarToggle->setObjectName("sidebarToggle");
    m_sidebarToggle->setCursor(Qt::PointingHandCursor);
    m_sidebarToggle->setFixedSize(36, 36);
    m_sidebarToggle->setToolTip("Toggle conversation sidebar");
    connect(m_sidebarToggle, &QPushButton::clicked, this, &ChatWidget::onSidebarToggled);
    headerLayout->addWidget(m_sidebarToggle);

    // App title
    auto *titleLabel = new QLabel("<b>AgenticAI Chat</b>", headerFrame);
    titleLabel->setObjectName("titleLabel");
    headerLayout->addWidget(titleLabel);

    headerLayout->addSpacing(6);

    // Status dot
    m_statusDot = new QLabel("●", headerFrame);
    m_statusDot->setStyleSheet("color: #565f89; font-size: 10px;");
    m_statusDot->setFixedWidth(14);
    headerLayout->addWidget(m_statusDot);

    // Status label
    m_statusLabel = new QLabel("Offline", headerFrame);
    m_statusLabel->setObjectName("statusLabel");
    headerLayout->addWidget(m_statusLabel);

    headerLayout->addStretch();

    // Context window meter
    auto *contextMeterWidget = new QWidget(headerFrame);
    contextMeterWidget->setStyleSheet("background: transparent; border: none;");
    auto *contextMeterLayout = new QHBoxLayout(contextMeterWidget);
    contextMeterLayout->setContentsMargins(0, 0, 0, 0);
    contextMeterLayout->setSpacing(6);

    m_contextHeaderLabel = new QLabel("Context: Offline", contextMeterWidget);
    m_contextHeaderLabel->setObjectName("contextLabel");

    m_contextProgressBar = new QProgressBar(contextMeterWidget);
    m_contextProgressBar->setFixedWidth(120);
    m_contextProgressBar->setFixedHeight(8);
    m_contextProgressBar->setTextVisible(false);
    m_contextProgressBar->setRange(0, 100);
    m_contextProgressBar->setValue(0);
    m_contextProgressBar->setObjectName("contextProgressBar");

    contextMeterLayout->addWidget(m_contextHeaderLabel);
    contextMeterLayout->addWidget(m_contextProgressBar);
    headerLayout->addWidget(contextMeterWidget);

    headerLayout->addSpacing(4);

    // Settings gear button
    m_settingsButton = new QPushButton("⚙", headerFrame);
    m_settingsButton->setObjectName("settingsButton");
    m_settingsButton->setCursor(Qt::PointingHandCursor);
    m_settingsButton->setFixedSize(36, 36);
    m_settingsButton->setToolTip("Settings");
    connect(m_settingsButton, &QPushButton::clicked, this, &ChatWidget::onSettingsClicked);
    headerLayout->addWidget(m_settingsButton);

    // Load model button
    m_loadModelButton = new QPushButton("Select GGUF Model...", headerFrame);
    m_loadModelButton->setObjectName("loadModelButton");
    m_loadModelButton->setCursor(Qt::PointingHandCursor);
    headerLayout->addWidget(m_loadModelButton);

    chatMainLayout->addWidget(headerFrame);

    // ═══════════════════════════════════════════════════════════════════
    // CHAT SCROLL AREA
    // ═══════════════════════════════════════════════════════════════════
    m_scrollArea = new QScrollArea(chatContainer);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setObjectName("chatScrollArea");

    m_scrollContainer = new QWidget(m_scrollArea);
    m_chatLayout = new QVBoxLayout(m_scrollContainer);
    m_chatLayout->setContentsMargins(0, 20, 0, 20);
    m_chatLayout->setSpacing(4);
    m_chatLayout->addStretch();

    m_scrollContainer->setLayout(m_chatLayout);
    m_scrollArea->setWidget(m_scrollContainer);

    chatMainLayout->addWidget(m_scrollArea, 1);

    // ═══════════════════════════════════════════════════════════════════
    // INPUT AREA
    // ═══════════════════════════════════════════════════════════════════
    auto *inputFrame = new QFrame(chatContainer);
    inputFrame->setObjectName("inputFrame");
    auto *inputOuterLayout = new QVBoxLayout(inputFrame);
    inputOuterLayout->setContentsMargins(16, 10, 16, 16);
    inputOuterLayout->setSpacing(8);

    // Attachment pills container
    m_attachmentFrame = new QWidget(inputFrame);
    m_attachmentPillsLayout = new QHBoxLayout(m_attachmentFrame);
    m_attachmentPillsLayout->setContentsMargins(0, 0, 0, 0);
    m_attachmentPillsLayout->setSpacing(8);
    m_attachmentFrame->setVisible(false);
    inputOuterLayout->addWidget(m_attachmentFrame);

    // Input controls bar
    auto *inputControlsLayout = new QHBoxLayout();
    inputControlsLayout->setContentsMargins(0, 0, 0, 0);
    inputControlsLayout->setSpacing(8);

    m_attachButton = new QPushButton("📎", chatContainer);
    m_attachButton->setObjectName("attachButton");
    m_attachButton->setToolTip("Attach files (up to 5)");
    m_attachButton->setCursor(Qt::PointingHandCursor);
    m_attachButton->setFixedSize(44, 50);

    m_inputEdit = new QTextEdit(chatContainer);
    m_inputEdit->setPlaceholderText("Ask AgenticAI anything or type 'OPEN THE IDE'...");
    m_inputEdit->setFixedHeight(50);
    m_inputEdit->setObjectName("inputEdit");

    m_sendButton = new QPushButton("Send", chatContainer);
    m_sendButton->setObjectName("sendButton");
    m_sendButton->setCursor(Qt::PointingHandCursor);
    m_sendButton->setFixedHeight(50);
    m_sendButton->setFixedWidth(80);

    m_stopButton = new QPushButton("■ Stop", chatContainer);
    m_stopButton->setObjectName("stopButton");
    m_stopButton->setCursor(Qt::PointingHandCursor);
    m_stopButton->setFixedHeight(50);
    m_stopButton->setFixedWidth(80);
    m_stopButton->setVisible(false);

    inputControlsLayout->addWidget(m_attachButton);
    inputControlsLayout->addWidget(m_inputEdit, 1);
    inputControlsLayout->addWidget(m_sendButton);
    inputControlsLayout->addWidget(m_stopButton);

    inputOuterLayout->addLayout(inputControlsLayout);
    chatMainLayout->addWidget(inputFrame);

    m_mainSplitter->addWidget(chatContainer);
    mainAreaLayout->addWidget(m_mainSplitter);
    outerLayout->addWidget(mainArea, 1);

    // Connections
    connect(m_sendButton, &QPushButton::clicked, this, &ChatWidget::onSendClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &ChatWidget::onStopClicked);
    connect(m_loadModelButton, &QPushButton::clicked, this, &ChatWidget::onLoadModelClicked);
    connect(m_attachButton, &QPushButton::clicked, this, &ChatWidget::onAttachFileClicked);

    // Initial Welcome Message
    appendAssistantMessage("Welcome to **AgenticAI**! Click **'Select GGUF Model...'** above to select any `.gguf` file on your computer and start chatting offline!");
}

// ============================================================================
// Premium Dark Theme — Glassmorphic / Tokyo Night inspired
// ============================================================================

void ChatWidget::applyTheme() {
    setStyleSheet(R"(
        QWidget {
            background-color: #0d0e15;
            color: #c0caf5;
            font-family: 'Segoe UI', 'Inter', Arial, sans-serif;
            font-size: 14px;
        }

        #headerFrame {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #131520, stop:1 #151728);
            border-bottom: 1px solid #1e2036;
            min-height: 48px;
        }

        #titleLabel {
            font-size: 16px;
            font-weight: 700;
            color: #7aa2f7;
        }

        #statusLabel {
            color: #565f89;
            font-size: 13px;
        }

        #sidebarToggle {
            background-color: transparent;
            color: #565f89;
            border: 1px solid transparent;
            border-radius: 8px;
            font-size: 18px;
        }
        #sidebarToggle:hover {
            background-color: rgba(122, 162, 247, 0.1);
            color: #7aa2f7;
            border: 1px solid rgba(122, 162, 247, 0.2);
        }

        #settingsButton {
            background-color: transparent;
            color: #565f89;
            border: 1px solid transparent;
            border-radius: 8px;
            font-size: 17px;
        }
        #settingsButton:hover {
            background-color: rgba(122, 162, 247, 0.1);
            color: #7aa2f7;
            border: 1px solid rgba(122, 162, 247, 0.2);
        }

        #loadModelButton {
            background-color: #1a1b2e;
            color: #c0caf5;
            border: 1px solid #2a2d4a;
            border-radius: 8px;
            padding: 7px 16px;
            font-weight: 600;
            font-size: 13px;
        }
        #loadModelButton:hover {
            background-color: #242640;
            color: #7aa2f7;
            border: 1px solid #7aa2f7;
        }

        #contextLabel {
            color: #94e2d5;
            font-size: 12px;
            font-weight: 500;
        }

        #contextProgressBar {
            background-color: #1a1b2e;
            border: 1px solid #2a2d4a;
            border-radius: 4px;
        }
        #contextProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #9ece6a, stop:1 #73daca);
            border-radius: 3px;
        }

        #chatScrollArea {
            background-color: #0d0e15;
            border: none;
        }

        QScrollBar:vertical {
            background: transparent;
            width: 6px;
            margin: 4px 2px;
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

        #inputFrame {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #131520, stop:1 #0f1018);
            border-top: 1px solid #1e2036;
        }

        #attachButton {
            background-color: #1a1b2e;
            color: #565f89;
            border: 1px solid #2a2d4a;
            border-radius: 12px;
            font-size: 18px;
        }
        #attachButton:hover {
            background-color: rgba(122, 162, 247, 0.12);
            color: #7aa2f7;
            border: 1px solid rgba(122, 162, 247, 0.3);
        }

        #inputEdit {
            background-color: #1a1b2e;
            color: #c0caf5;
            border: 1px solid #2a2d4a;
            border-radius: 12px;
            padding: 10px 14px;
            font-size: 14px;
            selection-background-color: rgba(122, 162, 247, 0.3);
        }
        #inputEdit:focus {
            border: 1px solid rgba(122, 162, 247, 0.6);
            background-color: #1c1e32;
        }

        #sendButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #7aa2f7, stop:1 #5d85d4);
            color: #0d0e15;
            font-weight: bold;
            border: none;
            border-radius: 12px;
            font-size: 14px;
        }
        #sendButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #89b4fa, stop:1 #7a9de0);
        }
        #sendButton:disabled {
            background: #2a2d4a;
            color: #565f89;
        }

        #stopButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #f7768e, stop:1 #d4566e);
            color: #ffffff;
            font-weight: bold;
            border: none;
            border-radius: 12px;
            font-size: 13px;
        }
        #stopButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #ff899e, stop:1 #e06880);
        }
    )");
}

// ============================================================================
// Context Window Meter
// ============================================================================

void ChatWidget::updateHeaderContextBar() {
    if (m_maxContextTokens <= 0) {
        m_contextProgressBar->setRange(0, 100);
        m_contextProgressBar->setValue(0);
        m_contextHeaderLabel->setText("🧠 Context: Offline");
        return;
    }

    qint64 totalChars = 0;
    for (const auto &msg : m_chatHistory) {
        totalChars += msg.content.size();
    }
    totalChars += m_currentAssistantText.size();

    int usedTokens = static_cast<int>(totalChars / 4);
    int remainingTokens = qMax(0, m_maxContextTokens - usedTokens);

    m_contextProgressBar->setRange(0, m_maxContextTokens);
    m_contextProgressBar->setValue(qMin(usedTokens, m_maxContextTokens));

    // Dynamic color coding
    double usagePct = (double)usedTokens / (double)m_maxContextTokens;
    QString chunkColor1 = "#9ece6a";
    QString chunkColor2 = "#73daca";
    if (usagePct > 0.85) {
        chunkColor1 = "#f7768e"; chunkColor2 = "#d4566e";
    } else if (usagePct > 0.60) {
        chunkColor1 = "#e0af68"; chunkColor2 = "#d4a054";
    }

    m_contextProgressBar->setStyleSheet(
        "QProgressBar#contextProgressBar { background-color: #1a1b2e; border: 1px solid #2a2d4a; border-radius: 4px; }"
        "QProgressBar#contextProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 " + chunkColor1 + ", stop:1 " + chunkColor2 + "); border-radius: 3px; }"
    );

    auto formatTokens = [](int tokens) -> QString {
        if (tokens >= 1024) return QString::number(tokens / 1024.0, 'f', 1) + "K";
        return QString::number(tokens);
    };

    m_contextHeaderLabel->setText("🧠 Context: " + formatTokens(usedTokens) + " / " + formatTokens(m_maxContextTokens) + " (" + formatTokens(remainingTokens) + " left)");
}

// ============================================================================
// Avatar Helper
// ============================================================================

QWidget* ChatWidget::createAvatar(const QString &label, const QString &bgColor, QWidget *parent) {
    auto *avatar = new QLabel(parent);
    avatar->setFixedSize(32, 32);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setText(label);
    avatar->setStyleSheet(
        "background-color: " + bgColor + ";"
        "color: #0d0e15;"
        "border-radius: 16px;"
        "font-weight: bold;"
        "font-size: 13px;"
        "border: none;"
        "min-width: 32px; max-width: 32px; min-height: 32px; max-height: 32px;"
    );
    return avatar;
}

// ============================================================================
// Bubble Rendering (display only — no state changes)
// ============================================================================

void ChatWidget::renderUserBubble(const QString &text) {
    auto *rowWidget = new QWidget(m_scrollContainer);
    rowWidget->setStyleSheet("background: transparent; border: none;");
    auto *hLayout = new QHBoxLayout(rowWidget);
    hLayout->setContentsMargins(80, 4, 20, 4);
    hLayout->setSpacing(10);

    auto *bubble = new QLabel(rowWidget);
    bubble->setTextFormat(Qt::MarkdownText);
    bubble->setText(text);
    bubble->setWordWrap(true);
    bubble->setMaximumWidth(600);
    bubble->setStyleSheet(
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1e3a5f, stop:1 #1a2744);"
        "color: #c0caf5;"
        "border-radius: 16px;"
        "border-top-right-radius: 4px;"
        "padding: 12px 16px;"
        "border: 1px solid rgba(122, 162, 247, 0.15);"
        "font-size: 14px;"
    );

    auto *avatar = createAvatar("U", "#7aa2f7", rowWidget);

    hLayout->addStretch();
    hLayout->addWidget(bubble, 0);
    hLayout->addWidget(avatar);

    m_chatLayout->takeAt(m_chatLayout->count() - 1);
    m_chatLayout->addWidget(rowWidget);
    m_chatLayout->addStretch();

    smoothScrollToBottom();
}

void ChatWidget::renderAssistantBubble(const QString &text) {
    auto *rowWidget = new QWidget(m_scrollContainer);
    rowWidget->setStyleSheet("background: transparent; border: none;");
    auto *hLayout = new QHBoxLayout(rowWidget);
    hLayout->setContentsMargins(20, 4, 80, 4);
    hLayout->setSpacing(10);

    auto *avatar = createAvatar("✦", "#9ece6a", rowWidget);

    auto *bubble = createFormattedBubble(text, rowWidget);

    hLayout->addWidget(avatar);
    hLayout->addWidget(bubble, 0);
    hLayout->addStretch();

    m_chatLayout->takeAt(m_chatLayout->count() - 1);
    m_chatLayout->addWidget(rowWidget);
    m_chatLayout->addStretch();

    smoothScrollToBottom();
}

// ============================================================================
// Formatted Bubble — Code blocks rendered in copyable sandboxes
// ============================================================================

QWidget* ChatWidget::createFormattedBubble(const QString &text, QWidget *parent) {
    auto *container = new QWidget(parent);
    container->setMaximumWidth(600);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    // Regex to extract fenced code blocks: ```lang\ncode\n```
    QRegularExpression codeBlockRegex("```(\\w*)\\n(.*?)```",
        QRegularExpression::DotMatchesEverythingOption);

    int lastPos = 0;
    bool hasCodeBlocks = false;
    auto it = codeBlockRegex.globalMatch(text);

    while (it.hasNext()) {
        hasCodeBlocks = true;
        auto match = it.next();

        // ── Regular text before this code block ──
        QString before = text.mid(lastPos, match.capturedStart() - lastPos).trimmed();
        if (!before.isEmpty()) {
            auto *textLabel = new QLabel(container);
            textLabel->setTextFormat(Qt::MarkdownText);
            textLabel->setText(before);
            textLabel->setWordWrap(true);
            textLabel->setStyleSheet(
                "background-color: #161622;"
                "color: #c0caf5;"
                "border: 1px solid #1e2036;"
                "border-radius: 16px;"
                "border-top-left-radius: 4px;"
                "padding: 12px 16px;"
                "font-size: 14px;"
            );
            layout->addWidget(textLabel);
        }

        // ── Code block sandbox ──
        QString lang = match.captured(1);
        QString code = match.captured(2);
        if (code.endsWith('\n')) code.chop(1);

        auto *codeFrame = new QFrame(container);
        codeFrame->setObjectName("codeBlockFrame");
        codeFrame->setStyleSheet(
            "QFrame#codeBlockFrame { background-color: #0f1018; border: 1px solid #1e2036; border-radius: 10px; }"
        );
        auto *codeLayout = new QVBoxLayout(codeFrame);
        codeLayout->setContentsMargins(0, 0, 0, 0);
        codeLayout->setSpacing(0);

        // Header bar with language + copy button
        auto *headerBar = new QWidget(codeFrame);
        headerBar->setFixedHeight(32);
        headerBar->setStyleSheet(
            "background-color: #131520;"
            "border-top-left-radius: 10px; border-top-right-radius: 10px;"
            "border-bottom: 1px solid #1e2036;"
        );
        auto *headerLayout = new QHBoxLayout(headerBar);
        headerLayout->setContentsMargins(12, 0, 8, 0);
        headerLayout->setSpacing(8);

        auto *langLabel = new QLabel(lang.isEmpty() ? "code" : lang.toLower(), headerBar);
        langLabel->setStyleSheet(
            "color: #565f89; font-size: 11px; font-weight: 600;"
            "background: transparent; border: none;"
        );
        headerLayout->addWidget(langLabel);
        headerLayout->addStretch();

        auto *copyBtn = new QPushButton("📋 Copy", headerBar);
        copyBtn->setCursor(Qt::PointingHandCursor);
        copyBtn->setFixedHeight(22);
        copyBtn->setStyleSheet(
            "QPushButton { background-color: #1a1b2e; color: #a6adc8; border: 1px solid #2a2d4a;"
            "border-radius: 6px; padding: 2px 10px; font-size: 11px; font-weight: 600; }"
            "QPushButton:hover { background-color: #242640; color: #7aa2f7; border: 1px solid #7aa2f7; }"
        );

        QString codeCopy = code;
        connect(copyBtn, &QPushButton::clicked, this, [codeCopy, copyBtn]() {
            QApplication::clipboard()->setText(codeCopy);
            copyBtn->setText("✓ Copied!");
            copyBtn->setStyleSheet(
                "QPushButton { background-color: rgba(158, 206, 106, 0.15); color: #9ece6a;"
                "border: 1px solid rgba(158, 206, 106, 0.3); border-radius: 6px;"
                "padding: 2px 10px; font-size: 11px; font-weight: 600; }"
            );
            QTimer::singleShot(1500, copyBtn, [copyBtn]() {
                if (copyBtn) {
                    copyBtn->setText("📋 Copy");
                    copyBtn->setStyleSheet(
                        "QPushButton { background-color: #1a1b2e; color: #a6adc8; border: 1px solid #2a2d4a;"
                        "border-radius: 6px; padding: 2px 10px; font-size: 11px; font-weight: 600; }"
                        "QPushButton:hover { background-color: #242640; color: #7aa2f7; border: 1px solid #7aa2f7; }"
                    );
                }
            });
        });
        headerLayout->addWidget(copyBtn);

        codeLayout->addWidget(headerBar);

        // Code content
        auto *codeLabel = new QLabel(codeFrame);
        codeLabel->setText(code);
        codeLabel->setWordWrap(true);
        codeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        codeLabel->setStyleSheet(
            "color: #c0caf5; font-family: 'Consolas', 'JetBrains Mono', monospace;"
            "font-size: 12px; padding: 12px 14px; background: transparent; border: none;"
            "selection-background-color: rgba(122, 162, 247, 0.3);"
        );
        codeLayout->addWidget(codeLabel);

        layout->addWidget(codeFrame);
        lastPos = match.capturedEnd();
    }

    // ── No code blocks: simple markdown label ──
    if (!hasCodeBlocks) {
        auto *bubble = new QLabel(container);
        bubble->setTextFormat(Qt::MarkdownText);
        bubble->setText(text);
        bubble->setWordWrap(true);
        bubble->setStyleSheet(
            "background-color: #161622;"
            "color: #c0caf5;"
            "border: 1px solid #1e2036;"
            "border-radius: 16px;"
            "border-top-left-radius: 4px;"
            "padding: 12px 16px;"
            "font-size: 14px;"
        );
        layout->addWidget(bubble);
        return container;
    }

    // ── Remaining text after last code block ──
    QString remaining = text.mid(lastPos).trimmed();
    if (!remaining.isEmpty()) {
        auto *textLabel = new QLabel(container);
        textLabel->setTextFormat(Qt::MarkdownText);
        textLabel->setText(remaining);
        textLabel->setWordWrap(true);
        textLabel->setStyleSheet(
            "background-color: #161622;"
            "color: #c0caf5;"
            "border: 1px solid #1e2036;"
            "border-radius: 16px;"
            "padding: 12px 16px;"
            "font-size: 14px;"
        );
        layout->addWidget(textLabel);
    }

    return container;
}

// ============================================================================
// Finalize Streaming Bubble — Replace raw label with formatted code blocks
// ============================================================================

void ChatWidget::finalizeAssistantBubble() {
    if (!m_currentAssistantRow || m_currentAssistantText.isEmpty()) return;

    QString cleanedText = ContinuousThinkingEngine::cleanResponseText(m_currentAssistantText);
    if (cleanedText.isEmpty() || !cleanedText.contains("```")) return;

    // Remove the streaming row from the chat layout
    int idx = m_chatLayout->indexOf(m_currentAssistantRow);
    if (idx >= 0) {
        m_chatLayout->takeAt(idx);
        m_currentAssistantRow->deleteLater();
    }

    m_currentAssistantRow = nullptr;
    m_currentAssistantBubble = nullptr;

    // Re-render with code-block sandbox support
    renderAssistantBubble(cleanedText);
}

// ============================================================================
// Public Message API
// ============================================================================

void ChatWidget::appendUserMessage(const QString &text) {
    renderUserBubble(text);
    m_chatHistory.append({"user", text});
    updateHeaderContextBar();
}

void ChatWidget::appendAssistantMessage(const QString &text) {
    renderAssistantBubble(text);
}

// ============================================================================
// Streaming Token Display + Thinking Animation
// ============================================================================

void ChatWidget::updateCurrentAssistantToken(const QString &token) {
    if (!m_currentAssistantBubble) {
        m_currentAssistantText = "";

        auto *rowWidget = new QWidget(m_scrollContainer);
        m_currentAssistantRow = rowWidget;
        rowWidget->setObjectName("currentAssistantRow");
        rowWidget->setStyleSheet("background: transparent; border: none;");
        auto *hLayout = new QHBoxLayout(rowWidget);
        hLayout->setContentsMargins(20, 4, 80, 4);
        hLayout->setSpacing(10);

        auto *avatar = createAvatar("✦", "#9ece6a", rowWidget);

        m_currentAssistantBubble = new QLabel(rowWidget);
        m_currentAssistantBubble->setTextFormat(Qt::MarkdownText);
        m_currentAssistantBubble->setWordWrap(true);
        m_currentAssistantBubble->setMaximumWidth(600);
        m_currentAssistantBubble->setStyleSheet(
            "background-color: #161622;"
            "color: #c0caf5;"
            "border: 1px solid #1e2036;"
            "border-radius: 16px;"
            "border-top-left-radius: 4px;"
            "padding: 12px 16px;"
            "font-size: 14px;"
        );

        hLayout->addWidget(avatar);
        hLayout->addWidget(m_currentAssistantBubble, 0);
        hLayout->addStretch();

        m_chatLayout->takeAt(m_chatLayout->count() - 1);
        m_chatLayout->addWidget(rowWidget);
        m_chatLayout->addStretch();

        // Start thinking animation
        m_thinkingDotCount = 0;
        m_thinkingAnimTimer->start();
    }

    m_currentAssistantText += token;

    // Filter out raw JSON tool calls and think tags
    QString cleanedText = ContinuousThinkingEngine::cleanResponseText(m_currentAssistantText);

    if (cleanedText.isEmpty()) {
        // Keep showing thinking animation (timer handles this)
    } else {
        m_thinkingAnimTimer->stop();
        m_currentAssistantBubble->setTextFormat(Qt::MarkdownText);
        m_currentAssistantBubble->setText(cleanedText);
    }

    updateHeaderContextBar();
    smoothScrollToBottom();
}

void ChatWidget::updateThinkingAnimation() {
    if (!m_currentAssistantBubble) return;

    m_thinkingDotCount = (m_thinkingDotCount + 1) % 4;

    QString dots;
    for (int i = 0; i < 3; ++i) {
        if (i < m_thinkingDotCount || m_thinkingDotCount == 0) {
            dots += (i < (m_thinkingDotCount == 0 ? 1 : m_thinkingDotCount)) ? "● " : "○ ";
        } else {
            dots += "○ ";
        }
    }

    m_currentAssistantBubble->setTextFormat(Qt::RichText);
    m_currentAssistantBubble->setText(
        "<span style='color: #9ece6a; font-size: 16px; letter-spacing: 3px;'>" + dots.trimmed() + "</span>"
        "<span style='color: #565f89; font-style: italic;'>&nbsp; Thinking...</span>"
    );
}

// ============================================================================
// Smooth Scroll
// ============================================================================

void ChatWidget::smoothScrollToBottom() {
    QTimer::singleShot(10, this, [this]() {
        QScrollBar *bar = m_scrollArea->verticalScrollBar();
        auto *anim = new QPropertyAnimation(bar, "value", this);
        anim->setDuration(200);
        anim->setStartValue(bar->value());
        anim->setEndValue(bar->maximum());
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

// ============================================================================
// Generation State Management
// ============================================================================

void ChatWidget::setGeneratingState(bool generating) {
    m_isGenerating = generating;
    m_sendButton->setVisible(!generating);
    m_stopButton->setVisible(generating);
}

void ChatWidget::onStopClicked() {
    if (m_llamaClient) {
        m_llamaClient->abortCurrentRequest();
    }
    m_thinkingAnimTimer->stop();
    setGeneratingState(false);
    if (!m_currentAssistantText.isEmpty()) {
        QString cleanedText = ContinuousThinkingEngine::cleanResponseText(m_currentAssistantText);
        if (!cleanedText.isEmpty()) {
            m_chatHistory.append({"assistant", m_currentAssistantText});
        }
    }
    m_currentAssistantBubble = nullptr;
    m_currentAssistantRow = nullptr;
    updateHeaderContextBar();
}

// ============================================================================
// File Attachments
// ============================================================================

void ChatWidget::onAttachFileClicked() {
    if (m_attachedFilePaths.size() >= MAX_ATTACHED_FILES) {
        appendAssistantMessage("⚠️ **Attachment Limit**: Maximum 5 files allowed per message upload.");
        return;
    }

    QStringList files = QFileDialog::getOpenFileNames(this, "Select Files to Attach (Max 5)", "", "All Files (*)");
    if (!files.isEmpty()) {
        for (const QString &file : files) {
            if (m_attachedFilePaths.size() < MAX_ATTACHED_FILES) {
                if (!m_attachedFilePaths.contains(file)) {
                    m_attachedFilePaths.append(file);
                }
            }
        }
        refreshAttachmentBadges();
    }
}

void ChatWidget::onRemoveSingleAttachment(const QString &filePath) {
    m_attachedFilePaths.removeOne(filePath);
    refreshAttachmentBadges();
}

void ChatWidget::clearAllAttachments() {
    m_attachedFilePaths.clear();
    refreshAttachmentBadges();
}

void ChatWidget::refreshAttachmentBadges() {
    QLayoutItem *item;
    while ((item = m_attachmentPillsLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    if (m_attachedFilePaths.isEmpty()) {
        m_attachmentFrame->setVisible(false);
        m_sendButton->setEnabled(true);
        return;
    }

    qint64 totalBytes = 0;
    for (const QString &path : m_attachedFilePaths) {
        QFileInfo fi(path);
        totalBytes += fi.size();

        auto *pillWidget = new QWidget(m_attachmentFrame);
        auto *pillLayout = new QHBoxLayout(pillWidget);
        pillLayout->setContentsMargins(10, 5, 10, 5);
        pillLayout->setSpacing(6);
        pillWidget->setStyleSheet(
            "background-color: rgba(122, 162, 247, 0.1);"
            "border: 1px solid rgba(122, 162, 247, 0.2);"
            "border-radius: 8px;"
        );

        auto *lbl = new QLabel("📄 " + fi.fileName(), pillWidget);
        lbl->setStyleSheet("color: #7aa2f7; font-size: 13px; border: none; background: transparent;");

        auto *btn = new QPushButton("✖", pillWidget);
        btn->setFixedSize(16, 16);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet("background: transparent; color: #f7768e; border: none; font-size: 11px; font-weight: bold;");

        connect(btn, &QPushButton::clicked, this, [this, path]() {
            onRemoveSingleAttachment(path);
        });

        pillLayout->addWidget(lbl);
        pillLayout->addWidget(btn);
        m_attachmentPillsLayout->addWidget(pillWidget);
    }

    // Attachment count & token estimate label
    int estimatedTokens = static_cast<int>(totalBytes / 4);
    auto *meterLabel = new QLabel(m_attachmentFrame);
    QString countStr = QString::number(m_attachedFilePaths.size()) + "/" + QString::number(MAX_ATTACHED_FILES) + " files";

    if (m_maxContextTokens > 0 && estimatedTokens > m_maxContextTokens) {
        meterLabel->setText("⚠️ **Limit Exceeded**: " + countStr + " (~" + QString::number(estimatedTokens) + " / " + QString::number(m_maxContextTokens) + " tokens)");
        meterLabel->setStyleSheet("color: #f7768e; font-weight: bold; font-size: 12px; margin-left: 8px; background: transparent; border: none;");
        m_sendButton->setEnabled(false);
        m_sendButton->setToolTip("Attached files exceed model capacity!");
    } else {
        meterLabel->setText("📎 " + countStr + " (~" + QString::number(estimatedTokens) + " tokens)");
        meterLabel->setStyleSheet("color: #a6e3a1; font-size: 12px; margin-left: 8px; background: transparent; border: none;");
        m_sendButton->setEnabled(true);
        m_sendButton->setToolTip("");
    }

    m_attachmentPillsLayout->addWidget(meterLabel);
    m_attachmentPillsLayout->addStretch();
    m_attachmentFrame->setVisible(true);
}

// ============================================================================
// Model Loading & Server Events
// ============================================================================

void ChatWidget::onLoadModelClicked() {
    QString filePath = QFileDialog::getOpenFileName(this, "Select GGUF Model File", "", "GGUF Models (*.gguf);;All Files (*)");
    if (!filePath.isEmpty()) {
        QFileInfo fi(filePath);
        m_statusDot->setStyleSheet("color: #e0af68; font-size: 10px;");
        m_statusLabel->setText("Starting (" + fi.fileName() + ")...");
        m_statusLabel->setStyleSheet("color: #e0af68; font-size: 13px;");
        m_llamaManager->startServer(filePath);
    }
}

void ChatWidget::onServerStarted(int port) {
    Q_UNUSED(port);
    QFileInfo fi(m_llamaManager->currentModelPath());
    m_statusDot->setStyleSheet("color: #9ece6a; font-size: 10px;");
    m_statusLabel->setText("Online: " + fi.fileName());
    m_statusLabel->setStyleSheet("color: #9ece6a; font-weight: 600; font-size: 13px;");
    appendAssistantMessage("🟢 **Model Loaded Successfully**: `" + fi.fileName() + "` is ready on GPU/CPU!");

    // Fetch dynamic model context properties
    m_llamaClient->fetchModelProperties();
}

void ChatWidget::onModelPropertiesLoaded(int maxContextTokens, const QString &modelName) {
    Q_UNUSED(modelName);
    m_maxContextTokens = maxContextTokens;
    updateHeaderContextBar();
    refreshAttachmentBadges();
}

void ChatWidget::onServerError(const QString &error) {
    m_statusDot->setStyleSheet("color: #f7768e; font-size: 10px;");
    m_statusLabel->setText("Server Error");
    m_statusLabel->setStyleSheet("color: #f7768e; font-weight: 600; font-size: 13px;");
    appendAssistantMessage("⚠️ **Server Error**: " + error);
}

// ============================================================================
// Send Message
// ============================================================================

void ChatWidget::onSendClicked() {
    QString userText = m_inputEdit->toPlainText().trimmed();
    if (userText.isEmpty() && m_attachedFilePaths.isEmpty()) return;

    // Check if user specifically entered OPEN_THE_IDE prompt
    if (userText.trimmed().compare("OPEN_THE_IDE", Qt::CaseInsensitive) == 0 ||
        userText.trimmed().compare("OPEN THE IDE", Qt::CaseInsensitive) == 0) {
        appendUserMessage(userText);
        if (m_ideWidget) {
            m_ideWidget->setVisible(true);
            if (m_mainSplitter) {
                int totalH = m_mainSplitter->height();
                if (totalH <= 0) totalH = 800;
                m_mainSplitter->setSizes(QList<int>() << totalH * 55 / 100 << totalH * 45 / 100);
            }
        }
        appendAssistantMessage("🚀 **IDE Opened**: Workspace IDE Studio is now visible above!");
        m_inputEdit->clear();
        return;
    }

    QString fullPrompt = userText;
    QString userDisplayBubble = userText;

    // Attach all files content
    if (!m_attachedFilePaths.isEmpty()) {
        QString filesBlock;
        QString displayBadges = "📎 **Attached Files**:\n";

        for (const QString &filePath : m_attachedFilePaths) {
            QFile file(filePath);
            QFileInfo fi(filePath);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&file);
                QString fileContent = in.readAll();
                filesBlock += "[Attached File: " + fi.fileName() + "]\n```\n" + fileContent + "\n```\n\n";
            }
            displayBadges += "• `" + fi.fileName() + "` (" + QString::number(fi.size() / 1024.0, 'f', 1) + " KB)\n";
        }
        fullPrompt = filesBlock + userText;
        userDisplayBubble = displayBadges + (userText.isEmpty() ? "" : "\n" + userText);

        clearAllAttachments();
    }

    // Render user bubble with avatar
    renderUserBubble(userDisplayBubble);

    // Save full prompt to chat history for LLM
    m_chatHistory.append({"user", fullPrompt});
    m_inputEdit->clear();
    updateHeaderContextBar();

    // Set generating state
    setGeneratingState(true);
    m_currentAssistantBubble = nullptr;
    m_currentAssistantText = "";
    m_toolLoopCount = 0; // Reset agentic loop counter on new user message

    m_llamaClient->sendChatCompletion(m_chatHistory, "default");
}

// ============================================================================
// Completion & Token Events
// ============================================================================

void ChatWidget::onTokenReceived(const QString &token) {
    updateCurrentAssistantToken(token);
}

void ChatWidget::onCompletionFinished() {
    setGeneratingState(false);
    m_thinkingAnimTimer->stop();
    if (!m_currentAssistantText.isEmpty()) {
        m_chatHistory.append({"assistant", m_currentAssistantText});
        finalizeAssistantBubble();
        updateHeaderContextBar();
    }
    m_currentAssistantBubble = nullptr;
    m_currentAssistantRow = nullptr;
}

void ChatWidget::onErrorOccurred(const QString &error) {
    setGeneratingState(false);
    m_thinkingAnimTimer->stop();
    appendAssistantMessage("⚠️ **Error**: " + error);
}

// ============================================================================
// Tool Call Handling — with premium styled cards
// ============================================================================

void ChatWidget::onToolCallDetected(const QString &toolName, const QString &result) {
    if (toolName == "open_ide" || toolName == "OPEN_THE_IDE") {
        if (m_ideWidget) {
            m_ideWidget->setVisible(true);
            if (m_mainSplitter) {
                int totalH = m_mainSplitter->height();
                if (totalH <= 0) totalH = 800;
                m_mainSplitter->setSizes(QList<int>() << totalH * 55 / 100 << totalH * 45 / 100);
            }
        }
        appendAssistantMessage("🚀 **Executed Tool**: `OPEN_THE_IDE` - Revealed upper-half Workspace IDE Studio!");
        return;
    }

    // ── Error Card ──
    if (result.startsWith("Error:", Qt::CaseInsensitive) || result.contains("Error", Qt::CaseInsensitive)) {
        auto *rowWidget = new QWidget(m_scrollContainer);
        rowWidget->setStyleSheet("background: transparent; border: none;");
        auto *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(20, 4, 80, 4);
        rowLayout->setSpacing(10);

        auto *avatar = createAvatar("⚡", "#f7768e", rowWidget);

        auto *cardWidget = new QWidget(rowWidget);
        auto *cardLayout = new QVBoxLayout(cardWidget);
        cardLayout->setContentsMargins(14, 12, 14, 12);
        cardWidget->setMaximumWidth(600);
        cardWidget->setStyleSheet(
            "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2d1818, stop:1 #201212);"
            "color: #f7768e;"
            "border: 1px solid rgba(247, 118, 142, 0.3);"
            "border-left: 3px solid #f7768e;"
            "border-radius: 12px;"
        );

        auto *lbl = new QLabel(cardWidget);
        lbl->setTextFormat(Qt::MarkdownText);
        lbl->setText("⚠️ **Execution Error**: `" + toolName + "`\n\n" + result);
        lbl->setWordWrap(true);
        lbl->setStyleSheet("background: transparent; border: none; color: #f7768e;");
        cardLayout->addWidget(lbl);

        rowLayout->addWidget(avatar);
        rowLayout->addWidget(cardWidget, 0);
        rowLayout->addStretch();

        m_chatLayout->takeAt(m_chatLayout->count() - 1);
        m_chatLayout->addWidget(rowWidget);
        m_chatLayout->addStretch();
        setGeneratingState(false);
        return;
    }

    // ── Success Card — File generation tools ──
    if (toolName == "generate_pdf" || toolName == "generate_docx" || toolName == "write_file" || toolName == "generate_image") {
        // Extract file path from result
        QString filePath;
        int singleQuoteStart = result.indexOf('\'');
        int singleQuoteEnd = result.lastIndexOf('\'');
        if (singleQuoteStart != -1 && singleQuoteEnd > singleQuoteStart) {
            filePath = result.mid(singleQuoteStart + 1, singleQuoteEnd - singleQuoteStart - 1);
        }

        auto *rowWidget = new QWidget(m_scrollContainer);
        rowWidget->setStyleSheet("background: transparent; border: none;");
        auto *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(20, 4, 80, 4);
        rowLayout->setSpacing(10);

        auto *avatar = createAvatar("⚡", "#e0af68", rowWidget);

        auto *cardWidget = new QWidget(rowWidget);
        auto *cardLayout = new QVBoxLayout(cardWidget);
        cardLayout->setContentsMargins(14, 12, 14, 12);
        cardLayout->setSpacing(8);
        cardWidget->setMaximumWidth(600);
        cardWidget->setStyleSheet(
            "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #161622, stop:1 #141420);"
            "color: #c0caf5;"
            "border: 1px solid rgba(158, 206, 106, 0.2);"
            "border-left: 3px solid #9ece6a;"
            "border-radius: 12px;"
        );

        auto *lbl = new QLabel(cardWidget);
        lbl->setTextFormat(Qt::MarkdownText);
        lbl->setText("🛠️ **Executed Tool**: `" + toolName + "`\n\n" + result);
        lbl->setWordWrap(true);
        lbl->setStyleSheet("background: transparent; border: none;");
        cardLayout->addWidget(lbl);

        // Inline image preview for generated images
        if (toolName == "generate_image" || filePath.endsWith(".png", Qt::CaseInsensitive) || filePath.endsWith(".jpg", Qt::CaseInsensitive)) {
            QPixmap pix(filePath);
            if (!pix.isNull()) {
                auto *imgPreview = new QLabel(cardWidget);
                imgPreview->setPixmap(pix.scaled(320, 240, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                imgPreview->setStyleSheet("border: 1px solid #2a2d4a; border-radius: 8px; margin-top: 4px; background: transparent;");
                cardLayout->addWidget(imgPreview);
            }
        }

        // Action buttons
        if (!filePath.isEmpty()) {
            auto *btnLayout = new QHBoxLayout();
            btnLayout->setSpacing(8);

            QString openBtnText = (toolName == "generate_image") ? "🖼️ Open Image" : "📂 Open File";
            auto *openFileBtn = new QPushButton(openBtnText, cardWidget);
            openFileBtn->setCursor(Qt::PointingHandCursor);
            openFileBtn->setStyleSheet(
                "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #7aa2f7, stop:1 #5d85d4);"
                "color: #0d0e15; font-weight: bold; border-radius: 8px; padding: 7px 14px; border: none; font-size: 12px;"
            );

            auto *openFolderBtn = new QPushButton("📁 Open Folder", cardWidget);
            openFolderBtn->setCursor(Qt::PointingHandCursor);
            openFolderBtn->setStyleSheet(
                "background-color: #1a1b2e; color: #c0caf5; border: 1px solid #2a2d4a; border-radius: 8px; padding: 7px 14px; font-size: 12px;"
            );

            connect(openFileBtn, &QPushButton::clicked, this, [filePath]() {
                QFileInfo fi(filePath);
                QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absoluteFilePath()));
            });

            connect(openFolderBtn, &QPushButton::clicked, this, [filePath]() {
                QFileInfo fi(filePath);
                QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
            });

            btnLayout->addWidget(openFileBtn);
            btnLayout->addWidget(openFolderBtn);
            btnLayout->addStretch();
            cardLayout->addLayout(btnLayout);
        }

        rowLayout->addWidget(avatar);
        rowLayout->addWidget(cardWidget, 0);
        rowLayout->addStretch();

        m_chatLayout->takeAt(m_chatLayout->count() - 1);
        m_chatLayout->addWidget(rowWidget);
        m_chatLayout->addStretch();

        smoothScrollToBottom();
    } else {
        // ── Generic Tool Result Card (read_file, list_dir, run_command) ──
        auto *rowWidget = new QWidget(m_scrollContainer);
        rowWidget->setStyleSheet("background: transparent; border: none;");
        auto *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(20, 4, 80, 4);
        rowLayout->setSpacing(10);

        auto *avatar = createAvatar("⚡", "#e0af68", rowWidget);

        auto *cardWidget = new QWidget(rowWidget);
        auto *cardLayout = new QVBoxLayout(cardWidget);
        cardLayout->setContentsMargins(14, 12, 14, 12);
        cardWidget->setMaximumWidth(600);
        cardWidget->setStyleSheet(
            "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #161622, stop:1 #141420);"
            "color: #c0caf5;"
            "border: 1px solid rgba(158, 206, 106, 0.2);"
            "border-left: 3px solid #9ece6a;"
            "border-radius: 12px;"
        );

        auto *lbl = new QLabel(cardWidget);
        lbl->setTextFormat(Qt::MarkdownText);
        lbl->setText("🛠️ **Executed Tool**: `" + toolName + "`\n\n```\n" + result + "\n```");
        lbl->setWordWrap(true);
        lbl->setStyleSheet("background: transparent; border: none;");
        cardLayout->addWidget(lbl);

        rowLayout->addWidget(avatar);
        rowLayout->addWidget(cardWidget, 0);
        rowLayout->addStretch();

        m_chatLayout->takeAt(m_chatLayout->count() - 1);
        m_chatLayout->addWidget(rowWidget);
        m_chatLayout->addStretch();

        smoothScrollToBottom();
    }

    // Feed real tool output back to LLM context so it generates truthful answers
    m_chatHistory.append({"user", "[Tool Output for " + toolName + "]:\n" + result});

    // Agentic loop: allow multi-step tool chaining up to MAX_TOOL_LOOPS
    m_toolLoopCount++;
    if (m_toolLoopCount >= MAX_TOOL_LOOPS) {
        appendAssistantMessage("⚠️ **Agentic Loop Limit**: Reached maximum of " + QString::number(MAX_TOOL_LOOPS) + " consecutive tool calls. Stopping to avoid infinite loop.");
        setGeneratingState(false);
        m_toolLoopCount = 0;
        return;
    }

    setGeneratingState(true);
    m_currentAssistantBubble = nullptr;
    m_currentAssistantText = "";
    m_llamaClient->sendChatCompletion(m_chatHistory, "default");
}

// ============================================================================
// Settings Dialog
// ============================================================================

void ChatWidget::onSettingsClicked() {
    auto *dialog = new SettingsDialog(this);
    connect(dialog, &SettingsDialog::settingsApplied, this, [](double temp, double repeat, int fontSize) {
        // Settings will be applied to LlamaClient in future (temperature, repeat_penalty are already sent in payload)
        Q_UNUSED(temp);
        Q_UNUSED(repeat);
        Q_UNUSED(fontSize);
    });
    dialog->exec();
    dialog->deleteLater();
}

// ============================================================================
// Sidebar & Conversation Management
// ============================================================================

void ChatWidget::onSidebarToggled() {
    if (m_sidebar) {
        m_sidebar->setVisible(!m_sidebar->isVisible());
    }
}

void ChatWidget::saveCurrentConversation() {
    if (m_chatHistory.isEmpty()) return;

    // Get title from first user message
    QString title = "New Chat";
    for (const auto &msg : m_chatHistory) {
        if (msg.role == "user") {
            title = msg.content.left(50);
            title.replace(QRegularExpression("[\\n\\r]+"), " ");
            title.replace(QRegularExpression("\\*+"), "");
            title = title.trimmed();
            if (title.isEmpty()) title = "New Chat";
            break;
        }
    }

    ConversationSession session;
    session.id = m_activeConversationId;
    session.title = title;
    session.createdAt = QDateTime::currentDateTime();
    session.messages = m_chatHistory;

    bool isNew = !m_conversations.contains(session.id);
    m_conversations[session.id] = session;

    if (m_sidebar) {
        if (isNew) {
            m_sidebar->addConversation(session.id, session.title);
        }
        m_sidebar->setActiveConversation(session.id);
    }
}

void ChatWidget::clearChatDisplay() {
    delete m_scrollContainer;
    m_scrollContainer = new QWidget(m_scrollArea);
    m_chatLayout = new QVBoxLayout(m_scrollContainer);
    m_chatLayout->setContentsMargins(0, 20, 0, 20);
    m_chatLayout->setSpacing(4);
    m_chatLayout->addStretch();
    m_scrollArea->setWidget(m_scrollContainer);
    m_currentAssistantBubble = nullptr;
    m_currentAssistantRow = nullptr;
    m_currentAssistantText.clear();
}

void ChatWidget::onNewChatRequested() {
    saveCurrentConversation();

    clearChatDisplay();
    m_chatHistory.clear();
    m_activeConversationId = QString::number(QDateTime::currentMSecsSinceEpoch());

    appendAssistantMessage("✨ **New conversation started!** How can I help you?");
    updateHeaderContextBar();
}

void ChatWidget::onConversationSelected(const QString &id) {
    if (id == m_activeConversationId) return;

    saveCurrentConversation();
    loadConversation(id);
}

void ChatWidget::loadConversation(const QString &id) {
    if (!m_conversations.contains(id)) return;

    const auto &session = m_conversations[id];
    m_activeConversationId = id;
    m_chatHistory = session.messages;

    clearChatDisplay();

    // Replay messages into display (rendering only, no state changes)
    for (const auto &msg : session.messages) {
        if (msg.role == "user") {
            // Only show non-tool-output user messages
            if (!msg.content.startsWith("[Tool Output for ")) {
                QString displayText = msg.content.left(300);
                if (msg.content.length() > 300) displayText += "...";
                renderUserBubble(displayText);
            }
        } else if (msg.role == "assistant") {
            QString displayText = msg.content.left(500);
            if (msg.content.length() > 500) displayText += "...";
            renderAssistantBubble(displayText);
        }
    }

    if (m_sidebar) {
        m_sidebar->setActiveConversation(id);
    }
    updateHeaderContextBar();
}

void ChatWidget::onConversationDeleteRequested(const QString &id) {
    m_conversations.remove(id);
    if (m_sidebar) {
        m_sidebar->removeConversation(id);
    }
    if (id == m_activeConversationId) {
        onNewChatRequested();
    }
}
