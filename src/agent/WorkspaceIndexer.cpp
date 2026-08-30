#include "WorkspaceIndexer.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDirIterator>
#include <QRegularExpression>
#include <QDebug>
#include <algorithm>

// Directories to skip during indexing
const QStringList WorkspaceIndexer::s_ignoredDirs = {
    ".git", "node_modules", "build", "Build", "cmake-build-debug", "cmake-build-release",
    "__pycache__", ".vs", ".vscode", ".idea", "out", "dist", "target",
    ".gradle", ".cache", "vendor", "Pods", "DerivedData", ".next",
    "bin", "obj", "Debug", "Release", "x64", "x86", ".svn"
};

// Binary/large file extensions to skip content reading
const QStringList WorkspaceIndexer::s_ignoredExtensions = {
    "exe", "dll", "so", "dylib", "o", "obj", "a", "lib",
    "png", "jpg", "jpeg", "gif", "bmp", "ico", "svg", "webp",
    "mp3", "mp4", "wav", "avi", "mkv", "mov", "flac",
    "zip", "tar", "gz", "rar", "7z", "bz2", "xz",
    "pdf", "doc", "docx", "xls", "xlsx", "ppt", "pptx",
    "gguf", "bin", "dat", "db", "sqlite", "wasm",
    "ttf", "otf", "woff", "woff2", "eot",
    "pyc", "pyo", "class", "jar"
};

// Key project files to auto-detect and inject into context
const QStringList WorkspaceIndexer::s_keyFileNames = {
    "README.md", "README.txt", "README",
    "CMakeLists.txt", "Makefile", "meson.build",
    "package.json", "Cargo.toml", "go.mod", "build.gradle",
    "pyproject.toml", "setup.py", "requirements.txt",
    ".gitignore", "Dockerfile", "docker-compose.yml",
    "main.cpp", "main.c", "main.py", "main.go", "main.rs",
    "index.js", "index.ts", "app.py", "App.tsx", "App.jsx"
};

WorkspaceIndexer::WorkspaceIndexer(QObject *parent)
    : QObject(parent)
{
}

void WorkspaceIndexer::setWorkspacePath(const QString &path) {
    if (path.isEmpty() || !QDir(path).exists()) return;
    m_workspacePath = QDir::cleanPath(path);
    reindex();
}

QString WorkspaceIndexer::workspacePath() const {
    return m_workspacePath;
}

void WorkspaceIndexer::reindex() {
    m_files.clear();
    if (m_workspacePath.isEmpty()) return;
    scanDirectory(m_workspacePath, "", 0);
}

int WorkspaceIndexer::fileCount() const {
    return m_files.size();
}

bool WorkspaceIndexer::shouldIgnore(const QString &name, const QString &relativePath) {
    Q_UNUSED(relativePath);
    if (name.startsWith(".") && name != ".gitignore" && name != ".env") {
        // Allow .gitignore and .env, skip other dotfiles/dirs
        if (s_ignoredDirs.contains(name, Qt::CaseInsensitive)) return true;
    }
    if (s_ignoredDirs.contains(name, Qt::CaseInsensitive)) return true;
    return false;
}

void WorkspaceIndexer::scanDirectory(const QString &dirPath, const QString &prefix, int depth) {
    if (depth > m_maxDepth) return;

    QDir dir(dirPath);
    QFileInfoList entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries, QDir::DirsFirst | QDir::Name);

    for (const QFileInfo &info : entries) {
        QString name = info.fileName();
        QString relPath = prefix.isEmpty() ? name : (prefix + "/" + name);

        if (info.isDir()) {
            if (shouldIgnore(name, relPath)) continue;

            FileEntry entry;
            entry.relativePath = relPath;
            entry.absolutePath = info.absoluteFilePath();
            entry.sizeBytes = 0;
            entry.extension = "";
            entry.lastModified = info.lastModified();
            entry.isDirectory = true;
            m_files.append(entry);

            scanDirectory(info.absoluteFilePath(), relPath, depth + 1);
        } else {
            QString ext = info.suffix().toLower();
            if (s_ignoredExtensions.contains(ext)) continue;

            FileEntry entry;
            entry.relativePath = relPath;
            entry.absolutePath = info.absoluteFilePath();
            entry.sizeBytes = info.size();
            entry.extension = ext;
            entry.lastModified = info.lastModified();
            entry.isDirectory = false;
            m_files.append(entry);
        }
    }
}

QString WorkspaceIndexer::getProjectTree(int maxLines) const {
    if (m_files.isEmpty()) return "(empty workspace)\n";

    QString tree;
    int lineCount = 0;
    QFileInfo wsInfo(m_workspacePath);
    tree += wsInfo.fileName() + "/\n";
    lineCount++;

    for (const FileEntry &entry : m_files) {
        if (lineCount >= maxLines) {
            tree += "  ... (" + QString::number(m_files.size() - lineCount) + " more items)\n";
            break;
        }

        // Calculate indent level from path separators
        int depth = entry.relativePath.count('/');
        QString indent;
        for (int i = 0; i < depth + 1; ++i) indent += "  ";

        // Extract just the filename from relative path
        QString name = entry.relativePath;
        int lastSlash = name.lastIndexOf('/');
        if (lastSlash >= 0) name = name.mid(lastSlash + 1);

        if (entry.isDirectory) {
            tree += indent + name + "/\n";
        } else {
            // Show file size for context
            QString sizeStr;
            if (entry.sizeBytes < 1024) {
                sizeStr = QString::number(entry.sizeBytes) + "B";
            } else if (entry.sizeBytes < 1024 * 1024) {
                sizeStr = QString::number(entry.sizeBytes / 1024.0, 'f', 1) + "KB";
            } else {
                sizeStr = QString::number(entry.sizeBytes / (1024.0 * 1024.0), 'f', 1) + "MB";
            }
            tree += indent + name + " (" + sizeStr + ")\n";
        }
        lineCount++;
    }

    return tree;
}

QString WorkspaceIndexer::getProjectSummary() const {
    if (m_files.isEmpty()) return "Empty workspace.\n";

    int totalFiles = 0;
    int totalDirs = 0;
    qint64 totalSize = 0;
    QMap<QString, int> extCounts;

    for (const FileEntry &entry : m_files) {
        if (entry.isDirectory) {
            totalDirs++;
        } else {
            totalFiles++;
            totalSize += entry.sizeBytes;
            QString ext = entry.extension.isEmpty() ? "(no ext)" : ("." + entry.extension);
            extCounts[ext]++;
        }
    }

    QString summary;
    summary += "Project: " + QFileInfo(m_workspacePath).fileName() + "\n";
    summary += "Files: " + QString::number(totalFiles) + " | Directories: " + QString::number(totalDirs) + "\n";

    if (totalSize < 1024 * 1024) {
        summary += "Total Size: " + QString::number(totalSize / 1024.0, 'f', 1) + " KB\n";
    } else {
        summary += "Total Size: " + QString::number(totalSize / (1024.0 * 1024.0), 'f', 1) + " MB\n";
    }

    // Top languages / file types
    QList<QPair<QString, int>> sorted;
    for (auto it = extCounts.constBegin(); it != extCounts.constEnd(); ++it) {
        sorted.append({it.key(), it.value()});
    }
    std::sort(sorted.begin(), sorted.end(), [](const QPair<QString, int> &a, const QPair<QString, int> &b) {
        return a.second > b.second;
    });

    summary += "File Types: ";
    int shown = 0;
    for (const auto &pair : sorted) {
        if (shown >= 8) {
            summary += "...";
            break;
        }
        if (shown > 0) summary += ", ";
        summary += pair.first + "(" + QString::number(pair.second) + ")";
        shown++;
    }
    summary += "\n";

    return summary;
}

QStringList WorkspaceIndexer::getRelevantFiles(const QString &query, int maxFiles) const {
    if (query.isEmpty() || m_files.isEmpty()) return {};

    // Tokenize query into search keywords (lowercase)
    QStringList keywords;
    QString normalized = query.toLower();
    // Remove common stop words and split
    normalized.remove(QRegularExpression("[^a-z0-9_./\\-]"));
    QStringList parts = normalized.split(QRegularExpression("[\\s,;]+"), Qt::SkipEmptyParts);
    for (const QString &p : parts) {
        if (p.length() >= 2) keywords.append(p);
    }

    if (keywords.isEmpty()) return {};

    // Score each file by keyword matches in path/name
    QList<QPair<int, QString>> scored;
    for (const FileEntry &entry : m_files) {
        if (entry.isDirectory) continue;

        QString lowerPath = entry.relativePath.toLower();
        int score = 0;

        for (const QString &kw : keywords) {
            if (lowerPath.contains(kw)) {
                score += 10;
                // Bonus for filename match (not just path)
                QString fname = lowerPath.mid(lowerPath.lastIndexOf('/') + 1);
                if (fname.contains(kw)) score += 5;
            }
        }

        // Bonus for key config files
        QString baseName = entry.relativePath.mid(entry.relativePath.lastIndexOf('/') + 1);
        if (s_keyFileNames.contains(baseName, Qt::CaseInsensitive)) {
            score += 3;
        }

        if (score > 0) {
            scored.append({score, entry.relativePath});
        }
    }

    // Sort descending by score
    std::sort(scored.begin(), scored.end(), [](const QPair<int, QString> &a, const QPair<int, QString> &b) {
        return a.first > b.first;
    });

    QStringList results;
    for (int i = 0; i < qMin(maxFiles, scored.size()); ++i) {
        results.append(scored[i].second);
    }
    return results;
}

QString WorkspaceIndexer::getFileContents(const QString &relativePath, int maxChars) const {
    QString absPath = QDir(m_workspacePath).filePath(relativePath);
    QFile file(absPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return "";
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    if (content.size() <= maxChars) return content;

    // Smart truncation: show head and tail with gap
    int headSize = maxChars * 2 / 3;
    int tailSize = maxChars - headSize - 50; // Reserve space for the gap message
    if (tailSize < 100) tailSize = 100;

    QString head = content.left(headSize);
    QString tail = content.right(tailSize);
    int skippedLines = content.mid(headSize, content.size() - headSize - tailSize).count('\n');

    return head + "\n\n... [" + QString::number(skippedLines) + " lines omitted] ...\n\n" + tail;
}

QString WorkspaceIndexer::getKeyFilesContext(int totalCharBudget) const {
    if (m_files.isEmpty()) return "";

    // Find key files that exist in this project
    QStringList found;
    for (const FileEntry &entry : m_files) {
        if (entry.isDirectory) continue;
        QString baseName = entry.relativePath.mid(entry.relativePath.lastIndexOf('/') + 1);
        if (s_keyFileNames.contains(baseName, Qt::CaseInsensitive)) {
            found.append(entry.relativePath);
        }
    }

    if (found.isEmpty()) return "";

    QString context;
    int remaining = totalCharBudget;

    for (const QString &relPath : found) {
        if (remaining <= 200) break; // Not enough budget for another file

        int perFileBudget = qMin(remaining, totalCharBudget / qMax(1, found.size()));
        QString contents = getFileContents(relPath, perFileBudget);
        if (contents.isEmpty()) continue;

        QString block = "--- " + relPath + " ---\n" + contents + "\n\n";
        context += block;
        remaining -= block.size();
    }

    return context;
}

QString WorkspaceIndexer::buildContextBlock(const QString &userQuery, int totalTokenBudget) const {
    if (m_workspacePath.isEmpty() || m_files.isEmpty()) return "";

    // Approximate: 1 token ≈ 4 chars
    int totalCharBudget = totalTokenBudget * 4;

    // Budget allocation:
    // - 15% for project summary + tree
    // - 45% for key project files
    // - 40% for query-relevant files
    int treeBudget = totalCharBudget * 15 / 100;
    int keyFilesBudget = totalCharBudget * 45 / 100;
    int relevantBudget = totalCharBudget * 40 / 100;

    QString context;

    // 1. Project summary
    context += "=== PROJECT OVERVIEW ===\n";
    context += getProjectSummary();
    context += "\n";

    // 2. Project tree (compact)
    int maxTreeLines = treeBudget / 40; // ~40 chars per line average
    QString tree = getProjectTree(qMax(30, maxTreeLines));
    context += "=== FILE TREE ===\n";
    context += tree;
    context += "\n";

    // 3. Key config/entry files
    QString keyFiles = getKeyFilesContext(keyFilesBudget);
    if (!keyFiles.isEmpty()) {
        context += "=== KEY FILES (auto-detected) ===\n";
        context += keyFiles;
    }

    // 4. Query-relevant files
    if (!userQuery.isEmpty()) {
        QStringList relevant = getRelevantFiles(userQuery, 5);
        if (!relevant.isEmpty()) {
            context += "=== FILES RELEVANT TO YOUR QUERY ===\n";
            int perFileChars = relevantBudget / qMax(1, relevant.size());
            for (const QString &relPath : relevant) {
                QString contents = getFileContents(relPath, perFileChars);
                if (!contents.isEmpty()) {
                    context += "--- " + relPath + " ---\n" + contents + "\n\n";
                }
            }
        }
    }

    return context;
}
