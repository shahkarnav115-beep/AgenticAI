#include "ContinuousThinkingEngine.h"
#include <QRegularExpression>

ContinuousThinkingEngine::ContinuousThinkingEngine(QObject *parent)
    : QObject(parent)
{
}

bool ContinuousThinkingEngine::isThinkingEnabled() const {
    return m_thinkingEnabled;
}

void ContinuousThinkingEngine::setThinkingEnabled(bool enabled) {
    m_thinkingEnabled = enabled;
}

bool ContinuousThinkingEngine::isCurrentlyThinking() const {
    return m_isThinking;
}

int ContinuousThinkingEngine::thoughtTokenCount() const {
    return m_thoughtTokenCount;
}

int ContinuousThinkingEngine::currentLoopCount() const {
    return m_currentLoopCount;
}

int ContinuousThinkingEngine::maxLoopLimit() const {
    return m_maxLoopLimit;
}

void ContinuousThinkingEngine::setMaxLoopLimit(int maxLoops) {
    m_maxLoopLimit = maxLoops;
}

void ContinuousThinkingEngine::resetSession() {
    m_isThinking = false;
    m_thoughtTokenCount = 0;
    m_currentLoopCount = 0;
    m_buffer.clear();
}

bool ContinuousThinkingEngine::checkTaskCompletion(const QString &assistantResponse) {
    m_currentLoopCount++;
    emit loopCompleted(m_currentLoopCount);

    if (assistantResponse.contains("TASK_COMPLETED", Qt::CaseInsensitive) ||
        assistantResponse.contains("Goal Achieved", Qt::CaseInsensitive) ||
        m_currentLoopCount >= m_maxLoopLimit) {
        emit taskFullyCompleted();
        return true;
    }
    return false;
}

QString ContinuousThinkingEngine::cleanResponseText(const QString &rawText) {
    QString cleaned = rawText;

    // Strip <think>...</think> blocks and unclosed <think>
    static QRegularExpression thinkRegex("<think>[\\s\\S]*?(</think>|$)", QRegularExpression::DotMatchesEverythingOption);
    cleaned.remove(thinkRegex);
    cleaned.remove("<think>");
    cleaned.remove("</think>");

    // Strip completed or streaming JSON tool blocks: {"tool": ...} or {"name": ...}
    static QRegularExpression toolJsonRegex("\\{[\\s\\r\\n]*\"(tool|name)\"[\\s\\S]*", QRegularExpression::DotMatchesEverythingOption);
    cleaned.remove(toolJsonRegex);

    // Strip markdown code block wrappers around tool JSON: ```json ... ```
    static QRegularExpression codeBlockJsonRegex("```json[\\s\\S]*", QRegularExpression::DotMatchesEverythingOption);
    cleaned.remove(codeBlockJsonRegex);

    // Strip XML-format tool calls: <tool>...</tool><arg>...</arg>[<content>...</content>]
    static QRegularExpression xmlToolRegex(
        "<tool>[\\s\\S]*?(</content>|</arg>|</tool>|$)",
        QRegularExpression::DotMatchesEverythingOption
    );
    cleaned.remove(xmlToolRegex);

    // Clean up any orphaned XML tags from partial streaming
    cleaned.remove(QRegularExpression("<tool>[^<]*$"));
    cleaned.remove(QRegularExpression("<arg>[^<]*$"));
    cleaned.remove(QRegularExpression("<content>[\\s\\S]*$"));

    return cleaned.trimmed();
}

void ContinuousThinkingEngine::appendToken(const QString &token, QString &outThoughtChunk, QString &outAnswerChunk) {
    outThoughtChunk.clear();
    outAnswerChunk.clear();

    if (!m_thinkingEnabled) {
        outAnswerChunk = token;
        return;
    }

    m_buffer += token;

    // Detect <think> start tag
    if (!m_isThinking && m_buffer.contains("<think>")) {
        m_isThinking = true;
        emit thinkingStarted();
        int idx = m_buffer.indexOf("<think>");
        outAnswerChunk = m_buffer.left(idx);
        m_buffer.remove(0, idx + 7); // Length of "<think>"
    }

    // Detect </think> end tag
    if (m_isThinking) {
        if (m_buffer.contains("</think>")) {
            int endIdx = m_buffer.indexOf("</think>");
            outThoughtChunk = m_buffer.left(endIdx);
            m_thoughtTokenCount += outThoughtChunk.length() / 4; // Approx token count
            m_isThinking = false;
            emit thoughtChunkReceived(outThoughtChunk);
            emit thinkingFinished(m_thoughtTokenCount);

            m_buffer.remove(0, endIdx + 8); // Length of "</think>"
            outAnswerChunk = m_buffer;
            m_buffer.clear();
        } else {
            outThoughtChunk = m_buffer;
            m_thoughtTokenCount += outThoughtChunk.length() / 4;
            emit thoughtChunkReceived(outThoughtChunk);
            m_buffer.clear();
        }
    } else {
        outAnswerChunk = m_buffer;
        m_buffer.clear();
    }
}
