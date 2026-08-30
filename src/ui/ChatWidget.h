#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QLabel>
#include <QComboBox>
#include <QProgressBar>
#include <QStringList>
#include <QSplitter>
#include <QTimer>
#include "../agent/LlamaClient.h"
#include "../agent/LlamaManager.h"
#include "../agent/ToolRegistry.h"
#include "../agent/WorkspaceIndexer.h"
#include "../continuous_thinking/ContinuousThinkingEngine.h"
#include "IdeWidget.h"

class ChatWidget : public QWidget {
    Q_OBJECT

public:
    explicit ChatWidget(QWidget *parent = nullptr);
    ~ChatWidget() = default;

    static constexpr int MAX_ATTACHED_FILES = 5;

public slots:
    void appendUserMessage(const QString &text);
    void appendAssistantMessage(const QString &text);
    void updateCurrentAssistantToken(const QString &token);

private slots:
    void onSendClicked();
    void onLoadModelClicked();
    void onAttachFileClicked();
    void onRemoveSingleAttachment(const QString &filePath);
    void clearAllAttachments();
    void onTokenReceived(const QString &token);
    void onCompletionFinished();
    void onErrorOccurred(const QString &error);
    void onToolCallDetected(const QString &toolName, const QString &result);
    void onModelPropertiesLoaded(int maxContextTokens, const QString &modelName);
    void onServerStarted(int port);
    void onServerError(const QString &error);

private:
    void setupUi();
    void applyTheme();
    void refreshAttachmentBadges();
    void updateHeaderContextBar();

    QSplitter *m_mainSplitter{nullptr};
    QVBoxLayout *m_chatLayout{nullptr};
    QScrollArea *m_scrollArea{nullptr};
    QWidget *m_scrollContainer{nullptr};
    QTextEdit *m_inputEdit{nullptr};
    QPushButton *m_sendButton{nullptr};
    QPushButton *m_attachButton{nullptr};
    QPushButton *m_loadModelButton{nullptr};
    QLabel *m_statusLabel{nullptr};

    // Header Context Bar
    QLabel *m_contextHeaderLabel{nullptr};
    QProgressBar *m_contextProgressBar{nullptr};

    QWidget *m_attachmentFrame{nullptr};
    QHBoxLayout *m_attachmentPillsLayout{nullptr};
    QStringList m_attachedFilePaths;
    int m_maxContextTokens{0}; // 0 = Offline / No model loaded yet

    LlamaClient *m_llamaClient{nullptr};
    LlamaManager *m_llamaManager{nullptr};
    ToolRegistry *m_toolRegistry{nullptr};
    WorkspaceIndexer *m_workspaceIndexer{nullptr};
    IdeWidget *m_ideWidget{nullptr};

    int m_toolLoopCount{0};
    static constexpr int MAX_TOOL_LOOPS = 5;

    QList<ChatMessage> m_chatHistory;
    QLabel *m_currentAssistantBubble{nullptr};
    QString m_currentAssistantText;
};

#endif // CHATWIDGET_H
