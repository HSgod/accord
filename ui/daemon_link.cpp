/*
 * Accord - The one line the title shows for the title-to-daemon link (T2.2b).
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ui/daemon_link.hpp"

#include <cstdio>

namespace accord::ui
{
std::string format_daemon_link_line(const DaemonLinkResult &result)
{
    if (!result.probed)
        return "daemon link: probing 127.0.0.1:" + std::to_string(kDaemonPort) + " ...";

    char timing[32];
    std::snprintf(timing, sizeof timing, "%.0f ms", result.milliseconds);

    if (result.answered)
        return "daemon link: HTTP " + std::to_string(result.status) + " in " + timing;

    return std::string("daemon link: FAILED in ") + timing + ": " + result.detail;
}
} // namespace accord::ui
