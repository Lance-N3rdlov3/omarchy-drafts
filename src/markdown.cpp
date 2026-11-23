#include "markdown.h"

#include <QRegularExpression>
#include <QStringList>
#include <QUrl>

QString MarkdownProcessor::escapeHtml(const QString &s) {
    QString out = s;
    out.replace('&', "&amp;");
    out.replace('<', "&lt;");
    out.replace('>', "&gt;");
    out.replace('"', "&quot;");
    return out;
}

QString MarkdownProcessor::renderInline(const QString &line, const QString &accentColor) {
    QString s = escapeHtml(line);

    // Inline code spans first, using placeholders so inner markup is untouched.
    QStringList codeSpans;
    static const QRegularExpression codeRe(R"(`([^`]+)`)");
    int offset = 0;
    while (true) {
        auto m = codeRe.match(s, offset);
        if (!m.hasMatch())
            break;
        const int idx = codeSpans.size();
        codeSpans.append(QStringLiteral("<code style=\"background:#242428;padding:1px 4px;font-family:monospace;\">%1</code>")
                             .arg(m.captured(1)));
        const QString token = QStringLiteral("\x01CODE%1\x01").arg(idx);
        s.replace(m.capturedStart(), m.capturedLength(), token);
        offset = m.capturedStart() + token.size();
    }

    // Wiki-links [[Title]] or [[Title#Heading]]
    static const QRegularExpression wikiRe(R"(\[\[([^\]]+)\]\])");
    offset = 0;
    while (true) {
        auto m = wikiRe.match(s, offset);
        if (!m.hasMatch())
            break;
        const QString inner = m.captured(1);
        const QString encoded = QString::fromUtf8(QUrl::toPercentEncoding(inner));
        const QString link = QStringLiteral("<a href=\"drafts-wiki:%1\" style=\"color:%2;text-decoration:underline;\">[[%3]]</a>")
                                 .arg(encoded, accentColor, inner);
        s.replace(m.capturedStart(), m.capturedLength(), link);
        offset = m.capturedStart() + link.size();
    }

    // Markdown links [text](url)
    static const QRegularExpression mdLinkRe(R"(\[([^\]]+)\]\(([^)\s]+)\))");
    offset = 0;
    while (true) {
        auto m = mdLinkRe.match(s, offset);
        if (!m.hasMatch())
            break;
        const QString label = m.captured(1);
        const QString url = m.captured(2);
        const QString link = QStringLiteral("<a href=\"%1\" style=\"color:%2;\">%3</a>")
                                 .arg(url, accentColor, label);
        s.replace(m.capturedStart(), m.capturedLength(), link);
        offset = m.capturedStart() + link.size();
    }

    // Bold + Italic, Bold, Italic, Strikethrough
    static const QRegularExpression boldItalicRe(R"(\*\*\*([^*]+)\*\*\*)");
    s.replace(boldItalicRe, "<b><i>\\1</i></b>");
    static const QRegularExpression boldRe(R"(\*\*([^*]+)\*\*)");
    s.replace(boldRe, "<b>\\1</b>");
    static const QRegularExpression boldUnderRe(R"(__([^_]+)__)");
    s.replace(boldUnderRe, "<b>\\1</b>");
    static const QRegularExpression italicRe(R"(\*([^*]+)\*)");
    s.replace(italicRe, "<i>\\1</i>");
    static const QRegularExpression strikeRe(R"(~~([^~]+)~~)");
    s.replace(strikeRe, "<s>\\1</s>");

    // Restore code spans
    for (int i = 0; i < codeSpans.size(); ++i)
        s.replace(QStringLiteral("\x01CODE%1\x01").arg(i), codeSpans[i]);

    return s;
}

QString MarkdownProcessor::toHtml(const QString &markdown, const QString &accentColor,
                                  bool standaloneDocument) {
    const QStringList lines = markdown.split('\n');
    QString out;
    bool inCodeBlock = false;
    bool inUl = false;
    bool inOl = false;
    bool inBlockquote = false;
    bool inTable = false;

    auto closeLists = [&]() {
        if (inUl) { out += "</ul>\n"; inUl = false; }
        if (inOl) { out += "</ol>\n"; inOl = false; }
        if (inBlockquote) { out += "</blockquote>\n"; inBlockquote = false; }
        if (inTable) { out += "</tbody></table>\n"; inTable = false; }
    };

    static const QRegularExpression headingRe(R"(^(#{1,6})\s+(.+)$)");
    static const QRegularExpression hrRe(R"(^\s*([-*_])(\s*\1){2,}\s*$)");
    static const QRegularExpression taskRe(R"(^\s*[-*+]\s+\[([ xX])\]\s+(.*)$)");
    static const QRegularExpression ulRe(R"(^\s*[-*+]\s+(.*)$)");
    static const QRegularExpression olRe(R"(^\s*\d+\.\s+(.*)$)");
    static const QRegularExpression tableSepRe(R"(^\s*\|?(\s*:?-+:?\s*\|)+\s*:?-+:?\s*\|?\s*$)");

    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines[i];

        // Fenced code blocks
        if (line.trimmed().startsWith("```")) {
            if (!inCodeBlock) {
                closeLists();
                inCodeBlock = true;
                out += "<pre style=\"background:#18181b;padding:10px 12px;border:1px solid #2c2c32;font-family:monospace;white-space:pre-wrap;\"><code>";
            } else {
                inCodeBlock = false;
                out += "</code></pre>\n";
            }
            continue;
        }
        if (inCodeBlock) {
            out += escapeHtml(line) + "\n";
            continue;
        }

        if (line.trimmed().isEmpty()) {
            closeLists();
            continue;
        }

        // GFM Table
        if (line.contains('|') && !inTable && i + 1 < lines.size() && tableSepRe.match(lines[i + 1]).hasMatch()) {
            closeLists();
            inTable = true;
            auto splitCells = [](const QString &row) {
                QString r = row.trimmed();
                if (r.startsWith('|')) r.remove(0, 1);
                if (r.endsWith('|')) r.chop(1);
                return r.split('|');
            };
            out += "<table style=\"border-collapse:collapse;width:100%;margin:8px 0;\"><thead><tr>";
            for (const QString &cell : splitCells(line))
                out += QStringLiteral("<th style=\"border:1px solid #333;padding:6px 8px;text-align:left;\">%1</th>")
                           .arg(renderInline(cell.trimmed(), accentColor));
            out += "</tr></thead><tbody>\n";
            ++i; // skip separator row
            continue;
        }
        if (inTable) {
            if (!line.contains('|')) {
                out += "</tbody></table>\n";
                inTable = false;
            } else {
                QString r = line.trimmed();
                if (r.startsWith('|')) r.remove(0, 1);
                if (r.endsWith('|')) r.chop(1);
                out += "<tr>";
                for (const QString &cell : r.split('|'))
                    out += QStringLiteral("<td style=\"border:1px solid #333;padding:6px 8px;\">%1</td>")
                               .arg(renderInline(cell.trimmed(), accentColor));
                out += "</tr>\n";
                continue;
            }
        }

        // Headings
        auto hm = headingRe.match(line);
        if (hm.hasMatch()) {
            closeLists();
            const int level = hm.captured(1).size();
            const QString color = (level <= 2) ? accentColor : "#e4e4e7";
            out += QStringLiteral("<h%1 style=\"color:%2;margin:14px 0 6px 0;\">%3</h%1>\n")
                       .arg(level).arg(color, renderInline(hm.captured(2), accentColor));
            continue;
        }

        // Horizontal rule
        if (hrRe.match(line).hasMatch()) {
            closeLists();
            out += "<hr style=\"border:none;border-top:1px solid #333;margin:14px 0;\"/>\n";
            continue;
        }

        // Blockquote
        if (line.trimmed().startsWith('>')) {
            if (!inBlockquote) {
                closeLists();
                inBlockquote = true;
                out += QStringLiteral("<blockquote style=\"border-left:3px solid %1;margin:8px 0;padding:4px 12px;color:#a1a1aa;\">").arg(accentColor);
            }
            QString inner = line.trimmed().mid(1).trimmed();
            out += renderInline(inner, accentColor) + "<br/>";
            continue;
        } else if (inBlockquote) {
            out += "</blockquote>\n";
            inBlockquote = false;
        }

        // Task list item
        auto tm = taskRe.match(line);
        if (tm.hasMatch()) {
            if (!inUl) {
                closeLists();
                inUl = true;
                out += "<ul style=\"margin:6px 0;padding-left:20px;\">\n";
            }
            const bool checked = (tm.captured(1).toLower() == "x");
            const QString box = checked ? QStringLiteral("<span style=\"color:%1;\">&#9745;</span>").arg(accentColor)
                                        : QStringLiteral("<span>&#9744;</span>");
            const QString txt = checked ? QStringLiteral("<s style=\"color:#71717a;\">%1</s>").arg(renderInline(tm.captured(2), accentColor))
                                        : renderInline(tm.captured(2), accentColor);
            out += QStringLiteral("<li style=\"list-style:none;\">%1 %2</li>\n").arg(box, txt);
            continue;
        }

        // Unordered list
        auto um = ulRe.match(line);
        if (um.hasMatch()) {
            if (!inUl) {
                closeLists();
                inUl = true;
                out += "<ul style=\"margin:6px 0;padding-left:22px;\">\n";
            }
            out += QStringLiteral("<li>%1</li>\n").arg(renderInline(um.captured(1), accentColor));
            continue;
        }

        // Ordered list
        auto om = olRe.match(line);
        if (om.hasMatch()) {
            if (!inOl) {
                closeLists();
                inOl = true;
                out += "<ol style=\"margin:6px 0;padding-left:22px;\">\n";
            }
            out += QStringLiteral("<li>%1</li>\n").arg(renderInline(om.captured(1), accentColor));
            continue;
        }

        // Regular paragraph line
        closeLists();
        out += QStringLiteral("<p style=\"margin:6px 0;line-height:1.55;\">%1</p>\n")
                   .arg(renderInline(line, accentColor));
    }

    if (inCodeBlock)
        out += "</code></pre>\n";
    closeLists();

    if (!standaloneDocument)
        return out;

    return QStringLiteral(
               "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
               "<style>"
               "body{background:#121214;color:#e4e4e7;font-family:system-ui,-apple-system,sans-serif;"
               "max-width:740px;margin:36px auto;padding:0 24px;line-height:1.6;}"
               "a{color:%1;} code,pre{font-family:monospace;}"
               "</style></head><body>%2</body></html>")
        .arg(accentColor, out);
}

QVector<OutlineItem> MarkdownProcessor::extractOutline(const QString &text, const QString &syntax) {
    QVector<OutlineItem> items;
    const QStringList lines = text.split('\n');
    int pos = 0;

    static const QRegularExpression headingRe(R"(^(#{1,6})\s+(.+)$)");
    static const QRegularExpression tpProjectRe(R"(^([^-\s][^:]*):\s*$)");
    static const QRegularExpression wikiRe(R"(\[\[([^\]]+)\]\])");

    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines[i];

        auto hm = headingRe.match(line);
        if (hm.hasMatch()) {
            OutlineItem item;
            item.title = hm.captured(2).trimmed();
            item.kind = "heading";
            item.level = hm.captured(1).size();
            item.line = i;
            item.position = pos;
            items.append(item);
        } else if (syntax == "taskpaper") {
            auto pm = tpProjectRe.match(line);
            if (pm.hasMatch()) {
                OutlineItem item;
                item.title = pm.captured(1).trimmed();
                item.kind = "project";
                item.level = 1;
                item.line = i;
                item.position = pos;
                items.append(item);
            }
        }

        auto wit = wikiRe.globalMatch(line);
        while (wit.hasNext()) {
            auto wm = wit.next();
            OutlineItem item;
            item.title = wm.captured(1).trimmed();
            item.kind = "wikilink";
            item.level = 2;
            item.line = i;
            item.position = pos + wm.capturedStart();
            items.append(item);
        }

        pos += line.size() + 1;
    }
    return items;
}

QVector<WikiLinkRef> MarkdownProcessor::extractWikiLinks(const QString &text) {
    QVector<WikiLinkRef> refs;
    static const QRegularExpression wikiRe(R"(\[\[([^\]]+)\]\])");
    auto it = wikiRe.globalMatch(text);
    while (it.hasNext()) {
        auto m = it.next();
        WikiLinkRef ref;
        ref.raw = m.captured(1).trimmed();
        ref.start = m.capturedStart();
        ref.length = m.capturedLength();

        if (ref.raw.startsWith("s:", Qt::CaseInsensitive)) {
            ref.kind = "search";
            ref.target = ref.raw.mid(2).trimmed();
        } else if (ref.raw.startsWith("w:", Qt::CaseInsensitive)) {
            ref.kind = "workspace";
            ref.target = ref.raw.mid(2).trimmed();
        } else if (ref.raw.startsWith("google:", Qt::CaseInsensitive)) {
            ref.kind = "url";
            ref.target = "https://www.google.com/search?q=" + QString::fromUtf8(QUrl::toPercentEncoding(ref.raw.mid(7).trimmed()));
        } else if (ref.raw.startsWith("u:", Qt::CaseInsensitive)) {
            ref.kind = "url";
            ref.target = ref.raw.mid(2).trimmed();
        } else {
            ref.kind = "draft";
            QString body = ref.raw;
            if (body.startsWith("d:", Qt::CaseInsensitive))
                body = body.mid(2).trimmed();
            const int hashIdx = body.indexOf('#');
            if (hashIdx >= 0) {
                ref.target = body.left(hashIdx).trimmed();
                ref.heading = body.mid(hashIdx + 1).trimmed();
            } else {
                ref.target = body;
            }
        }
        refs.append(ref);
    }
    return refs;
}

QString MarkdownProcessor::toLinkModeHtml(const QString &text, const QString &accentColor) {
    const QStringList lines = text.split('\n');
    QStringList outLines;

    static const QRegularExpression taskRe(R"(^(\s*[-*+]\s+)\[([ xX])\](.*)$)");
    static const QRegularExpression wikiRe(R"(\[\[([^\]]+)\]\])");
    static const QRegularExpression urlRe(R"((https?://[^\s<>"\)\]]+))");
    static const QRegularExpression mailRe(R"(([a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,}))");

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];

        // Interactive checkbox toggle in Link Mode!
        auto tm = taskRe.match(line);
        QString prefixHtml;
        if (tm.hasMatch()) {
            const bool checked = (tm.captured(2).toLower() == "x");
            const QString glyph = checked ? "&#9745;" : "&#9744;";
            prefixHtml = QStringLiteral("%1<a href=\"drafts-task:%2\" style=\"color:%3;text-decoration:none;font-weight:bold;\">%4</a>")
                             .arg(escapeHtml(tm.captured(1)))
                             .arg(i)
                             .arg(accentColor, glyph);
            line = tm.captured(3);
        }

        QString escaped = escapeHtml(line);

        // Wiki links
        int offset = 0;
        while (true) {
            auto m = wikiRe.match(escaped, offset);
            if (!m.hasMatch())
                break;
            const QString inner = m.captured(1);
            const QString enc = QString::fromUtf8(QUrl::toPercentEncoding(inner));
            const QString a = QStringLiteral("<a href=\"drafts-wiki:%1\" style=\"color:%2;text-decoration:underline;\">[[%3]]</a>")
                                  .arg(enc, accentColor, inner);
            escaped.replace(m.capturedStart(), m.capturedLength(), a);
            offset = m.capturedStart() + a.size();
        }

        // HTTP(S) URLs (only outside already-generated anchor tags)
        offset = 0;
        while (true) {
            auto m = urlRe.match(escaped, offset);
            if (!m.hasMatch())
                break;
            const int start = m.capturedStart();
            if (start >= 6 && escaped.mid(start - 6, 6) == "href=\"") {
                offset = m.capturedEnd();
                continue;
            }
            const QString u = m.captured(1);
            const QString a = QStringLiteral("<a href=\"%1\" style=\"color:%2;text-decoration:underline;\">%1</a>")
                                  .arg(u, accentColor);
            escaped.replace(start, m.capturedLength(), a);
            offset = start + a.size();
        }

        // Email addresses
        offset = 0;
        while (true) {
            auto m = mailRe.match(escaped, offset);
            if (!m.hasMatch())
                break;
            const int start = m.capturedStart();
            if (start >= 7 && escaped.mid(start - 7, 7) == "mailto:") {
                offset = m.capturedEnd();
                continue;
            }
            const QString addr = m.captured(1);
            const QString a = QStringLiteral("<a href=\"mailto:%1\" style=\"color:%2;text-decoration:underline;\">%1</a>")
                                  .arg(addr, accentColor);
            escaped.replace(start, m.capturedLength(), a);
            offset = start + a.size();
        }

        outLines.append(prefixHtml + escaped);
    }

    return QStringLiteral("<div style=\"white-space:pre-wrap;\">%1</div>").arg(outLines.join("<br/>"));
}
