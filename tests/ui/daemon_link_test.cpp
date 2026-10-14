/*
 * Accord - Tests for the one line the title shows for the daemon link (T2.2b).
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * What the console has to answer is whether the title can reach the daemon over
 * 127.0.0.1 at all, and that is a socket and a console. What is checked here is
 * the part that is ours to get wrong: the line the report quotes, with the
 * status and the time when there is an answer, and the reason when there is not.
 */

#include "tests/micro_test.hpp"

#include "ui/daemon_link.hpp"

using namespace accord::ui;

MICRO_TEST(a_link_that_has_not_been_probed_says_so)
{
    MICRO_CHECK(format_daemon_link_line(DaemonLinkResult{}) ==
                "daemon link: probing 127.0.0.1:8280 ...");
}

MICRO_TEST(an_answer_carries_the_status_and_the_time)
{
    DaemonLinkResult result;
    result.probed = true;
    result.answered = true;
    result.status = 200;
    result.milliseconds = 7.4;
    MICRO_CHECK(format_daemon_link_line(result) == "daemon link: HTTP 200 in 7 ms");
}

MICRO_TEST(a_failure_carries_the_time_and_the_reason)
{
    DaemonLinkResult result;
    result.probed = true;
    result.milliseconds = 3.2;
    result.detail = "connect: Connection refused";
    MICRO_CHECK(format_daemon_link_line(result) ==
                "daemon link: FAILED in 3 ms: connect: Connection refused");
}
