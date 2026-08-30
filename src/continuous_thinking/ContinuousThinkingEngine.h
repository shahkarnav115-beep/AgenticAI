#ifndef CONTINUOUSTHINKINGENGINE_H
#define CONTINUOUSTHINKINGENGINE_H

#include <QObject>
#include <QString>

class ContinuousThinkingEngine : public QObject {
    Q_OBJECT

public:
    explicit ContinuousThinkingEngine(QObject *parent = nullptr);
    ~ContinuousThinkingEngine() override = default;

    bool isThinkingEnabled() const;
    void setThinkingEnabled(bool enabled);

    bool isCurrentlyThinking() const;
    int thoughtTokenCount() const;
    int currentLoopCount() const;
    int maxLoopLimit() const;

    void setMaxLoopLimit(int maxLoops);
    void resetSession();

    // Checks if the incoming task completion condition is met
    bool checkTaskCompletion(const QString &assistantResponse);

    // Processes an incoming token, updating thought states and separating thoughts from final output
    void appendToken(const QString &token, QString &outThoughtChunk, QString &outAnswerChunk);

    // Helper static method to clean raw text output by stripping tool JSON blocks and think tags
    static QString cleanResponseText(const QString &rawText);

signals:
    void thinkingStarted();
    void thinkingFinished(int totalThoughtTokens);
    void thoughtChunkReceived(const QString &chunk);
    void loopCompleted(int loopNumber);
    void taskFullyCompleted();

private:
    bool m_thinkingEnabled{true};
    bool m_isThinking{false};
    int m_thoughtTokenCount{0};
    int m_currentLoopCount{0};
    int m_maxLoopLimit{10};
    QString m_buffer;
};

#endif // CONTINUOUSTHINKINGENGINE_H
