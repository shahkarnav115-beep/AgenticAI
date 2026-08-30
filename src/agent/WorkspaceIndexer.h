#ifndef WORKSPACEINDEXER_H
#define WORKSPACEINDEXER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QDateTime>
#include <QFileInfo>

struct FileEntry {
    QString relativePath;
    QString absolutePath;
    qint64 sizeBytes;
    QString extension;
    QDateTime lastModified;
    bool isDirectory;
};

class WorkspaceIndexer : public QObject {
    Q_OBJECT

public:
    explicit WorkspaceIndexer(QObject *parent = nullptr);
    ~WorkspaceIndexer() override = default;

    // Set the root workspace path and trigger a full re-index
    void setWorkspacePath(const QString &path);
    QString workspacePath() const;

    // Re-scan the workspace directory tree
    void reindex();

    // Get a compact tree representation of the entire project (like VS Code explorer)
    // maxLines caps output length for small context windows
    QString getProjectTree(int maxLines = 200) const;

    // Get a structured summary: language stats, file counts, key config files
    QString getProjectSummary() const;

    // Find files whose name or path matches keywords from a user query
    // Returns up to maxFiles results, sorted by relevance
    QStringList getRelevantFiles(const QString &query, int maxFiles = 10) const;

    // Read file contents with smart truncation (head + tail with "..." gap)
    QString getFileContents(const QString &relativePath, int maxChars = 3000) const;

    // Auto-detect and return contents of key project files (README, CMakeLists, package.json, etc.)
    // Respects a total character budget across all returned files
    QString getKeyFilesContext(int totalCharBudget = 4000) const;

    // Build a context block suitable for injection into the system prompt
    // Combines project tree + key files + relevant files for the given user query
    QString buildContextBlock(const QString &userQuery, int totalTokenBudget = 2048) const;

    // Total number of indexed files
    int fileCount() const;

    // Check if a path should be ignored
    static bool shouldIgnore(const QString &name, const QString &relativePath);

private:
    void scanDirectory(const QString &dirPath, const QString &prefix, int depth);

    QString m_workspacePath;
    QList<FileEntry> m_files;
    int m_maxDepth{12};

    // Default ignore patterns
    static const QStringList s_ignoredDirs;
    static const QStringList s_ignoredExtensions;
    static const QStringList s_keyFileNames;
};

#endif // WORKSPACEINDEXER_H
