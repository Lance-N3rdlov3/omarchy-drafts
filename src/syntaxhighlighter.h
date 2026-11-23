#pragma once

#include <QColor>
#include <QObject>
#include <QQuickTextDocument>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

class DraftSyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit DraftSyntaxHighlighter(QTextDocument *parent = nullptr);

    void setSyntax(const QString &syntax);
    void setColors(const QString &accent, const QString &foreground, const QString &muted);

protected:
    void highlightBlock(const QString &text) override;

private:
    void highlightMarkdown(const QString &text);
    void highlightTaskPaper(const QString &text);
    void highlightJavaScript(const QString &text);
    void highlightJson(const QString &text);
    void highlightHtml(const QString &text);

    QString m_syntax = "markdown";
    QColor m_accent{"#e68e0d"};
    QColor m_foreground{"#e4e4e7"};
    QColor m_muted{"#71717a"};
};

// QML-friendly wrapper that attaches DraftSyntaxHighlighter to a TextArea's textDocument.
class SyntaxHighlighterAdapter : public QObject {
    Q_OBJECT
    Q_PROPERTY(QQuickTextDocument *document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(QString syntax READ syntax WRITE setSyntax NOTIFY syntaxChanged)
    Q_PROPERTY(QString accent READ accent WRITE setAccent NOTIFY colorsChanged)
    Q_PROPERTY(QString foreground READ foreground WRITE setForeground NOTIFY colorsChanged)
    Q_PROPERTY(QString muted READ muted WRITE setMuted NOTIFY colorsChanged)

public:
    explicit SyntaxHighlighterAdapter(QObject *parent = nullptr);

    QQuickTextDocument *document() const { return m_document; }
    void setDocument(QQuickTextDocument *doc);

    QString syntax() const { return m_syntax; }
    void setSyntax(const QString &syntax);

    QString accent() const { return m_accent; }
    void setAccent(const QString &accent);

    QString foreground() const { return m_foreground; }
    void setForeground(const QString &foreground);

    QString muted() const { return m_muted; }
    void setMuted(const QString &muted);

signals:
    void documentChanged();
    void syntaxChanged();
    void colorsChanged();

private:
    void syncHighlighter();

    QQuickTextDocument *m_document = nullptr;
    DraftSyntaxHighlighter *m_highlighter = nullptr;
    QString m_syntax = "markdown";
    QString m_accent = "#e68e0d";
    QString m_foreground = "#e4e4e7";
    QString m_muted = "#71717a";
};
