#pragma once

#include <QStringList>

// Commands that need no display, for scripts and agents:
//   rewind episodes <show> [--offset N] [--limit N] [--json]
//   rewind download <link>... [--output DIR] [--json]
//   rewind skill install
// Each exits 0 on success and 1 on failure, with errors on stderr.
namespace Cli {

// Whether argv names a command rather than the window.
bool handles(const QStringList &arguments);
int run(const QStringList &arguments);
QString usage();

}
