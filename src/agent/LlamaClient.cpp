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

    // Extract the latest user message for relevance-based context injection
    QString latestUserQuery;
    for (int i = messages.size() - 1; i >= 0; --i) {
        if (messages[i].role == "user") {
            latestUserQuery = messages[i].content;
            break;
        }
    }

    // Build enhanced system prompt with full project context
    QJsonObject sysObj;
    sysObj["role"] = "system";
    sysObj["content"] = buildSystemPrompt(latestUserQuery);
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

QString LlamaClient::buildSystemPrompt(const QString &userQuery) const {
    QString activeWs = m_workspacePath.isEmpty() ? QDir::homePath() : m_workspacePath;

    // --- Workspace Context Block ---
    // If WorkspaceIndexer is available, use it for rich project context
    // Otherwise fall back to a simple root listing
    QString projectContext;
    if (m_workspaceIndexer && m_workspaceIndexer->fileCount() > 0) {
        // Dynamic token budget: use ~30% of available space for project context
        // For small models (2K-4K context), this keeps it tight
        // For larger models, we inject more
        int contextBudget = 1024; // Default for small models
        projectContext = m_workspaceIndexer->buildContextBlock(userQuery, contextBudget);
    } else {
        // Fallback: simple root listing (original behavior)
        QDir wsDir(activeWs);
        if (wsDir.exists()) {
            QFileInfoList entries = wsDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
            int maxEntries = qMin(entries.size(), 20);
            projectContext += "=== ROOT FILES ===\n";
            for (int i = 0; i < maxEntries; ++i) {
                const auto &info = entries[i];
                projectContext += (info.isDir() ? " - [DIR] " : " - [FILE] ") + info.fileName() + "\n";
            }
            if (entries.size() > 20) {
                projectContext += " - ... (" + QString::number(entries.size() - 20) + " more items)\n";
            }
        }
    }

    // --- Build the system prompt ---
    QString prompt;
    prompt += "You are AgenticAI, a helpful AI coding assistant with full access to the user's project workspace.\n\n";

    prompt += "ACTIVE WORKSPACE: '" + activeWs + "'\n\n";

    // Inject project context
    if (!projectContext.isEmpty()) {
        prompt += projectContext + "\n";
    }

    // --- Tool instructions ---
    // Dual format: XML tags for small models, JSON for larger models
    prompt += "=== AVAILABLE TOOLS ===\n"
              "You can use tools to interact with the workspace. Use EXACTLY one of these formats:\n\n"
              "FORMAT A (preferred, simpler):\n"
              "<tool>read_file</tool><arg>relative/path/to/file</arg>\n"
              "<tool>list_dir</tool><arg>relative/path/to/dir</arg>\n"
              "<tool>write_file</tool><arg>relative/path/to/file</arg><content>file content here</content>\n"
              "<tool>run_command</tool><arg>command string</arg>\n"
              "<tool>generate_pdf</tool><arg>output.pdf</arg><content>markdown content</content>\n"
              "<tool>generate_docx</tool><arg>output.docx</arg><content>markdown content</content>\n"
              "<tool>generate_image</tool><arg>output.png</arg><content>image description prompt</content>\n"
              "<tool>open_ide</tool>\n\n"
              "FORMAT B (JSON, also accepted):\n"
              "{\"tool\": \"read_file\", \"parameters\": {\"path\": \"relative/path/to/file\"}}\n\n";

    // --- Rules ---
    prompt += "RULES:\n"
              "1. For simple greetings or chat ('hi','hello'), reply with plain text. Do NOT use tools.\n"
              "2. When asked about files, code, or the project, USE THE PROJECT CONTEXT ABOVE FIRST before calling tools.\n"
              "3. Only use tools when you need information NOT already provided in the context above.\n"
              "4. All file paths must be relative to the active workspace unless absolute.\n"
              "5. NEVER use generate_pdf, generate_docx, or generate_image unless the user explicitly asks to create a document or image.\n"
              "6. When using a tool, output ONLY the tool call with no extra text before or after.\n"
              "7. After receiving tool results, provide a clear answer based on those results.\n";

    return prompt;
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
    // Check both JSON and XML tool call formats
    checkForTextToolCalls();
    checkForXmlToolCalls();
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

void LlamaClient::checkForXmlToolCalls() {
    if (!m_toolRegistry || m_accumulatedText.isEmpty()) return;

    // Known valid tool names
    static const QStringList validTools = {"read_file", "write_file", "list_dir", "run_command",
                                            "generate_pdf", "generate_docx", "generate_image",
                                            "open_ide", "OPEN_THE_IDE"};

    // Parse XML-tag format: <tool>name</tool><arg>value</arg>[<content>...</content>]
    static QRegularExpression toolRegex(
        "<tool>\\s*([^<]+?)\\s*</tool>"           // Capture tool name
        "(?:\\s*<arg>\\s*([^<]*?)\\s*</arg>)?"     // Optional: capture arg
        "(?:\\s*<content>([\\s\\S]*?)</content>)?", // Optional: capture content
        QRegularExpression::DotMatchesEverythingOption
    );

    QRegularExpressionMatchIterator it = toolRegex.globalMatch(m_accumulatedText);
    if (!it.hasNext()) return;

    QRegularExpressionMatch match = it.next();
    QString toolName = match.captured(1).trimmed();
    QString arg = match.captured(2).trimmed();
    QString content = match.captured(3); // Preserve whitespace in content

    if (toolName.isEmpty() || !validTools.contains(toolName, Qt::CaseInsensitive)) return;

    // Build args object compatible with ToolRegistry::executeTool
    QJsonObject argsObj;

    if (toolName == "run_command") {
        argsObj["command"] = arg;
    } else if (toolName == "open_ide" || toolName == "OPEN_THE_IDE") {
        // No args needed
    } else {
        argsObj["path"] = arg;
        if (!content.isNull() && !content.isEmpty()) {
            argsObj["content"] = content;
        }
        // For generate_image, content is the prompt
        if (toolName == "generate_image" && !content.isEmpty()) {
            argsObj["prompt"] = content;
        }
    }

    QString result = m_toolRegistry->executeTool(toolName, argsObj);
    emit toolCallDetected(toolName, result);
}
