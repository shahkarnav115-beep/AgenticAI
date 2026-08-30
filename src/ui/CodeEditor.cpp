#include "CodeEditor.h"
#include <QPainter>
#include <QTextBlock>
#include <QFontDatabase>

// ============================================================================
// Constructor
// ============================================================================

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    m_lineNumberArea = new LineNumberArea(this);

    // Use a premium monospace font
    QFont editorFont;
    QStringList preferredFonts = {"JetBrains Mono", "Cascadia Code", "Fira Code", "Consolas", "Courier New"};
    for (const QString &fontName : preferredFonts) {
        if (QFontDatabase::hasFamily(fontName)) {
            editorFont = QFont(fontName, 11);
            break;
        }
    }
    if (editorFont.family().isEmpty()) {
        editorFont = QFont("Consolas", 11);
    }
    editorFont.setStyleHint(QFont::Monospace);
    editorFont.setFixedPitch(true);
    setFont(editorFont);

    // Tab width: 4 spaces
    QFontMetrics metrics(editorFont);
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    setTabStopDistance(metrics.horizontalAdvance(' ') * 4);
#else
    setTabStopWidth(metrics.width(' ') * 4);
#endif

    // Connections
    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);
    connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::emitCursorInfo);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
}

// ============================================================================
// Line Number Area — Width Calculation
// ============================================================================

int CodeEditor::lineNumberAreaWidth() const {
    int digits = 1;
    int maxBlock = qMax(1, blockCount());
    while (maxBlock >= 10) {
        maxBlock /= 10;
        ++digits;
    }
    // Minimum 3 digits width for visual consistency
    digits = qMax(digits, 3);

    QFontMetrics fm(font());
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    int charWidth = fm.horizontalAdvance(QLatin1Char('9'));
#else
    int charWidth = fm.width(QLatin1Char('9'));
#endif

    // padding: 12px left + digits + 12px right
    return 12 + charWidth * digits + 12;
}

void CodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */) {
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

// ============================================================================
// Line Number Area — Paint Event
// ============================================================================

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event) {
    QPainter painter(m_lineNumberArea);

    // Gutter background — subtle darker panel
    painter.fillRect(event->rect(), QColor("#0f1018"));

    // Right border accent line
    painter.setPen(QColor("#1e2036"));
    painter.drawLine(event->rect().topRight(), event->rect().bottomRight());

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    int currentBlockNumber = textCursor().blockNumber();

    QFont lineFont = font();
    lineFont.setPointSize(font().pointSize() - 1);

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);

            if (blockNumber == currentBlockNumber) {
                // Current line number — bright accent
                painter.setPen(QColor("#c0caf5"));
                QFont boldFont = lineFont;
                boldFont.setBold(true);
                painter.setFont(boldFont);
            } else {
                // Non-current line numbers — dim
                painter.setPen(QColor("#3d4066"));
                painter.setFont(lineFont);
            }

            painter.drawText(0, top, m_lineNumberArea->width() - 12, fontMetrics().height(),
                             Qt::AlignRight | Qt::AlignVCenter, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

// ============================================================================
// Current Line Highlight
// ============================================================================

void CodeEditor::highlightCurrentLine() {
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(QColor("#1a1c2e"));
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    setExtraSelections(extraSelections);
}

// ============================================================================
// Resize — Sync line number area geometry
// ============================================================================

void CodeEditor::resizeEvent(QResizeEvent *event) {
    QPlainTextEdit::resizeEvent(event);

    QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

// ============================================================================
// Update Line Number Area on scroll/edit
// ============================================================================

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy) {
    if (dy) {
        m_lineNumberArea->scroll(0, dy);
    } else {
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());
    }

    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth(0);
    }
}

// ============================================================================
// Emit cursor line/column info for status bar
// ============================================================================

void CodeEditor::emitCursorInfo() {
    QTextCursor cursor = textCursor();
    int line = cursor.blockNumber() + 1;
    int column = cursor.columnNumber() + 1;
    emit cursorPositionInfo(line, column);
}
