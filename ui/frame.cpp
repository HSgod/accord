/*
 * Accord - composing a frame, shared by the console title and the host runner.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ui/frame.hpp"

namespace accord::ui
{
void build_frame(hui::gfx::DrawList &list, const Screen &screen, const ScreenContext &context)
{
    list.clear();
    screen.draw(list, context);
}

void submit_frame(hui::gfx::Renderer &renderer, const hui::gfx::DrawList &list,
                  const hui::gfx::BackdropSpec &backdrop)
{
    renderer.begin();
    renderer.backdrop(backdrop);
    renderer.draw(list);
    renderer.glass();
}
} // namespace accord::ui
