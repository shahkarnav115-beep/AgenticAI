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
#include <QDateTime>
#include "../agent/LlamaClient.h"
#include "../agent/LlamaManager.h"
#include "../agent/ToolRegistry.h"
#include "../agent/WorkspaceIndexer.h"
#include "../continuous_thinking/ContinuousThinkingEngine.h"
#include "IdeWidget.h"

class ConversationSidebar;
class SettingsDialog;

struct ConversationSession {
    QString id;
    QString title;
    QDateTime createdAt;
    QList<ChatMessage> messages;
};

class ChatWidget : public QWidget {
    Q_OBJECT

public:
    explicit ChatWidget(QWidget *parent = nullptr);
    ~ChatWidget() = default;

    static constexpr int MAX_ATTACHED_FILES = 5;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

public slots:
    void appendUserMessage(const QString &text);
    void appendAssistantMessage(const QString &text);
    void updateCurrentAssistantToken(const QString &token);

private slots:
    void onSendClicked();
    void onStopClicked();
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
    void onSettingsClicked();
    void onSidebarToggled();
    void updateThinkingAnimation();
    void onNewChatRequested();
    void onConversationSelected(const QString &id);
    void onConversationDeleteRequested(const QString &id);

private:
    void setupUi();
    void applyTheme();
    void refreshAttachmentBadges();
    void updateHeaderContextBar();
    void smoothScrollToBottom();
    QWidget* createAvatar(const QString &label, const QString &bgColor, QWidget *parent);
    void setGeneratingState(bool generating);
    void renderUserBubble(const QString &text);
    void renderAssistantBubble(const QString &text);
    void clearChatDisplay();
    QWidget* createFormattedBubble(const QString &text, QWidget *parent);
    void finalizeAssistantBubble();
    void saveCurrentConversation();
    void loadConversation(const QString &id);

    // Layout & containers
    QSplitter *m_mainSplitter{nullptr};
    QVBoxLayout *m_chatLayout{nullptr};
    QScrollArea *m_scrollArea{nullptr};
    QWidget *m_scrollContainer{nullptr};

    // Input area
    QTextEdit *m_inputEdit{nullptr};
    QPushButton *m_sendButton{nullptr};
    QPushButton *m_stopButton{nullptr};
    QPushButton *m_attachButton{nullptr};

    // Header
    QPushButton *m_loadModelButton{nullptr};
    QPushButton *m_settingsButton{nullptr};
    QPushButton *m_sidebarToggle{nullptr};
    QLabel *m_statusLabel{nullptr};
    QLabel *m_statusDot{nullptr};

    // Context bar
    QLabel *m_contextHeaderLabel{nullptr};
    QProgressBar *m_contextProgressBar{nullptr};

    // Attachments
    QWidget *m_attachmentFrame{nullptr};
    QHBoxLayout *m_attachmentPillsLayout{nullptr};
    QStringList m_attachedFilePaths;
    int m_maxContextTokens{0};

    // Backend
    LlamaClient *m_llamaClient{nullptr};
    LlamaManager *m_llamaManager{nullptr};
    ToolRegistry *m_toolRegistry{nullptr};
    WorkspaceIndexer *m_workspaceIndexer{nullptr};
    IdeWidget *m_ideWidget{nullptr};

    // Tool loop
    int m_toolLoopCount{0};
    static constexpr int MAX_TOOL_LOOPS = 5;

    // Chat state
    QList<ChatMessage> m_chatHistory;
    QLabel *m_currentAssistantBubble{nullptr};
    QString m_currentAssistantText;
    QWidget *m_currentAssistantRow{nullptr};

    // Animation & generation state
    QTimer *m_thinkingAnimTimer{nullptr};
    int m_thinkingDotCount{0};
    bool m_isGenerating{false};

    // Sidebar & conversations
    ConversationSidebar *m_sidebar{nullptr};
    QMap<QString, ConversationSession> m_conversations;
    QString m_activeConversationId;
};

#endif // CHATWIDGET_H
