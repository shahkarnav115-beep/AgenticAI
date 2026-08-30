#include "LlamaManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

LlamaManager::LlamaManager(QObject *parent)
    : QObject(parent),
      m_process(new QProcess(this))
{
    connect(m_process, &QProcess::started, this, &LlamaManager::onProcessStarted);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &LlamaManager::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &LlamaManager::onProcessError);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &LlamaManager::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &LlamaManager::onReadyReadStandardError);
}

LlamaManager::~LlamaManager() {
    stopServer();
}

bool LlamaManager::isRunning() const {
    return m_process && m_process->state() == QProcess::Running;
}

void LlamaManager::startServer(const QString &modelPath, int port, int nGpuLayers) {
    if (isRunning()) {
        stopServer();
    }

    m_currentModelPath = modelPath;
    m_port = port;

    // Search for llama-server executable in app directory or build directories
    QString appDir = QCoreApplication::applicationDirPath();
    QString serverExe = appDir + "/llama-server.exe";

    if (!QFileInfo::exists(serverExe)) {
        serverExe = appDir + "/bin/llama-server.exe";
    }
    if (!QFileInfo::exists(serverExe)) {
        serverExe = appDir + "/bin/Release/llama-server.exe";
    }
    if (!QFileInfo::exists(serverExe)) {
        serverExe = appDir + "/../bin/Release/llama-server.exe";
    }
    if (!QFileInfo::exists(serverExe)) {
        serverExe = appDir + "/../bin/llama-server.exe";
    }
    if (!QFileInfo::exists(serverExe)) {
        serverExe = "llama-server"; // Fallback to system PATH
    }

    QStringList args;
    args << "-m" << modelPath
         << "--port" << QString::number(port)
         << "-ngl" << QString::number(nGpuLayers)
         << "-c" << "0"; // 0 = Auto-detect model's native context window from GGUF header

    m_process->start(serverExe, args);
}

void LlamaManager::stopServer() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(3000)) {
            m_process->kill();
        }
    }
}

void LlamaManager::onProcessStarted() {
    emit serverStarted(m_port);
}

void LlamaManager::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);
    emit serverStopped();
}

void LlamaManager::onProcessError(QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
        emit serverError("Failed to start llama-server. Ensure llama-server binary is present or in PATH.");
    } else {
        emit serverError("llama-server error occurred.");
    }
}

void LlamaManager::onReadyReadStandardOutput() {
    QString out = QString::fromUtf8(m_process->readAllStandardOutput());
    emit logOutput(out);
}

void LlamaManager::onReadyReadStandardError() {
    QString err = QString::fromUtf8(m_process->readAllStandardError());
    emit logOutput(err);
}
