/*
 * Accord - composing a frame, shared by the console title and the host runner.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Both callers do the same two things with a DrawList: fill it, then hand it to
 * the renderer. They are separate functions because the console slips overlays
 * (the on-screen keyboard, the link line) in between.
 *
 * Why the list is a parameter and never a local: Renderer::draw() keeps a
 * pointer to it and plays it only in present(), which runs after the frame has
 * been composed. A list that dies before that leaves the screen blank except for
 * the backdrop -- the one layer the renderer copies instead of points at. That
 * is exactly how the title looked on the console until it was found, while the
 * host snapshots stayed fine, because they composed and presented in one scope.
 */

#pragma once

#include "ui/kit/gfx/backdrop_spec.hpp"
#include "ui/kit/gfx/draw_list.hpp"
#include "ui/kit/gfx/renderer.hpp"
#include "ui/screens.hpp"

namespace accord::ui
{
// Clears the list and draws one screen into it. A caller may append overlays
// after this; they reach the screen with everything else.
void build_frame(hui::gfx::DrawList &list, const Screen &screen, const ScreenContext &context);

// Queues the backdrop, then the list, and marks where the frosted layer reads
// what has been drawn so far. The list must outlive the present() that follows.
void submit_frame(hui::gfx::Renderer &renderer, const hui::gfx::DrawList &list,
                  const hui::gfx::BackdropSpec &backdrop);
} // namespace accord::ui
