#pragma once

#include <QString>
#include <functional>

// Abstract file picker so the app uses xdg-desktop-portal on Linux and tests
// can inject a deterministic fake without touching the desktop.
class FilePicker {
public:
    virtual ~FilePicker() = default;
    virtual void openFile(const QString &title, const QString &filterName, const QString &pattern,
                          std::function<void(const QString &path)> callback) = 0;
    virtual void saveFile(const QString &title, const QString &suggestedName,
                          const QString &filterName, const QString &pattern,
                          std::function<void(const QString &path)> callback) = 0;
};
