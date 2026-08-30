#ifndef DOCUMENTTOOLS_H
#define DOCUMENTTOOLS_H

#include <QString>

class DocumentTools {
public:
    static QString generatePdf(const QString &path, const QString &markdownContent);
    static QString generateDocx(const QString &path, const QString &markdownContent);
};

#endif // DOCUMENTTOOLS_H
