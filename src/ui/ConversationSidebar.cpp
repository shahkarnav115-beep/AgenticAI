#include "ConversationSidebar.h"
#include <QScrollBar>

ConversationSidebar::ConversationSidebar(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    setFixedWidth(260);
}

void ConversationSidebar::setupUi() {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    setStyleSheet("background-color: #0a0b12;");

    // Header with brand + New Chat button
    auto *headerWidget = new QWidget(this);
    headerWidget->setStyleSheet("background-color: #0f1018; border-bottom: 1px solid #1e2036; border-right: 1px solid #1a1c30;");
    auto *headerLayout = new QVBoxLayout(headerWidget);
    headerLayout->setContentsMargins(14, 14, 14, 14);
    headerLayout->setSpacing(14);

    // Brand row
    auto *brandLayout = new QHBoxLayout();
    brandLayout->setSpacing(8);
    auto *logoLabel = new QLabel("✦", headerWidget);
    logoLabel->setStyleSheet("color: #7aa2f7; font-size: 22px; font-weight: bold; background: transparent; border: none;");
    auto *brandLabel = new QLabel("AgenticAI", headerWidget);
    brandLabel->setStyleSheet("color: #c0caf5; font-size: 16px; font-weight: 700; background: transparent; border: none;");
    brandLayout->addWidget(logoLabel);
    brandLayout->addWidget(brandLabel);
    brandLayout->addStretch();
    headerLayout->addLayout(brandLayout);

    // New Chat button
    m_newChatButton = new QPushButton("+ New Chat", headerWidget);
    m_newChatButton->setCursor(Qt::PointingHandCursor);
    m_newChatButton->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 rgba(122, 162, 247, 0.15), stop:1 rgba(122, 162, 247, 0.08));
            color: #7aa2f7;
            font-weight: 600;
            border: 1px solid rgba(122, 162, 247, 0.25);
            border-radius: 8px;
            padding: 10px;
            font-size: 13px;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 rgba(122, 162, 247, 0.25), stop:1 rgba(122, 162, 247, 0.15));
            border: 1px solid rgba(122, 162, 247, 0.4);
        }
    )");
    connect(m_newChatButton, &QPushButton::clicked, this, &ConversationSidebar::newChatRequested);
    headerLayout->addWidget(m_newChatButton);

    layout->addWidget(headerWidget);

    // Section label
    auto *sectionLabel = new QLabel("  RECENT CHATS", this);
    sectionLabel->setFixedHeight(32);
    sectionLabel->setStyleSheet(
        "color: #565f89; font-size: 10px; font-weight: 700; letter-spacing: 1.5px;"
        "padding-left: 14px; padding-top: 10px; background: transparent; border-right: 1px solid #1a1c30;"
    );
    layout->addWidget(sectionLabel);

    // Conversation list
    m_conversationList = new QListWidget(this);
    m_conversationList->setStyleSheet(R"(
        QListWidget {
            background: transparent;
            border: none;
            border-right: 1px solid #1a1c30;
            outline: none;
            padding: 4px 6px;
        }
        QListWidget::item {
            background: transparent;
            color: #a6adc8;
            padding: 11px 12px;
            border-radius: 8px;
            margin: 1px 2px;
            font-size: 13px;
        }
        QListWidget::item:selected {
            background: rgba(122, 162, 247, 0.12);
            color: #7aa2f7;
            font-weight: 600;
        }
        QListWidget::item:hover:!selected {
            background: rgba(122, 162, 247, 0.06);
            color: #c0caf5;
        }
    )");
    connect(m_conversationList, &QListWidget::itemClicked, this, &ConversationSidebar::onItemClicked);
    layout->addWidget(m_conversationList, 1);
}

void ConversationSidebar::addConversation(const QString &id, const QString &title) {
    // Check if it already exists (update title)
    for (int i = 0; i < m_conversationList->count(); ++i) {
        auto *existing = m_conversationList->item(i);
        if (existing->data(Qt::UserRole).toString() == id) {
            QString displayTitle = title.left(32) + (title.length() > 32 ? "..." : "");
            existing->setText("💬 " + displayTitle);
            return;
        }
    }

    auto *item = new QListWidgetItem();
    QString displayTitle = title.left(32) + (title.length() > 32 ? "..." : "");
    item->setText("💬 " + displayTitle);
    item->setData(Qt::UserRole, id);
    m_conversationList->insertItem(0, item); // Newest at top
}

void ConversationSidebar::setActiveConversation(const QString &id) {
    m_activeId = id;
    for (int i = 0; i < m_conversationList->count(); ++i) {
        auto *item = m_conversationList->item(i);
        if (item->data(Qt::UserRole).toString() == id) {
            m_conversationList->setCurrentItem(item);
            return;
        }
    }
}

void ConversationSidebar::removeConversation(const QString &id) {
    for (int i = 0; i < m_conversationList->count(); ++i) {
        auto *item = m_conversationList->item(i);
        if (item->data(Qt::UserRole).toString() == id) {
            delete m_conversationList->takeItem(i);
            return;
        }
    }
}

void ConversationSidebar::clear() {
    m_conversationList->clear();
    m_activeId.clear();
}

void ConversationSidebar::onItemClicked(QListWidgetItem *item) {
    if (!item) return;
    QString id = item->data(Qt::UserRole).toString();
    m_activeId = id;
    emit conversationSelected(id);
}
