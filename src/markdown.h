#pragma once

#include <QString>
#include <QVariantList>
#include <QVector>

struct OutlineItem {
    QString title;
    QString kind; // heading | project | wikilink
    int level = 1;
    int line = 0;
    int position = 0;
};

struct WikiLinkRef {
    QString raw;
    QString target;   // draft title, workspace name, or query
    QString heading;  // optional #Heading anchor
    QString kind;     // draft | search | workspace | url
    int start = 0;
    int length = 0;
};

class MarkdownProcessor {
public:
    // Renders Markdown (CommonMark + GFM tables, strikethrough, task lists, wiki-links)
    // to HTML suitable for QML RichText or standalone HTML export.
    static QString toHtml(const QString &markdown, const QString &accentColor = "#e68e0d",
                          bool standaloneDocument = false);

    // Extracts headings (#..######), TaskPaper projects, and [[wiki-links]] for Outline Navigation.
    static QVector<OutlineItem> extractOutline(const QString &text, const QString &syntax = "markdown");

    // Extracts all [[...]] wiki-links from the text.
    static QVector<WikiLinkRef> extractWikiLinks(const QString &text);

    // Converts draft content into clickable Link Mode HTML (URLs, emails, [[wiki-links]], checkboxes).
    static QString toLinkModeHtml(const QString &text, const QString &accentColor = "#e68e0d");

private:
    static QString escapeHtml(const QString &s);
    static QString renderInline(const QString &line, const QString &accentColor);
};
