#ifndef CONVERSATIONSIDEBAR_H
#define CONVERSATIONSIDEBAR_H

#include <QWidget>
#include <QVBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>

class ConversationSidebar : public QWidget {
    Q_OBJECT

public:
    explicit ConversationSidebar(QWidget *parent = nullptr);

    void addConversation(const QString &id, const QString &title);
    void setActiveConversation(const QString &id);
    void removeConversation(const QString &id);
    void clear();

signals:
    void newChatRequested();
    void conversationSelected(const QString &id);
    void conversationDeleteRequested(const QString &id);

private slots:
    void onItemClicked(QListWidgetItem *item);

private:
    void setupUi();

    QPushButton *m_newChatButton{nullptr};
    QListWidget *m_conversationList{nullptr};
    QString m_activeId;
};

#endif // CONVERSATIONSIDEBAR_H
