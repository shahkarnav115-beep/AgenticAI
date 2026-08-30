#include "ChatWidget.h"
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

ChatWidget::ChatWidget(QWidget *parent)
    : QWidget(parent),
      m_llamaClient(new LlamaClient(this)),
      m_llamaManager(new LlamaManager(this)),
      m_toolRegistry(new ToolRegistry(this)),
      m_ideWidget(new IdeWidget(nullptr))
{
    setupUi();
    applyTheme();

    m_llamaClient->setToolRegistry(m_toolRegistry);

    connect(m_llamaClient, &LlamaClient::tokenReceived, this, &ChatWidget::onTokenReceived);
    connect(m_llamaClient, &LlamaClient::toolCallDetected, this, &ChatWidget::onToolCallDetected);
    connect(m_llamaClient, &LlamaClient::modelPropertiesLoaded, this, &ChatWidget::onModelPropertiesLoaded);
    connect(m_llamaClient, &LlamaClient::completionFinished, this, &ChatWidget::onCompletionFinished);
    connect(m_llamaClient, &LlamaClient::errorOccurred, this, &ChatWidget::onErrorOccurred);

    connect(m_llamaManager, &LlamaManager::serverStarted, this, &ChatWidget::onServerStarted);
    connect(m_llamaManager, &LlamaManager::serverError, this, &ChatWidget::onServerError);

    updateHeaderContextBar();
}

void ChatWidget::setupUi() {
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    m_mainSplitter = new QSplitter(Qt::Vertical, this);

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

        // Clear any stale system/tool history referencing old build folders
        m_chatHistory.clear();
        appendAssistantMessage("📂 **Active Workspace Updated**: Locked onto directory `" + folderPath + "`.");
    });
    m_mainSplitter->addWidget(m_ideWidget);

    // Lower Half: Chat Container
    auto *chatContainer = new QWidget(m_mainSplitter);
    auto *mainLayout = new QVBoxLayout(chatContainer);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Top Header / Toolbar
    auto *headerFrame = new QFrame(chatContainer);
    headerFrame->setObjectName("headerFrame");
    auto *headerLayout = new QHBoxLayout(headerFrame);
    headerLayout->setContentsMargins(16, 10, 16, 10);
    headerLayout->setSpacing(12);

    auto *titleLabel = new QLabel("<b>AgenticAI Chat</b>", chatContainer);
    titleLabel->setObjectName("titleLabel");

    m_statusLabel = new QLabel("Offline (No model loaded)", chatContainer);
    m_statusLabel->setStyleSheet("color: #a6adc8; font-size: 13px;");

    // Context Window Meter Widget
    auto *contextMeterWidget = new QWidget(headerFrame);
    auto *contextMeterLayout = new QHBoxLayout(contextMeterWidget);
    contextMeterLayout->setContentsMargins(0, 0, 0, 0);
    contextMeterLayout->setSpacing(8);

    m_contextHeaderLabel = new QLabel("🧠 0 / 128K tokens (128K left)", contextMeterWidget);
    m_contextHeaderLabel->setStyleSheet("color: #94e2d5; font-size: 12px; font-weight: 500;");

    m_contextProgressBar = new QProgressBar(contextMeterWidget);
    m_contextProgressBar->setFixedWidth(130);
    m_contextProgressBar->setFixedHeight(10);
    m_contextProgressBar->setTextVisible(false);
    m_contextProgressBar->setRange(0, m_maxContextTokens);
    m_contextProgressBar->setValue(0);
    m_contextProgressBar->setObjectName("contextProgressBar");

    contextMeterLayout->addWidget(m_contextHeaderLabel);
    contextMeterLayout->addWidget(m_contextProgressBar);

    m_loadModelButton = new QPushButton("Select GGUF Model...", chatContainer);
    m_loadModelButton->setObjectName("loadModelButton");
    m_loadModelButton->setCursor(Qt::PointingHandCursor);

    headerLayout->addWidget(titleLabel);
    headerLayout->addSpacing(8);
    headerLayout->addWidget(m_statusLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(contextMeterWidget);
    headerLayout->addSpacing(8);
    headerLayout->addWidget(m_loadModelButton);

    mainLayout->addWidget(headerFrame);

    // Scroll Area for Message Stream
    m_scrollArea = new QScrollArea(chatContainer);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setObjectName("chatScrollArea");

    m_scrollContainer = new QWidget(m_scrollArea);
    m_chatLayout = new QVBoxLayout(m_scrollContainer);
    m_chatLayout->setContentsMargins(24, 20, 24, 20);
    m_chatLayout->setSpacing(16);
    m_chatLayout->addStretch();

    m_scrollContainer->setLayout(m_chatLayout);
    m_scrollArea->setWidget(m_scrollContainer);

    mainLayout->addWidget(m_scrollArea, 1);

    // Input Area Frame
    auto *inputFrame = new QFrame(chatContainer);
    inputFrame->setObjectName("inputFrame");
    auto *inputOuterLayout = new QVBoxLayout(inputFrame);
    inputOuterLayout->setContentsMargins(16, 10, 16, 16);
    inputOuterLayout->setSpacing(8);

    // Multiple Attachment Pills Container
    m_attachmentFrame = new QWidget(inputFrame);
    m_attachmentPillsLayout = new QHBoxLayout(m_attachmentFrame);
    m_attachmentPillsLayout->setContentsMargins(0, 0, 0, 0);
    m_attachmentPillsLayout->setSpacing(8);

    m_attachmentFrame->setVisible(false);
    inputOuterLayout->addWidget(m_attachmentFrame);

    // Input Controls Bar
    auto *inputControlsLayout = new QHBoxLayout();
    inputControlsLayout->setContentsMargins(0, 0, 0, 0);
    inputControlsLayout->setSpacing(8);

    m_attachButton = new QPushButton("📎", chatContainer);
    m_attachButton->setObjectName("attachButton");
    m_attachButton->setToolTip("Attach Multiple Files or Code");
    m_attachButton->setCursor(Qt::PointingHandCursor);
    m_attachButton->setFixedSize(42, 50);

    m_inputEdit = new QTextEdit(chatContainer);
    m_inputEdit->setPlaceholderText("Ask AgenticAI anything or type 'OPEN_THE_IDE'...");
    m_inputEdit->setFixedHeight(50);
    m_inputEdit->setObjectName("inputEdit");

    m_sendButton = new QPushButton("Send", chatContainer);
    m_sendButton->setObjectName("sendButton");
    m_sendButton->setCursor(Qt::PointingHandCursor);
    m_sendButton->setFixedHeight(50);
    m_sendButton->setFixedWidth(80);

    inputControlsLayout->addWidget(m_attachButton);
    inputControlsLayout->addWidget(m_inputEdit, 1);
    inputControlsLayout->addWidget(m_sendButton);

    inputOuterLayout->addLayout(inputControlsLayout);
    mainLayout->addWidget(inputFrame);

    m_mainSplitter->addWidget(chatContainer);
    outerLayout->addWidget(m_mainSplitter);

    // Connections
    connect(m_sendButton, &QPushButton::clicked, this, &ChatWidget::onSendClicked);
    connect(m_loadModelButton, &QPushButton::clicked, this, &ChatWidget::onLoadModelClicked);
    connect(m_attachButton, &QPushButton::clicked, this, &ChatWidget::onAttachFileClicked);

    // Initial Welcome Message
    appendAssistantMessage("Welcome to **AgenticAI**! Click **'Select GGUF Model...'** above to select any `.gguf` file on your computer and start chatting offline!");
}

void ChatWidget::applyTheme() {
    setStyleSheet(R"(
        QWidget {
            background-color: #0d0e15;
            color: #c0caf5;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 14px;
        }

        #headerFrame {
            background-color: #13141f;
            border-bottom: 1px solid #222436;
        }

        #titleLabel {
            font-size: 16px;
            font-weight: bold;
            color: #7aa2f7;
        }

        #loadModelButton {
            background-color: #1c1d2d;
            color: #c0caf5;
            border: 1px solid #2e3047;
            border-radius: 6px;
            padding: 6px 14px;
            font-weight: 500;
        }

        #loadModelButton:hover {
            background-color: #292a3d;
            color: #7aa2f7;
            border: 1px solid #7aa2f7;
        }

        #contextProgressBar {
            background-color: #1c1d2d;
            border: 1px solid #2e3047;
            border-radius: 5px;
        }

        #contextProgressBar::chunk {
            background-color: #9ece6a;
            border-radius: 4px;
        }

        #chatScrollArea {
            background-color: #0d0e15;
            border: none;
        }

        QScrollBar:vertical {
            background: #0d0e15;
            width: 8px;
            margin: 0px;
        }

        QScrollBar::handle:vertical {
            background: #222436;
            min-height: 20px;
            border-radius: 4px;
        }

        QScrollBar::handle:vertical:hover {
            background: #3b3e5e;
        }

        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }

        #inputFrame {
            background-color: #13141f;
            border-top: 1px solid #222436;
        }

        #attachButton {
            background-color: #1c1d2d;
            color: #7aa2f7;
            border: 1px solid #2e3047;
            border-radius: 8px;
            font-size: 18px;
        }

        #attachButton:hover {
            background-color: #292a3d;
            border: 1px solid #7aa2f7;
        }

        #inputEdit {
            background-color: #1c1d2d;
            color: #c0caf5;
            border: 1px solid #2e3047;
            border-radius: 8px;
            padding: 8px;
        }

        #inputEdit:focus {
            border: 1px solid #7aa2f7;
        }

        #sendButton {
            background-color: #7aa2f7;
            color: #0d0e15;
            font-weight: bold;
            border: none;
            border-radius: 8px;
        }

        #sendButton:hover {
            background-color: #89b4fa;
        }
    )");
}

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

    // Dynamic Color Coding based on usage %
    double usagePct = (double)usedTokens / (double)m_maxContextTokens;
    QString chunkColor = "#9ece6a"; // Green default
    if (usagePct > 0.85) {
        chunkColor = "#f7768e"; // Red (>85%)
    } else if (usagePct > 0.60) {
        chunkColor = "#e0af68"; // Yellow (60-85%)
    }

    m_contextProgressBar->setStyleSheet("QProgressBar#contextProgressBar { background-color: #1c1d2d; border: 1px solid #2e3047; border-radius: 5px; } QProgressBar#contextProgressBar::chunk { background-color: " + chunkColor + "; border-radius: 4px; }");

    QString usedStr = (usedTokens >= 1024) ? QString::number(usedTokens / 1024.0, 'f', 1) + "K" : QString::number(usedTokens);
    QString maxStr = (m_maxContextTokens >= 1024) ? QString::number(m_maxContextTokens / 1024.0, 'f', 0) + "K" : QString::number(m_maxContextTokens);
    QString leftStr = (remainingTokens >= 1024) ? QString::number(remainingTokens / 1024.0, 'f', 1) + "K" : QString::number(remainingTokens);

    m_contextHeaderLabel->setText("🧠 Context: " + usedStr + " / " + maxStr + " (" + leftStr + " left)");
}

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
    // Clear layout children
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
        pillLayout->setContentsMargins(8, 4, 8, 4);
        pillLayout->setSpacing(6);
        pillWidget->setStyleSheet("background-color: #222436; border: 1px solid #2e3047; border-radius: 6px;");

        auto *lbl = new QLabel("📄 " + fi.fileName(), pillWidget);
        lbl->setStyleSheet("color: #7aa2f7; font-size: 13px;");

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

    // Attachment Count & Token Estimate Label
    int estimatedTokens = static_cast<int>(totalBytes / 4);
    auto *meterLabel = new QLabel(m_attachmentFrame);
    QString countStr = QString::number(m_attachedFilePaths.size()) + "/" + QString::number(MAX_ATTACHED_FILES) + " files";

    if (m_maxContextTokens > 0 && estimatedTokens > m_maxContextTokens) {
        meterLabel->setText("⚠️ **Limit Exceeded**: " + countStr + " (~" + QString::number(estimatedTokens) + " / " + QString::number(m_maxContextTokens) + " tokens)");
        meterLabel->setStyleSheet("color: #f7768e; font-weight: bold; font-size: 12px; margin-left: 8px;");
        m_sendButton->setEnabled(false);
        m_sendButton->setToolTip("Attached files exceed model capacity!");
    } else {
        meterLabel->setText("📎 " + countStr + " (~" + QString::number(estimatedTokens) + " tokens)");
        meterLabel->setStyleSheet("color: #a6e3a1; font-size: 12px; margin-left: 8px;");
        m_sendButton->setEnabled(true);
        m_sendButton->setToolTip("");
    }

    m_attachmentPillsLayout->addWidget(meterLabel);
    m_attachmentPillsLayout->addStretch();
    m_attachmentFrame->setVisible(true);
}

void ChatWidget::onLoadModelClicked() {
    QString filePath = QFileDialog::getOpenFileName(this, "Select GGUF Model File", "", "GGUF Models (*.gguf);;All Files (*)");
    if (!filePath.isEmpty()) {
        QFileInfo fi(filePath);
        m_statusLabel->setText("Starting llama-server (" + fi.fileName() + ")...");
        m_llamaManager->startServer(filePath);
    }
}

void ChatWidget::onServerStarted(int port) {
    Q_UNUSED(port);
    QFileInfo fi(m_llamaManager->currentModelPath());
    m_statusLabel->setText("🟢 Online: " + fi.fileName());
    m_statusLabel->setStyleSheet("color: #9ece6a; font-weight: bold; font-size: 13px;");
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
    m_statusLabel->setText("🔴 Server Error");
    m_statusLabel->setStyleSheet("color: #f7768e; font-weight: bold; font-size: 13px;");
    appendAssistantMessage("⚠️ **Server Error**: " + error);
}

void ChatWidget::appendUserMessage(const QString &text) {
    auto *bubble = new QLabel(m_scrollContainer);
    bubble->setText(text);
    bubble->setWordWrap(true);
    bubble->setStyleSheet("background-color: #222436; color: #c0caf5; border-radius: 12px; padding: 12px 16px; border: 1px solid #2e3047;");

    auto *hLayout = new QHBoxLayout();
    hLayout->addStretch();
    hLayout->addWidget(bubble, 0);

    m_chatLayout->takeAt(m_chatLayout->count() - 1);
    m_chatLayout->addLayout(hLayout);
    m_chatLayout->addStretch();

    QScrollBar *bar = m_scrollArea->verticalScrollBar();
    bar->setValue(bar->maximum());

    m_chatHistory.append({"user", text});
    updateHeaderContextBar();
}

void ChatWidget::appendAssistantMessage(const QString &text) {
    auto *bubble = new QLabel(m_scrollContainer);
    bubble->setTextFormat(Qt::MarkdownText);
    bubble->setText(text);
    bubble->setWordWrap(true);
    bubble->setStyleSheet("background-color: #161622; color: #c0caf5; border: 1px solid #222436; border-radius: 12px; padding: 12px 16px;");

    auto *hLayout = new QHBoxLayout();
    hLayout->addWidget(bubble, 0);
    hLayout->addStretch();

    m_chatLayout->takeAt(m_chatLayout->count() - 1);
    m_chatLayout->addLayout(hLayout);
    m_chatLayout->addStretch();

    QScrollBar *bar = m_scrollArea->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void ChatWidget::updateCurrentAssistantToken(const QString &token) {
    if (!m_currentAssistantBubble) {
        m_currentAssistantText = "";
        m_currentAssistantBubble = new QLabel(m_scrollContainer);
        m_currentAssistantBubble->setTextFormat(Qt::MarkdownText);
        m_currentAssistantBubble->setWordWrap(true);
        m_currentAssistantBubble->setStyleSheet("background-color: #161622; color: #c0caf5; border: 1px solid #222436; border-radius: 12px; padding: 12px 16px;");

        auto *hLayout = new QHBoxLayout();
        hLayout->addWidget(m_currentAssistantBubble, 0);
        hLayout->addStretch();

        m_chatLayout->takeAt(m_chatLayout->count() - 1);
        m_chatLayout->addLayout(hLayout);
        m_chatLayout->addStretch();
    }

    m_currentAssistantText += token;

    // Filter out raw JSON tool calls and think tags so raw JSON is NEVER shown to user
    QString cleanedText = ContinuousThinkingEngine::cleanResponseText(m_currentAssistantText);

    if (cleanedText.isEmpty()) {
        m_currentAssistantBubble->setText("🧠 *Thinking...*");
    } else {
        m_currentAssistantBubble->setText(cleanedText);
    }
    updateHeaderContextBar();

    QScrollBar *bar = m_scrollArea->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void ChatWidget::onSendClicked() {
    QString userText = m_inputEdit->toPlainText().trimmed();
    if (userText.isEmpty() && m_attachedFilePaths.isEmpty()) return;

    // Check if user specifically entered OPEN_THE_IDE prompt
    if (userText.trimmed().compare("OPEN_THE_IDE", Qt::CaseInsensitive) == 0) {
        appendUserMessage(userText);
        if (m_ideWidget) {
            m_ideWidget->setVisible(true);
            if (m_mainSplitter) {
                m_mainSplitter->setSizes(QList<int>() << 450 << 350);
            }
        }
        appendAssistantMessage("🚀 **OPEN_THE_IDE**: Revealed upper-half Workspace IDE Studio!");
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

    // Render compact clean badge in UI
    auto *bubble = new QLabel(m_scrollContainer);
    bubble->setTextFormat(Qt::MarkdownText);
    bubble->setText(userDisplayBubble);
    bubble->setWordWrap(true);
    bubble->setStyleSheet("background-color: #222436; color: #c0caf5; border-radius: 12px; padding: 12px 16px; border: 1px solid #2e3047;");

    auto *hLayout = new QHBoxLayout();
    hLayout->addStretch();
    hLayout->addWidget(bubble, 0);

    m_chatLayout->takeAt(m_chatLayout->count() - 1);
    m_chatLayout->addLayout(hLayout);
    m_chatLayout->addStretch();

    QScrollBar *bar = m_scrollArea->verticalScrollBar();
    bar->setValue(bar->maximum());

    // Save full prompt to chat history for LLM
    m_chatHistory.append({"user", fullPrompt});
    m_inputEdit->clear();
    updateHeaderContextBar();

    m_sendButton->setEnabled(false);
    m_currentAssistantBubble = nullptr;
    m_currentAssistantText = "";

    m_llamaClient->sendChatCompletion(m_chatHistory, "default");
}

void ChatWidget::onTokenReceived(const QString &token) {
    updateCurrentAssistantToken(token);
}

void ChatWidget::onCompletionFinished() {
    m_sendButton->setEnabled(true);
    if (!m_currentAssistantText.isEmpty()) {
        m_chatHistory.append({"assistant", m_currentAssistantText});
        updateHeaderContextBar();
    }
}

void ChatWidget::onErrorOccurred(const QString &error) {
    m_sendButton->setEnabled(true);
    appendAssistantMessage("⚠️ **Error**: " + error);
}

void ChatWidget::onToolCallDetected(const QString &toolName, const QString &result) {
    if (toolName == "open_ide" || toolName == "OPEN_THE_IDE") {
        if (m_ideWidget) {
            m_ideWidget->setVisible(true);
            if (m_mainSplitter) {
                m_mainSplitter->setSizes(QList<int>() << 450 << 350);
            }
        }
        appendAssistantMessage("🚀 **Executed Tool**: `OPEN_THE_IDE` - Revealed upper-half Workspace IDE Studio!");
        return;
    }

    // Check if execution returned an error
    if (result.startsWith("Error:", Qt::CaseInsensitive) || result.contains("Error", Qt::CaseInsensitive)) {
        auto *cardWidget = new QWidget(m_scrollContainer);
        auto *cardLayout = new QVBoxLayout(cardWidget);
        cardLayout->setContentsMargins(12, 10, 12, 10);
        cardWidget->setStyleSheet("background-color: #2d1818; color: #f7768e; border: 1px solid #f7768e; border-radius: 12px;");

        auto *lbl = new QLabel(cardWidget);
        lbl->setTextFormat(Qt::MarkdownText);
        lbl->setText("⚠️ **Execution Error**: `" + toolName + "`\n\n" + result);
        lbl->setWordWrap(true);
        cardLayout->addWidget(lbl);

        auto *hLayout = new QHBoxLayout();
        hLayout->addWidget(cardWidget, 0);
        hLayout->addStretch();

        m_chatLayout->takeAt(m_chatLayout->count() - 1);
        m_chatLayout->addLayout(hLayout);
        m_chatLayout->addStretch();
        m_sendButton->setEnabled(true);
        return;
    }

    if (toolName == "generate_pdf" || toolName == "generate_docx" || toolName == "write_file" || toolName == "generate_image") {
        // Extract file path from tool result message
        QString filePath;
        int singleQuoteStart = result.indexOf('\'');
        int singleQuoteEnd = result.lastIndexOf('\'');
        if (singleQuoteStart != -1 && singleQuoteEnd > singleQuoteStart) {
            filePath = result.mid(singleQuoteStart + 1, singleQuoteEnd - singleQuoteStart - 1);
        }

        auto *cardWidget = new QWidget(m_scrollContainer);
        auto *cardLayout = new QVBoxLayout(cardWidget);
        cardLayout->setContentsMargins(12, 10, 12, 10);
        cardLayout->setSpacing(8);
        cardWidget->setStyleSheet("background-color: #161622; color: #c0caf5; border: 1px solid #222436; border-radius: 12px;");

        auto *lbl = new QLabel(cardWidget);
        lbl->setTextFormat(Qt::MarkdownText);
        lbl->setText("🛠️ **Executed Tool**: `" + toolName + "`\n\n" + result);
        lbl->setWordWrap(true);
        cardLayout->addWidget(lbl);

        // If tool generated an image, render inline preview thumbnail!
        if (toolName == "generate_image" || filePath.endsWith(".png", Qt::CaseInsensitive) || filePath.endsWith(".jpg", Qt::CaseInsensitive)) {
            QPixmap pix(filePath);
            if (!pix.isNull()) {
                auto *imgPreview = new QLabel(cardWidget);
                imgPreview->setPixmap(pix.scaled(320, 240, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                imgPreview->setStyleSheet("border: 1px solid #2e3047; border-radius: 8px; margin-top: 4px;");
                cardLayout->addWidget(imgPreview);
            }
        }

        if (!filePath.isEmpty()) {
            auto *btnLayout = new QHBoxLayout();
            btnLayout->setSpacing(8);

            QString openBtnText = (toolName == "generate_image") ? "🖼️ Open Image" : "📂 Open Document";
            auto *openFileBtn = new QPushButton(openBtnText, cardWidget);
            openFileBtn->setCursor(Qt::PointingHandCursor);
            openFileBtn->setStyleSheet("background-color: #7aa2f7; color: #0d0e15; font-weight: bold; border-radius: 6px; padding: 6px 12px;");

            auto *openFolderBtn = new QPushButton("📁 Open Folder", cardWidget);
            openFolderBtn->setCursor(Qt::PointingHandCursor);
            openFolderBtn->setStyleSheet("background-color: #1c1d2d; color: #c0caf5; border: 1px solid #2e3047; border-radius: 6px; padding: 6px 12px;");

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

        auto *hLayout = new QHBoxLayout();
        hLayout->addWidget(cardWidget, 0);
        hLayout->addStretch();

        m_chatLayout->takeAt(m_chatLayout->count() - 1);
        m_chatLayout->addLayout(hLayout);
        m_chatLayout->addStretch();

        QScrollBar *bar = m_scrollArea->verticalScrollBar();
        bar->setValue(bar->maximum());
    } else {
        auto *cardWidget = new QWidget(m_scrollContainer);
        auto *cardLayout = new QVBoxLayout(cardWidget);
        cardLayout->setContentsMargins(12, 10, 12, 10);
        cardWidget->setStyleSheet("background-color: #161622; color: #c0caf5; border: 1px solid #222436; border-radius: 12px;");

        auto *lbl = new QLabel(cardWidget);
        lbl->setTextFormat(Qt::MarkdownText);
        lbl->setText("🛠️ **Executed Tool**: `" + toolName + "`\n\n```\n" + result + "\n```");
        lbl->setWordWrap(true);
        cardLayout->addWidget(lbl);

        auto *hLayout = new QHBoxLayout();
        hLayout->addWidget(cardWidget, 0);
        hLayout->addStretch();

        m_chatLayout->takeAt(m_chatLayout->count() - 1);
        m_chatLayout->addLayout(hLayout);
        m_chatLayout->addStretch();

        QScrollBar *bar = m_scrollArea->verticalScrollBar();
        bar->setValue(bar->maximum());
    }

    // Feed real tool output back to LLM context so it generates truthful answers
    m_chatHistory.append({"user", "[Tool Output for " + toolName + "]:\n" + result});
    m_sendButton->setEnabled(false);
    m_currentAssistantBubble = nullptr;
    m_currentAssistantText = "";
    m_llamaClient->sendChatCompletion(m_chatHistory, "default");
}
