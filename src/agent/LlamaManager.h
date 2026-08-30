#ifndef LLAMAMANAGER_H
#define LLAMAMANAGER_H

#include <QObject>
#include <QProcess>

class LlamaManager : public QObject {
    Q_OBJECT

public:
    explicit LlamaManager(QObject *parent = nullptr);
    ~LlamaManager();

    bool isRunning() const;
    QString currentModelPath() const { return m_currentModelPath; }

public slots:
    void startServer(const QString &modelPath, int port = 8080, int nGpuLayers = 99);
    void stopServer();

signals:
    void serverStarted(int port);
    void serverStopped();
    void serverError(const QString &error);
    void logOutput(const QString &log);

private slots:
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();

private:
    QProcess *m_process{nullptr};
    QString m_currentModelPath;
    int m_port{8080};
};

#endif // LLAMAMANAGER_H
