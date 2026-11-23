#pragma once

#include "store.h"

#include <QDateTime>
#include <QString>

struct TemplateContext {
    DraftRecord draft;
    int selectionStart = 0;
    int selectionLength = 0;
    QString clipboard;
    QString accentColor = "#e68e0d";
    QDateTime now = QDateTime::currentDateTime();
};

class TemplateEngine {
public:
    static QString process(const QString &tmpl, const TemplateContext &ctx);
    static QString formatStrftime(const QDateTime &dt, const QString &fmt);
    static QDateTime adjustDate(const QDateTime &base, const QString &adjustment);
};
