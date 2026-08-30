#include "DocumentTools.h"
#include <QTextDocument>
#include <QPdfWriter>
#include <QPainter>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>
#include <QPageSize>
#include <QPageLayout>

QString DocumentTools::generatePdf(const QString &path, const QString &markdownContent) {
    QFileInfo fi(path);
    QDir dir = fi.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QPdfWriter pdfWriter(path);
    pdfWriter.setPageSize(QPageSize(QPageSize::A4));
    pdfWriter.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);

    QTextDocument doc;
    doc.setDefaultStyleSheet(R"(
        body { font-family: 'Segoe UI', Helvetica, Arial, sans-serif; font-size: 11pt; color: #1a1a1a; line-height: 1.6; }
        h1 { font-size: 22pt; color: #1a365d; border-bottom: 2px solid #2b6cb0; padding-bottom: 6px; }
        h2 { font-size: 16pt; color: #2b6cb0; margin-top: 16px; }
        h3 { font-size: 13pt; color: #2d3748; }
        code { font-family: 'Consolas', monospace; background-color: #edf2f7; padding: 2px 4px; border-radius: 4px; }
        pre { background-color: #f7fafc; border: 1px solid #e2e8f0; border-left: 4px solid #3182ce; padding: 10px; font-family: 'Consolas', monospace; }
        table { border-collapse: collapse; width: 100%; margin: 12px 0; }
        th, td { border: 1px solid #cbd5e0; padding: 8px 12px; text-align: left; }
        th { background-color: #ebf8ff; color: #2b6cb0; font-weight: bold; }
        blockquote { border-left: 4px solid #cbd5e0; padding-left: 12px; color: #4a5568; font-style: italic; }
    )");
    doc.setMarkdown(markdownContent);
    doc.print(&pdfWriter);

    return "Successfully generated PDF document: '" + fi.absoluteFilePath() + "'";
}

QString DocumentTools::generateDocx(const QString &path, const QString &markdownContent) {
    QFileInfo fi(path);
    QDir dir = fi.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QTextDocument doc;
    doc.setMarkdown(markdownContent);
    QString htmlContent = doc.toHtml();

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return "Error: Could not open file '" + path + "' for writing.";
    }

    QTextStream out(&file);
    out << "<html xmlns:o='urn:schemas-microsoft-com:office:office' "
        << "xmlns:w='urn:schemas-microsoft-com:office:word' "
        << "xmlns='http://www.w3.org/TR/REC-html40'>\n"
        << "<head><meta charset='utf-8'><title>Document</title>\n"
        << "<style>\n"
        << "body { font-family: 'Calibri', 'Segoe UI', sans-serif; font-size: 11pt; line-height: 1.5; margin: 1in; }\n"
        << "h1 { color: #1f4e78; font-size: 20pt; border-bottom: 1px solid #1f4e78; padding-bottom: 4px; }\n"
        << "h2 { color: #2e75b6; font-size: 15pt; }\n"
        << "code { font-family: 'Consolas', monospace; background-color: #f2f2f2; padding: 2px 4px; }\n"
        << "pre { background-color: #f2f2f2; padding: 10px; border-left: 3px solid #1f4e78; font-family: 'Consolas', monospace; }\n"
        << "table { border-collapse: collapse; width: 100%; margin: 10px 0; }\n"
        << "th, td { border: 1px solid #d9d9d9; padding: 6px 10px; text-align: left; }\n"
        << "th { background-color: #f2f2f2; font-weight: bold; }\n"
        << "</style></head>\n"
        << "<body>\n"
        << htmlContent
        << "\n</body></html>";

    file.close();
    return "Successfully generated DOCX document: '" + fi.absoluteFilePath() + "'";
}
