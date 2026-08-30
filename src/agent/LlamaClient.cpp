#include "LlamaClient.h"
#include <QNetworkRequest>
#include <QUrl>
#include <QTimer>
#include <QRegularExpression>
#include <QDir>
#include <QDebug>

LlamaClient::LlamaClient(QObject *parent)
    : QObject(parent)
{
}

void LlamaClient::sendChatCompletion(const QList<ChatMessage> &messages, const QString &model) {
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    QJsonArray msgArray;

    m_accumulatedText.clear();

    // Prepend System Prompt instructing model to use tools and informing it of active workspace folder
    QString activeWs = m_workspacePath.isEmpty() ? QDir::homePath() : m_workspacePath;
    QString wsListing;
    QDir wsDir(activeWs);
    if (wsDir.exists()) {
        QFileInfoList entries = wsDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
        int maxEntries = qMin(entries.size(), 20);
        for (int i = 0; i < maxEntries; ++i) {
            const auto &info = entries[i];
            wsListing += (info.isDir() ? " - [DIR] " : " - [FILE] ") + info.fileName() + "\n";
        }
        if (entries.size() > 20) {
            wsListing += " - ... (" + QString::number(entries.size() - 20) + " more items)\n";
        }
    }

    QJsonObject sysObj;
    sysObj["role"] = "system";
    sysObj["content"] = "You are AgenticAI, a helpful AI assistant.\n"
                        "Active Workspace: '" + activeWs + "'\n"
                        "Files:\n" + (wsListing.isEmpty() ? " - (empty)\n" : wsListing) +
                        "\nRULES:\n"
                        "1. For greetings or chat ('hi','hello'), reply with plain text. NO tools.\n"
                        "2. To list files, use: {\"tool\": \"list_dir\", \"parameters\": {\"path\": \"" + activeWs + "\"}}\n"
                        "3. To read a file, use: {\"tool\": \"read_file\", \"parameters\": {\"path\": \"filename\"}}\n"
                        "4. NEVER use generate_docx or generate_pdf unless user asks to create a document.\n"
                        "5. All file paths must be inside the active workspace.";
    msgArray.append(sysObj);

    for (const auto &msg : messages) {
        QJsonObject obj;
        obj["role"] = msg.role;
        obj["content"] = msg.content;
        msgArray.append(obj);
    }

    QJsonObject payload;
    payload["model"] = model;
    payload["messages"] = msgArray;
    payload["stream"] = true;
    payload["temperature"] = 0.7;
    payload["repeat_penalty"] = 1.1;

    if (m_toolRegistry) {
        payload["tools"] = m_toolRegistry->getToolsJson();
    }

    QJsonDocument doc(payload);
    QByteArray body = doc.toJson(QJsonDocument::Compact);

    QNetworkRequest request((QUrl(m_serverUrl)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    m_buffer.clear();
    m_currentReply = m_networkManager.post(request, body);

    connect(m_currentReply, &QNetworkReply::readyRead, this, &LlamaClient::onReadyRead);
    connect(m_currentReply, &QNetworkReply::finished, this, &LlamaClient::onFinished);
    connect(m_currentReply, &QNetworkReply::errorOccurred, this, &LlamaClient::onError);
}

void LlamaClient::onReadyRead() {
    if (!m_currentReply) return;

    m_buffer.append(m_currentReply->readAll());

    while (m_buffer.contains('\n')) {
        int pos = m_buffer.indexOf('\n');
        QByteArray line = m_buffer.left(pos).trimmed();
        m_buffer.remove(0, pos + 1);

        if (line.startsWith("data: ")) {
            QByteArray dataJson = line.mid(6).trimmed();
            if (dataJson == "[DONE]") {
                emit completionFinished();
                return;
            }

            QJsonDocument doc = QJsonDocument::fromJson(dataJson);
            if (!doc.isNull() && doc.isObject()) {
                QJsonObject root = doc.object();
                QJsonArray choices = root["choices"].toArray();
                if (!choices.isEmpty()) {
                    QJsonObject choice = choices[0].toObject();
                    QJsonObject delta = choice["delta"].toObject();
                    if (delta.contains("content")) {
                        QString token = delta["content"].toString();
                        m_accumulatedText += token;
                        emit tokenReceived(token);

                        if (m_accumulatedText.endsWith("\n\n\n\n\n\n\n\n")) {
                            if (m_currentReply) {
                                m_currentReply->abort();
                            }
                            return;
                        }
                    }
                    if (delta.contains("tool_calls")) {
                        QJsonArray toolCalls = delta["tool_calls"].toArray();
                        for (const auto &tcVal : toolCalls) {
                            QJsonObject tc = tcVal.toObject();
                            QJsonObject func = tc["function"].toObject();
                            QString toolName = func["name"].toString();
                            QJsonDocument argsDoc = QJsonDocument::fromJson(func["arguments"].toString().toUtf8());
                            QJsonObject argsObj = argsDoc.object();

                            if (m_toolRegistry && !toolName.isEmpty()) {
                                QString result = m_toolRegistry->executeTool(toolName, argsObj);
                                emit toolCallDetected(toolName, result);
                            }
                        }
                    }
                }
            }
        }
    }
}

void LlamaClient::onFinished() {
    if (m_currentReply) {
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
    checkForTextToolCalls();
    emit completionFinished();
}

void LlamaClient::onError(QNetworkReply::NetworkError code) {
    if (code == QNetworkReply::OperationCanceledError) return;

    QString errStr = m_currentReply ? m_currentReply->errorString() : "Unknown Network Error";
    emit errorOccurred("LlamaServer error: " + errStr + " (Is llama-server running on port 8080?)");
}

void LlamaClient::fetchModelProperties() {
    QUrl propsUrl("http://127.0.0.1:8080/props");
    QNetworkRequest req(propsUrl);

    QNetworkReply *reply = m_networkManager.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                QJsonObject root = doc.object();
                int nCtx = 4096; // default fallback
                QString modelName = "GGUF Model";

                if (root.contains("default_generation_settings")) {
                    QJsonObject gen = root["default_generation_settings"].toObject();
                    if (gen.contains("n_ctx")) {
                        nCtx = gen["n_ctx"].toInt(4096);
                    }
                }
                if (root.contains("model_path")) {
                    modelName = root["model_path"].toString();
                }

                emit modelPropertiesLoaded(nCtx, modelName);
                return;
            }
        }

        // If llama-server is still initializing tensors, retry in 1 second
        QTimer::singleShot(1000, this, &LlamaClient::fetchModelProperties);
    });
}

void LlamaClient::checkForTextToolCalls() {
    if (!m_toolRegistry || m_accumulatedText.isEmpty()) return;

    // Known valid tool names - reject anything else from small models hallucinating
    static const QStringList validTools = {"read_file", "write_file", "list_dir", "run_command",
                                            "generate_pdf", "generate_docx", "generate_image",
                                            "open_ide", "OPEN_THE_IDE"};

    int len = m_accumulatedText.size();
    for (int i = 0; i < len; ++i) {
        if (m_accumulatedText[i] == '{') {
            int depth = 0;
            int start = i;
            for (int j = i; j < len; ++j) {
                if (m_accumulatedText[j] == '{') depth++;
                else if (m_accumulatedText[j] == '}') depth--;

                if (depth == 0) {
                    QString jsonStr = m_accumulatedText.mid(start, j - start + 1);
                    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
                    if (!doc.isNull() && doc.isObject()) {
                        QJsonObject obj = doc.object();
                        QString toolName;
                        QJsonObject argsObj;

                        if (obj.contains("tool")) {
                            toolName = obj["tool"].toString();
                            argsObj = obj.contains("parameters") ? obj["parameters"].toObject() : (obj.contains("arguments") ? obj["arguments"].toObject() : obj);
                        } else if (obj.contains("name")) {
                            toolName = obj["name"].toString();
                            argsObj = obj.contains("arguments") ? obj["arguments"].toObject() : obj;
                        }

                        if (!toolName.isEmpty() && validTools.contains(toolName, Qt::CaseInsensitive)) {
                            QString result = m_toolRegistry->executeTool(toolName, argsObj);
                            emit toolCallDetected(toolName, result);
                            return;
                        }
                    }
                    break;
                }
            }
        }
    }
}
