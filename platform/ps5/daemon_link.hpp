/*
 * Accord - PS5 side of the title-to-daemon link probe (T2.2b).
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The fallback variant of T2.3c sends PCM and a heartbeat from the daemon to the
 * title over 127.0.0.1, so the title has to be able to open that connection at
 * all. This is the smallest thing that answers it: one GET /status, timed, with
 * the HTTP status or the failure kept as a value the caller can show on screen
 * and write to klog. Nothing here retries and nothing here decides anything --
 * the caller reports what came back.
 */

#pragma once

#include "ui/daemon_link.hpp"

namespace accord::ps5
{
// One GET http://127.0.0.1:<kDaemonPort>/status, bounded by timeout_ms for the
// whole exchange. Never throws; failure is a value.
accord::ui::DaemonLinkResult probe_daemon_link(int timeout_ms);
} // namespace accord::ps5
