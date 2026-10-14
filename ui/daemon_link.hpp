/*
 * Accord - The one line the title shows for the title-to-daemon link (T2.2b).
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Pure, in ui/ next to the tone schedule, for the same reason: the console code
 * that opens the socket lives in platform/ps5/, and what it has to say is a
 * value the host test can pin without a console, a socket or a daemon.
 */

#pragma once

#include <string>

namespace accord::ui
{
// accord::daemon::kDefaultPort in daemon/src/config.hpp. Repeated rather than
// included: the title's staging carries core/, platform/ps5/ and ui/, not
// daemon/, and a test that finds nothing listening should say so in the report
// instead of failing to build.
inline constexpr unsigned short kDaemonPort = 8280;

struct DaemonLinkResult
{
    bool probed = false;    // a connection was attempted
    bool answered = false;  // a complete HTTP answer came back
    int status = 0;         // its status code, when answered
    double milliseconds = 0.0;
    std::string detail;     // the failure, or the head of the body
};

// "daemon link: HTTP 200 in 7 ms" / "daemon link: FAILED in 7 ms: ..." /
// "daemon link: probing 127.0.0.1:8280 ...".
std::string format_daemon_link_line(const DaemonLinkResult &result);
} // namespace accord::ui
