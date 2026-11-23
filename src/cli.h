#pragma once

#include <QStringList>

namespace Cli {
bool isHeadlessCommand(const QStringList &args);
int run(const QStringList &args, const QString &dbPath = {});
} // namespace Cli
