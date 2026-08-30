#ifndef LLAMACLIENT_H
#define LLAMACLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "ToolRegistry.h"

struct ChatMessage {
    QString role;
    QString content;
};

class LlamaClient : public QObject {
    Q_OBJECT

public:
    explicit LlamaClient(QObject *parent = nullptr);
    ~LlamaClient() = default;

    void setServerUrl(const QString &url) { m_serverUrl = url; }
    void setToolRegistry(ToolRegistry *registry) { m_toolRegistry = registry; }
    void setWorkspacePath(const QString &path) { m_workspacePath = path; }
    void sendChatCompletion(const QList<ChatMessage> &messages, const QString &model = "default");
    void fetchModelProperties();

signals:
    void tokenReceived(const QString &token);
    void toolCallDetected(const QString &toolName, const QString &result);
    void modelPropertiesLoaded(int maxContextTokens, const QString &modelName);
    void completionFinished();
    void errorOccurred(const QString &error);

private slots:
    void onReadyRead();
    void onFinished();
    void onError(QNetworkReply::NetworkError code);

private:
    void checkForTextToolCalls();

    QNetworkAccessManager m_networkManager;
    QNetworkReply *m_currentReply{nullptr};
    ToolRegistry *m_toolRegistry{nullptr};
    QString m_serverUrl{"http://127.0.0.1:8080/v1/chat/completions"};
    QString m_workspacePath;
    QByteArray m_buffer;
    QString m_accumulatedText;
};

#endif // LLAMACLIENT_H
