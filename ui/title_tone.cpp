/*
 * Accord - The tone the title plays for the T2.3b control run.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ui/title_tone.hpp"

namespace accord::ui
{
bool title_tone_due(double now_seconds, double last_posted_seconds) noexcept
{
    return now_seconds - last_posted_seconds >= kTitleTonePeriodSeconds;
}
} // namespace accord::ui
