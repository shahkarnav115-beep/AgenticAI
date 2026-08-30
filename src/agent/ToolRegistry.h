#ifndef TOOLREGISTRY_H
#define TOOLREGISTRY_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>

#include "tools/DocumentTools.h"
#include "tools/ImageTools.h"

struct ToolDefinition {
    QString name;
    QString description;
    QJsonObject parametersSchema;
};

class ToolRegistry : public QObject {
    Q_OBJECT

public:
    explicit ToolRegistry(QObject *parent = nullptr);
    ~ToolRegistry() = default;

    QJsonArray getToolsJson() const;
    QString executeTool(const QString &toolName, const QJsonObject &args);

    void setWorkspacePath(const QString &path);
    QString workspacePath() const;

private:
    void registerDefaultTools();

    // Native C++ Tool Implementations
    QString toolReadFile(const QString &path);
    QString toolWriteFile(const QString &path, const QString &content);
    QString toolListDir(const QString &path);
    QString toolRunCommand(const QString &command);

    QList<ToolDefinition> m_tools;
    QString m_workspacePath;
};

#endif // TOOLREGISTRY_H
