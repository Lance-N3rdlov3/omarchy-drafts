#include "templateengine.h"
#include "markdown.h"

#include <QRegularExpression>
#include <QUrl>
#include <ctime>

QString TemplateEngine::formatStrftime(const QDateTime &dt, const QString &fmt) {
    const std::time_t secs = static_cast<std::time_t>(dt.toSecsSinceEpoch());
    std::tm tmBuf{};
    localtime_r(&secs, &tmBuf);
    char buffer[512];
    const QByteArray fmtBytes = fmt.toUtf8();
    const size_t n = std::strftime(buffer, sizeof(buffer), fmtBytes.constData(), &tmBuf);
    if (n == 0)
        return dt.toString("yyyy-MM-dd");
    return QString::fromUtf8(buffer, static_cast<int>(n));
}

QDateTime TemplateEngine::adjustDate(const QDateTime &base, const QString &adjustment) {
    QDateTime dt = base;
    static const QRegularExpression re(
        R"(([+-]?\d+)\s*(years?|yrs?|y|months?|mos?|weeks?|wks?|w|days?|d|hours?|hrs?|h|minutes?|mins?|m|seconds?|secs?|s))",
        QRegularExpression::CaseInsensitiveOption);
    auto it = re.globalMatch(adjustment);
    while (it.hasNext()) {
        auto m = it.next();
        const int amount = m.captured(1).toInt();
        const QString unit = m.captured(2).toLower();
        if (unit.startsWith('y'))
            dt = dt.addYears(amount);
        else if (unit.startsWith("mo"))
            dt = dt.addMonths(amount);
        else if (unit.startsWith('w'))
            dt = dt.addDays(amount * 7);
        else if (unit.startsWith('d'))
            dt = dt.addDays(amount);
        else if (unit.startsWith('h'))
            dt = dt.addSecs(static_cast<qint64>(amount) * 3600);
        else if (unit.startsWith("mi") || unit == "m")
            dt = dt.addSecs(static_cast<qint64>(amount) * 60);
        else if (unit.startsWith('s'))
            dt = dt.addSecs(amount);
    }
    return dt;
}

QString TemplateEngine::process(const QString &tmpl, const TemplateContext &ctx) {
    if (tmpl.isEmpty())
        return tmpl;

    const QString &content = ctx.draft.content;
    const int selStart = qBound(0, ctx.selectionStart, content.size());
    const int selLen = qBound(0, ctx.selectionLength, content.size() - selStart);
    const QString selectedText = content.mid(selStart, selLen);

    auto currentLine = [&]() -> QString {
        int start = content.lastIndexOf('\n', selStart > 0 ? selStart - 1 : 0);
        start = (start < 0) ? 0 : start + 1;
        int end = content.indexOf('\n', selStart);
        if (end < 0)
            end = content.size();
        return content.mid(start, end - start);
    };

    auto lineRange = [&](const QString &spec) -> QString {
        const QStringList lines = content.split('\n');
        if (lines.isEmpty())
            return {};
        auto resolveIdx = [&](int n) {
            if (n < 0)
                return qBound(0, lines.size() + n, lines.size() - 1);
            return qBound(0, n - 1, lines.size() - 1);
        };
        if (spec.contains("..")) {
            const QStringList parts = spec.split("..");
            int a = parts.value(0).isEmpty() ? 0 : resolveIdx(parts.value(0).toInt());
            int b = parts.value(1).isEmpty() ? (lines.size() - 1) : resolveIdx(parts.value(1).toInt());
            if (a > b)
                return {};
            QStringList slice;
            for (int i = a; i <= b; ++i)
                slice.append(lines[i]);
            return slice.join('\n');
        }
        return lines.value(resolveIdx(spec.toInt()));
    };

    auto evalTag = [&](QString expr) -> QString {
        bool urlEncode = false;
        if (expr.startsWith("enc:", Qt::CaseInsensitive)) {
            urlEncode = true;
            expr = expr.mid(4);
        }

        const QStringList parts = expr.split('|');
        const QString key = parts.value(0).trimmed().toLower();
        QString val;

        if (key == "draft") {
            val = content;
        } else if (key == "title") {
            val = ctx.draft.title();
        } else if (key == "display_title") {
            val = ctx.draft.displayTitle();
        } else if (key == "safe_title") {
            val = ctx.draft.safeTitle();
        } else if (key == "body") {
            val = ctx.draft.body();
        } else if (key == "selection") {
            val = selectedText.isEmpty() ? content : selectedText;
        } else if (key == "selection_only") {
            val = selectedText;
        } else if (key == "selection_start") {
            val = QString::number(selStart);
        } else if (key == "selection_length") {
            val = QString::number(selLen);
        } else if (key == "line") {
            val = (parts.size() >= 2) ? lineRange(parts[1].trimmed()) : currentLine();
        } else if (key == "uuid") {
            val = ctx.draft.uuid;
        } else if (key == "tags") {
            val = ctx.draft.tags.join(", ");
        } else if (key == "hashtags") {
            QStringList hashed;
            for (const QString &t : ctx.draft.tags)
                hashed.append("#" + t);
            val = hashed.join(' ');
        } else if (key == "clipboard") {
            val = ctx.clipboard;
        } else if (key == "draft_open_url") {
            val = "drafts://open?uuid=" + ctx.draft.uuid;
        } else if (key == "markdown") {
            val = MarkdownProcessor::toHtml(content, ctx.accentColor, false);
        } else if (key == "selection_markdown") {
            val = MarkdownProcessor::toHtml(selectedText.isEmpty() ? content : selectedText, ctx.accentColor, false);
        } else if (key == "date") {
            QDateTime dt = ctx.now;
            QString fmt = "%Y-%m-%d";
            if (parts.size() == 2) {
                const QString p1 = parts[1].trimmed();
                if (p1.contains('%'))
                    fmt = p1;
                else
                    dt = adjustDate(dt, p1);
            } else if (parts.size() >= 3) {
                dt = adjustDate(dt, parts[1].trimmed());
                fmt = parts[2].trimmed();
            }
            val = formatStrftime(dt, fmt);
        } else if (key == "time") {
            val = formatStrftime(ctx.now, "%Y-%m-%d-%H-%M-%S");
        } else if (key == "created") {
            QDateTime dt = QDateTime::fromString(ctx.draft.createdAt, Qt::ISODateWithMs).toLocalTime();
            if (!dt.isValid())
                dt = ctx.now;
            val = formatStrftime(dt, parts.size() >= 2 ? parts[1].trimmed() : "%Y-%m-%d");
        } else if (key == "modified") {
            QDateTime dt = QDateTime::fromString(ctx.draft.modifiedAt, Qt::ISODateWithMs).toLocalTime();
            if (!dt.isValid())
                dt = ctx.now;
            val = formatStrftime(dt, parts.size() >= 2 ? parts[1].trimmed() : "%Y-%m-%d");
        } else {
            // Unknown tag: keep verbatim so wiki-links like [[My Note]] aren't destroyed.
            return "[[" + expr + "]]";
        }

        if (urlEncode)
            return QString::fromUtf8(QUrl::toPercentEncoding(val));
        return val;
    };

    // Match innermost [[...]] tokens without nested brackets so [[[[title]]]] -> [[Title]].
    static const QRegularExpression tagRe(R"(\[\[([^\[\]]+)\]\])");
    QString out = tmpl;
    int offset = 0;
    while (true) {
        auto m = tagRe.match(out, offset);
        if (!m.hasMatch())
            break;
        const QString replaced = evalTag(m.captured(1));
        out.replace(m.capturedStart(), m.capturedLength(), replaced);
        offset = m.capturedStart() + replaced.size();
    }
    return out;
}
