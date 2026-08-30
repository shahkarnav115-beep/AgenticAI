#include "ToolRegistry.h"
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QProcess>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>

ToolRegistry::ToolRegistry(QObject *parent)
    : QObject(parent),
      m_workspacePath(QDir::currentPath())
{
    registerDefaultTools();
}

void ToolRegistry::setWorkspacePath(const QString &path) {
    if (!path.isEmpty() && QDir(path).exists()) {
        m_workspacePath = path;
    }
}

QString ToolRegistry::workspacePath() const {
    return m_workspacePath;
}

void ToolRegistry::registerDefaultTools() {
    // 1. read_file
    {
        ToolDefinition def;
        def.name = "read_file";
        def.description = "Reads the text contents of a file at the given relative or absolute path.";
        QJsonObject props;
        QJsonObject pathObj;
        pathObj["type"] = "string";
        pathObj["description"] = "Path to the file to read.";
        props["path"] = pathObj;

        QJsonObject schema;
        schema["type"] = "object";
        schema["properties"] = props;
        QJsonArray req;
        req.append("path");
        schema["required"] = req;
        def.parametersSchema = schema;

        m_tools.append(def);
    }

    // 2. write_file
    {
        ToolDefinition def;
        def.name = "write_file";
        def.description = "Writes text content to a file at the given path, creating parent directories if needed.";
        QJsonObject props;
        QJsonObject pathObj;
        pathObj["type"] = "string";
        pathObj["description"] = "Target file path.";
        props["path"] = pathObj;

        QJsonObject contentObj;
        contentObj["type"] = "string";
        contentObj["description"] = "Full code/text content to write to the file.";
        props["content"] = contentObj;

        QJsonObject schema;
        schema["type"] = "object";
        schema["properties"] = props;
        QJsonArray req;
        req.append("path");
        req.append("content");
        schema["required"] = req;
        def.parametersSchema = schema;

        m_tools.append(def);
    }

    // 3. list_dir
    {
        ToolDefinition def;
        def.name = "list_dir";
        def.description = "Lists files and directories inside a specified folder path.";
        QJsonObject props;
        QJsonObject pathObj;
        pathObj["type"] = "string";
        pathObj["description"] = "Directory path to list (use '.' for current directory).";
        props["path"] = pathObj;

        QJsonObject schema;
        schema["type"] = "object";
        schema["properties"] = props;
        QJsonArray req;
        req.append("path");
        schema["required"] = req;
        def.parametersSchema = schema;

        m_tools.append(def);
    }

    // 4. run_command
    {
        ToolDefinition def;
        def.name = "run_command";
        def.description = "Executes a system shell command and returns the stdout and stderr output.";
        QJsonObject props;
        QJsonObject cmdObj;
        cmdObj["type"] = "string";
        cmdObj["description"] = "Shell command line string to run.";
        props["command"] = cmdObj;

        QJsonObject schema;
        schema["type"] = "object";
        schema["properties"] = props;
        QJsonArray req;
        req.append("command");
        schema["required"] = req;
        def.parametersSchema = schema;

        m_tools.append(def);
    }

    // 5. generate_pdf
    {
        ToolDefinition def;
        def.name = "generate_pdf";
        def.description = "Generates a styled PDF document file from markdown or HTML content at the target path.";
        QJsonObject props;
        QJsonObject pathObj;
        pathObj["type"] = "string";
        pathObj["description"] = "Target PDF file output path (e.g., 'Report.pdf').";
        props["path"] = pathObj;

        QJsonObject contentObj;
        contentObj["type"] = "string";
        contentObj["description"] = "Markdown or HTML text content of the document.";
        props["content"] = contentObj;

        QJsonObject schema;
        schema["type"] = "object";
        schema["properties"] = props;
        QJsonArray req;
        req.append("path");
        req.append("content");
        schema["required"] = req;
        def.parametersSchema = schema;

        m_tools.append(def);
    }

    // 6. generate_docx
    {
        ToolDefinition def;
        def.name = "generate_docx";
        def.description = "Generates a styled Microsoft Word DOCX document file at the target path.";
        QJsonObject props;
        QJsonObject pathObj;
        pathObj["type"] = "string";
        pathObj["description"] = "Target DOCX file output path (e.g., 'Specification.docx').";
        props["path"] = pathObj;

        QJsonObject contentObj;
        contentObj["type"] = "string";
        contentObj["description"] = "Markdown text content of the document.";
        props["content"] = contentObj;

        QJsonObject schema;
        schema["type"] = "object";
        schema["properties"] = props;
        QJsonArray req;
        req.append("path");
        req.append("content");
        schema["required"] = req;
        def.parametersSchema = schema;

        m_tools.append(def);
    }

    // 7. generate_image
    {
        ToolDefinition def;
        def.name = "generate_image";
        def.description = "Generates an AI image file or graphic illustration at the target path based on a prompt.";
        QJsonObject props;
        QJsonObject pathObj;
        pathObj["type"] = "string";
        pathObj["description"] = "Target image file output path (e.g., 'artwork.png').";
        props["path"] = pathObj;

        QJsonObject promptObj;
        promptObj["type"] = "string";
        promptObj["description"] = "Detailed visual text prompt describing the image to generate.";
        props["prompt"] = promptObj;

        QJsonObject schema;
        schema["type"] = "object";
        schema["properties"] = props;
        QJsonArray req;
        req.append("path");
        req.append("prompt");
        schema["required"] = req;
        def.parametersSchema = schema;

        m_tools.append(def);
    }

    // 8. open_ide (OPEN_THE_IDE)
    {
        ToolDefinition def;
        def.name = "open_ide";
        def.description = "Opens the integrated AgenticAI IDE code editor workspace window (triggered by prompt OPEN_THE_IDE).";
        QJsonObject schema;
        schema["type"] = "object";
        def.parametersSchema = schema;

        m_tools.append(def);
    }
}

QJsonArray ToolRegistry::getToolsJson() const {
    QJsonArray toolsArray;
    for (const auto &tool : m_tools) {
        QJsonObject funcObj;
        funcObj["name"] = tool.name;
        funcObj["description"] = tool.description;
        funcObj["parameters"] = tool.parametersSchema;

        QJsonObject toolObj;
        toolObj["type"] = "function";
        toolObj["function"] = funcObj;

        toolsArray.append(toolObj);
    }
    return toolsArray;
}

QString ToolRegistry::executeTool(const QString &toolName, const QJsonObject &args) {
    // Parameter normalization for flexible LLM parameter key variants
    QString path = args.contains("path") ? args["path"].toString()
                 : (args.contains("name") ? args["name"].toString()
                 : (args.contains("file") ? args["file"].toString()
                 : (args.contains("output_file") ? args["output_file"].toString()
                 : args["filename"].toString())));

    QString content = args.contains("content") ? args["content"].toString()
                    : (args.contains("data") ? args["data"].toString()
                    : (args.contains("text") ? args["text"].toString()
                    : args["markdown"].toString()));

    QString prompt = args.contains("prompt") ? args["prompt"].toString() : content;

    // Parameter validation guard to prevent invalid tool execution
    if ((toolName == "read_file" || toolName == "write_file" || toolName == "generate_pdf" ||
         toolName == "generate_docx" || toolName == "generate_image") && path.trimmed().isEmpty()) {
        return "Error: Missing required file path parameter.";
    }

    if (toolName == "read_file") {
        return toolReadFile(path);
    } else if (toolName == "write_file") {
        if (path.endsWith(".pdf", Qt::CaseInsensitive)) {
            return DocumentTools::generatePdf(path, content);
        } else if (path.endsWith(".docx", Qt::CaseInsensitive)) {
            return DocumentTools::generateDocx(path, content);
        } else if (path.endsWith(".png", Qt::CaseInsensitive) || path.endsWith(".jpg", Qt::CaseInsensitive)) {
            return ImageTools::generateImage(path, prompt.isEmpty() ? path : prompt);
        }
        return toolWriteFile(path, content);
    } else if (toolName == "list_dir") {
        return toolListDir(path);
    } else if (toolName == "run_command") {
        QString cmd = args.contains("command") ? args["command"].toString() : args["cmd"].toString();
        return toolRunCommand(cmd);
    } else if (toolName == "generate_pdf") {
        return DocumentTools::generatePdf(path, content);
    } else if (toolName == "generate_docx") {
        return DocumentTools::generateDocx(path, content);
    } else if (toolName == "generate_image") {
        return ImageTools::generateImage(path, prompt);
    } else if (toolName == "open_ide" || toolName == "OPEN_THE_IDE") {
        return "Opened AgenticAI IDE Studio window successfully.";
    }
    return "Error: Tool '" + toolName + "' not found.";
}

QString ToolRegistry::toolReadFile(const QString &path) {
    QFileInfo fi(path);
    QString targetPath;
    if (fi.isAbsolute()) {
        QString canonical = QDir::cleanPath(fi.absoluteFilePath());
        QString canonicalWs = QDir::cleanPath(m_workspacePath);
        if (!canonical.startsWith(canonicalWs, Qt::CaseInsensitive)) {
            return "Error: File '" + path + "' is outside the active workspace '" + m_workspacePath + "'. Use the IDE to switch folders.";
        }
        targetPath = canonical;
    } else {
        targetPath = QDir(m_workspacePath).filePath(path);
    }

    QFile file(targetPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return "Error: Could not open file '" + targetPath + "' for reading.";
    }
    QTextStream in(&file);
    QString content = in.readAll();
    if (content.size() > 4000) {
        content = content.left(4000) + "\n... (truncated, file too large)";
    }
    return content;
}

QString ToolRegistry::toolWriteFile(const QString &path, const QString &content) {
    QFileInfo fi(path);
    QString targetPath;
    if (fi.isAbsolute()) {
        QString canonical = QDir::cleanPath(fi.absoluteFilePath());
        QString canonicalWs = QDir::cleanPath(m_workspacePath);
        if (!canonical.startsWith(canonicalWs, Qt::CaseInsensitive)) {
            return "Error: Cannot write to '" + path + "' outside the active workspace '" + m_workspacePath + "'.";
        }
        targetPath = canonical;
    } else {
        targetPath = QDir(m_workspacePath).filePath(path);
    }

    QFileInfo targetFi(targetPath);
    QDir dir = targetFi.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return "Error: Could not open file '" + targetPath + "' for writing.";
    }
    QTextStream out(&file);
    out << content;
    return "Successfully wrote " + QString::number(content.size()) + " characters to '" + targetPath + "'.";
}

QString ToolRegistry::toolListDir(const QString &path) {
    QString targetPath;

    // Enforce active workspace path; if path is empty, '.' or outside m_workspacePath, force m_workspacePath
    if (path.isEmpty() || path == "." || path == "./") {
        targetPath = m_workspacePath;
    } else {
        QFileInfo fi(path);
        if (fi.isAbsolute()) {
            QString canonicalTarget = QDir::cleanPath(fi.absoluteFilePath());
            QString canonicalWs = QDir::cleanPath(m_workspacePath);
            if (!canonicalTarget.startsWith(canonicalWs, Qt::CaseInsensitive)) {
                targetPath = m_workspacePath;
            } else {
                targetPath = canonicalTarget;
            }
        } else {
            targetPath = QDir(m_workspacePath).filePath(path);
        }
    }

    QDir dir(targetPath);
    if (!dir.exists()) {
        targetPath = m_workspacePath;
        dir = QDir(targetPath);
    }

    QFileInfoList list = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    QString result = "Directory listing for '" + dir.absolutePath() + "':\n";
    for (const auto &info : list) {
        result += (info.isDir() ? "[DIR]  " : "[FILE] ") + info.fileName() + "\n";
    }
    return result;
}

QString ToolRegistry::toolRunCommand(const QString &command) {
    QProcess process;
#ifdef Q_OS_WIN
    process.start("cmd.exe", QStringList() << "/c" << command);
#else
    process.start("sh", QStringList() << "-c" << command);
#endif
    process.waitForFinished(10000); // 10s timeout

    QString out = QString::fromUtf8(process.readAllStandardOutput());
    QString err = QString::fromUtf8(process.readAllStandardError());

    QString result = "Exit Code: " + QString::number(process.exitCode()) + "\n";
    if (!out.isEmpty()) result += "STDOUT:\n" + out + "\n";
    if (!err.isEmpty()) result += "STDERR:\n" + err + "\n";
    return result;
}
