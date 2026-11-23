#include "syntaxhighlighter.h"

#include <QFont>
#include <QRegularExpression>

DraftSyntaxHighlighter::DraftSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent) {}

void DraftSyntaxHighlighter::setSyntax(const QString &syntax) {
    if (m_syntax == syntax)
        return;
    m_syntax = syntax;
    rehighlight();
}

void DraftSyntaxHighlighter::setColors(const QString &accent, const QString &foreground, const QString &muted) {
    m_accent = QColor(accent);
    m_foreground = QColor(foreground);
    m_muted = QColor(muted);
    rehighlight();
}

void DraftSyntaxHighlighter::highlightBlock(const QString &text) {
    if (m_syntax == "plain") {
        setCurrentBlockState(0);
        return;
    }
    if (m_syntax == "taskpaper") {
        highlightTaskPaper(text);
    } else if (m_syntax == "javascript") {
        highlightJavaScript(text);
    } else if (m_syntax == "json") {
        highlightJson(text);
    } else if (m_syntax == "html") {
        highlightHtml(text);
    } else {
        highlightMarkdown(text);
    }
}

void DraftSyntaxHighlighter::highlightMarkdown(const QString &text) {
    // State 1 = inside fenced code block
    const bool prevCode = (previousBlockState() == 1);
    const bool isFence = text.trimmed().startsWith("```");

    QTextCharFormat codeBlockFmt;
    codeBlockFmt.setForeground(QColor("#a5f3fc"));
    codeBlockFmt.setBackground(QColor(255, 255, 255, 10));

    if (isFence) {
        QTextCharFormat fenceFmt;
        fenceFmt.setForeground(m_muted);
        setFormat(0, text.size(), fenceFmt);
        setCurrentBlockState(prevCode ? 0 : 1);
        return;
    }
    if (prevCode) {
        setFormat(0, text.size(), codeBlockFmt);
        setCurrentBlockState(1);
        return;
    }
    setCurrentBlockState(0);

    // Headings (# to ######)
    static const QRegularExpression headingRe(R"(^(#{1,6})(\s+)(.*)$)");
    auto hm = headingRe.match(text);
    if (hm.hasMatch()) {
        QTextCharFormat hashFmt;
        hashFmt.setForeground(m_muted);
        hashFmt.setFontWeight(QFont::Bold);
        setFormat(0, hm.capturedLength(1), hashFmt);

        QTextCharFormat titleFmt;
        titleFmt.setForeground(hm.capturedLength(1) <= 2 ? m_accent : m_foreground);
        titleFmt.setFontWeight(QFont::Bold);
        setFormat(hm.capturedStart(3), hm.capturedLength(3), titleFmt);
        return;
    }

    // Horizontal rule
    static const QRegularExpression hrRe(R"(^\s*([-*_])(\s*\1){2,}\s*$)");
    if (hrRe.match(text).hasMatch()) {
        QTextCharFormat hrFmt;
        hrFmt.setForeground(m_muted);
        setFormat(0, text.size(), hrFmt);
        return;
    }

    // Blockquote
    if (text.trimmed().startsWith('>')) {
        QTextCharFormat bqFmt;
        bqFmt.setForeground(QColor("#a1a1aa"));
        bqFmt.setFontItalic(true);
        setFormat(0, text.size(), bqFmt);
        const int idx = text.indexOf('>');
        QTextCharFormat barFmt;
        barFmt.setForeground(m_accent);
        barFmt.setFontWeight(QFont::Bold);
        setFormat(idx, 1, barFmt);
    }

    // Task list checkboxes
    static const QRegularExpression doneTaskRe(R"(^(\s*[-*+]\s+)(\[[xX]\])(.*)$)");
    auto dtm = doneTaskRe.match(text);
    if (dtm.hasMatch()) {
        QTextCharFormat checkFmt;
        checkFmt.setForeground(QColor("#10b981"));
        checkFmt.setFontWeight(QFont::Bold);
        setFormat(dtm.capturedStart(2), dtm.capturedLength(2), checkFmt);

        QTextCharFormat doneFmt;
        doneFmt.setForeground(m_muted);
        doneFmt.setFontStrikeOut(true);
        setFormat(dtm.capturedStart(3), dtm.capturedLength(3), doneFmt);
        return;
    }

    static const QRegularExpression openTaskRe(R"(^(\s*[-*+]\s+)(\[ \]))");
    auto otm = openTaskRe.match(text);
    if (otm.hasMatch()) {
        QTextCharFormat boxFmt;
        boxFmt.setForeground(m_accent);
        boxFmt.setFontWeight(QFont::Bold);
        setFormat(otm.capturedStart(2), otm.capturedLength(2), boxFmt);
    } else {
        // List bullet / number
        static const QRegularExpression listRe(R"(^\s*([-*+]|\d+\.)\s)");
        auto lm = listRe.match(text);
        if (lm.hasMatch()) {
            QTextCharFormat bulletFmt;
            bulletFmt.setForeground(m_accent);
            bulletFmt.setFontWeight(QFont::Bold);
            setFormat(lm.capturedStart(1), lm.capturedLength(1), bulletFmt);
        }
    }

    // Inline bold (**text**)
    static const QRegularExpression boldRe(R"((\*\*|__)([^*_]+)\1)");
    for (auto it = boldRe.globalMatch(text); it.hasNext();) {
        auto m = it.next();
        QTextCharFormat fmt;
        fmt.setFontWeight(QFont::Bold);
        fmt.setForeground(QColor("#f4f4f5"));
        setFormat(m.capturedStart(), m.capturedLength(), fmt);
    }

    // Inline italic (*text*)
    static const QRegularExpression italicRe(R"((?<!\*)\*([^*]+)\*(?!\*))");
    for (auto it = italicRe.globalMatch(text); it.hasNext();) {
        auto m = it.next();
        QTextCharFormat fmt;
        fmt.setFontItalic(true);
        setFormat(m.capturedStart(), m.capturedLength(), fmt);
    }

    // Strikethrough (~~text~~)
    static const QRegularExpression strikeRe(R"(~~([^~]+)~~)");
    for (auto it = strikeRe.globalMatch(text); it.hasNext();) {
        auto m = it.next();
        QTextCharFormat fmt;
        fmt.setForeground(m_muted);
        fmt.setFontStrikeOut(true);
        setFormat(m.capturedStart(), m.capturedLength(), fmt);
    }

    // Inline code (`code`)
    static const QRegularExpression codeRe(R"(`[^`]+`)");
    for (auto it = codeRe.globalMatch(text); it.hasNext();) {
        auto m = it.next();
        QTextCharFormat fmt;
        fmt.setForeground(QColor("#fbbf24"));
        fmt.setBackground(QColor(255, 255, 255, 16));
        setFormat(m.capturedStart(), m.capturedLength(), fmt);
    }

    // Wiki-links [[Title]]
    static const QRegularExpression wikiRe(R"(\[\[[^\]]+\]\])");
    for (auto it = wikiRe.globalMatch(text); it.hasNext();) {
        auto m = it.next();
        QTextCharFormat fmt;
        fmt.setForeground(m_accent);
        fmt.setFontUnderline(true);
        setFormat(m.capturedStart(), m.capturedLength(), fmt);
    }

    // Markdown links [label](url)
    static const QRegularExpression linkRe(R"(\[([^\]]+)\]\(([^)]+)\))");
    for (auto it = linkRe.globalMatch(text); it.hasNext();) {
        auto m = it.next();
        QTextCharFormat labelFmt;
        labelFmt.setForeground(QColor("#60a5fa"));
        labelFmt.setFontUnderline(true);
        setFormat(m.capturedStart(1), m.capturedLength(1), labelFmt);

        QTextCharFormat urlFmt;
        urlFmt.setForeground(m_muted);
        setFormat(m.capturedStart(2), m.capturedLength(2), urlFmt);
    }
}

void DraftSyntaxHighlighter::highlightTaskPaper(const QString &text) {
    setCurrentBlockState(0);
    static const QRegularExpression projRe(R"(^(\s*[^-\s][^:]*):\s*$)");
    if (projRe.match(text).hasMatch()) {
        QTextCharFormat fmt;
        fmt.setForeground(m_accent);
        fmt.setFontWeight(QFont::Bold);
        setFormat(0, text.size(), fmt);
        return;
    }

    if (text.contains("@done")) {
        QTextCharFormat doneFmt;
        doneFmt.setForeground(m_muted);
        doneFmt.setFontStrikeOut(true);
        setFormat(0, text.size(), doneFmt);
        return;
    }

    static const QRegularExpression dashRe(R"(^\s*(-)\s)");
    auto dm = dashRe.match(text);
    if (dm.hasMatch()) {
        QTextCharFormat fmt;
        fmt.setForeground(m_accent);
        fmt.setFontWeight(QFont::Bold);
        setFormat(dm.capturedStart(1), 1, fmt);
    }

    static const QRegularExpression tagRe(R"(@[a-zA-Z0-9_\-]+(?:\([^)]*\))?)");
    for (auto it = tagRe.globalMatch(text); it.hasNext();) {
        auto m = it.next();
        QTextCharFormat fmt;
        fmt.setForeground(QColor("#38bdf8"));
        setFormat(m.capturedStart(), m.capturedLength(), fmt);
    }
}

void DraftSyntaxHighlighter::highlightJavaScript(const QString &text) {
    setCurrentBlockState(0);
    static const QRegularExpression kwRe(
        R"(\b(var|let|const|function|return|if|else|for|while|switch|case|break|continue|try|catch|finally|throw|new|class|extends|import|export|from|async|await|true|false|null|undefined|this)\b)");
    for (auto it = kwRe.globalMatch(text); it.hasNext();) {
        auto m = it.next();
        QTextCharFormat fmt;
        fmt.setForeground(m_accent);
        fmt.setFontWeight(QFont::Bold);
        setFormat(m.capturedStart(), m.capturedLength(), fmt);
    }

    static const QRegularExpression numRe(R"(\b\d+(?:\.\d+)?\b)");
    for (auto it = numRe.globalMatch(text); it.hasNext();) {
        auto m = it.next();
        QTextCharFormat fmt;
        fmt.setForeground(QColor("#c084fc"));
        setFormat(m.capturedStart(), m.capturedLength(), fmt);
    }

    static const QRegularExpression strRe(R"(('(?:[^'\\]|\\.)*'|"(?:[^"\\]|\\.)*"|`(?:[^`\\]|\\.)*`))");
    for (auto it = strRe.globalMatch(text); it.hasNext();) {
        auto m = it.next();
        QTextCharFormat fmt;
        fmt.setForeground(QColor("#4ade80"));
        setFormat(m.capturedStart(), m.capturedLength(), fmt);
    }

    static const QRegularExpression commentRe(R"(//.*$)");
    auto cm = commentRe.match(text);
    if (cm.hasMatch()) {
        QTextCharFormat fmt;
        fmt.setForeground(m_muted);
        fmt.setFontItalic(true);
        setFormat(cm.capturedStart(), cm.capturedLength(), fmt);
    }
}

void DraftSyntaxHighlighter::highlightJson(const QString &text) {
    setCurrentBlockState(0);
    static const QRegularExpression strRe(R"("(?:[^"\\]|\\.)*")");
    for (auto it = strRe.globalMatch(text); it.hasNext();) {
        auto m = it.next();
        const QString after = text.mid(m.capturedEnd()).trimmed();
        QTextCharFormat fmt;
        if (after.startsWith(':')) {
            fmt.setForeground(m_accent);
            fmt.setFontWeight(QFont::Bold);
        } else {
            fmt.setForeground(QColor("#4ade80"));
        }
        setFormat(m.capturedStart(), m.capturedLength(), fmt);
    }

    static const QRegularExpression litRe(R"(\b(true|false|null|-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)\b)");
    for (auto it = litRe.globalMatch(text); it.hasNext();) {
        auto m = it.next();
        QTextCharFormat fmt;
        fmt.setForeground(QColor("#c084fc"));
        setFormat(m.capturedStart(), m.capturedLength(), fmt);
    }
}

void DraftSyntaxHighlighter::highlightHtml(const QString &text) {
    setCurrentBlockState(0);
    static const QRegularExpression tagRe(R"(</?[a-zA-Z0-9\-]+|\s*/?>)");
    for (auto it = tagRe.globalMatch(text); it.hasNext();) {
        auto m = it.next();
        QTextCharFormat fmt;
        fmt.setForeground(m_accent);
        fmt.setFontWeight(QFont::Bold);
        setFormat(m.capturedStart(), m.capturedLength(), fmt);
    }

    static const QRegularExpression attrRe(R"(\b([a-zA-Z\-]+)=("[^"]*"|'[^']*'))");
    for (auto it = attrRe.globalMatch(text); it.hasNext();) {
        auto m = it.next();
        QTextCharFormat kFmt;
        kFmt.setForeground(QColor("#38bdf8"));
        setFormat(m.capturedStart(1), m.capturedLength(1), kFmt);
        QTextCharFormat vFmt;
        vFmt.setForeground(QColor("#4ade80"));
        setFormat(m.capturedStart(2), m.capturedLength(2), vFmt);
    }
}

// ---------------------------------------------------------------------------
// SyntaxHighlighterAdapter
// ---------------------------------------------------------------------------

SyntaxHighlighterAdapter::SyntaxHighlighterAdapter(QObject *parent)
    : QObject(parent) {}

void SyntaxHighlighterAdapter::setDocument(QQuickTextDocument *doc) {
    if (m_document == doc)
        return;
    m_document = doc;
    if (m_highlighter) {
        delete m_highlighter;
        m_highlighter = nullptr;
    }
    if (m_document && m_document->textDocument()) {
        m_highlighter = new DraftSyntaxHighlighter(m_document->textDocument());
        syncHighlighter();
    }
    emit documentChanged();
}

void SyntaxHighlighterAdapter::setSyntax(const QString &syntax) {
    if (m_syntax == syntax)
        return;
    m_syntax = syntax;
    syncHighlighter();
    emit syntaxChanged();
}

void SyntaxHighlighterAdapter::setAccent(const QString &accent) {
    if (m_accent == accent)
        return;
    m_accent = accent;
    syncHighlighter();
    emit colorsChanged();
}

void SyntaxHighlighterAdapter::setForeground(const QString &foreground) {
    if (m_foreground == foreground)
        return;
    m_foreground = foreground;
    syncHighlighter();
    emit colorsChanged();
}

void SyntaxHighlighterAdapter::setMuted(const QString &muted) {
    if (m_muted == muted)
        return;
    m_muted = muted;
    syncHighlighter();
    emit colorsChanged();
}

void SyntaxHighlighterAdapter::syncHighlighter() {
    if (!m_highlighter)
        return;
    m_highlighter->setColors(m_accent, m_foreground, m_muted);
    m_highlighter->setSyntax(m_syntax);
}
