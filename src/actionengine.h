#pragma once

#include "store.h"

#include <QObject>
#include <QString>
#include <functional>

struct ActionRunInput {
    DraftRecord draft;
    int selectionStart = 0;
    int selectionLength = 0;
    QString accentColor = "#e68e0d";
    bool allowDesktopSideEffects = true; // false in unit tests if desired
};

struct ActionRunResult {
    bool ok = true;
    bool cancelled = false;
    QString status = "success"; // success | cancelled | error
    QString message;
    QString htmlPreview;
    bool draftModified = false;
    bool newDraftRequested = false;
    QString loadDraftUuid;
    DraftRecord updatedDraft;
    int newSelectionStart = -1;
    int newSelectionLength = 0;
    QStringList logs;
};

class ActionEngine {
public:
    explicit ActionEngine(Store &store);

    // Optional hooks for clipboard & URL opening so tests can intercept cleanly.
    std::function<QString()> getClipboard;
    std::function<void(const QString &)> setClipboard;
    std::function<bool(const QString &)> openUrl;

    ActionRunResult execute(const ActionRecord &action, const ActionRunInput &input);

private:
    QString readClipboard() const;
    void writeClipboard(const QString &text) const;
    bool launchUrl(const QString &url) const;

    bool runScriptStep(const QString &code, DraftRecord &draft, int &selStart, int &selLen,
                       const QString &accentColor, ActionRunResult &res);

    Store &m_store;
};
