# Accord

Accord brings Discord to your PlayStation 5. Your servers, your channels, your messages and voice
chat — on the TV, driven with the DualSense in your hand, with no PC sitting in the middle.

Accord is an **unofficial** client. It is not made, endorsed or supported by Discord Inc., and it
is not a system-level integration built into the console. You sign in with your own Discord
account; doing that from an unofficial app can break Discord's terms of service and put the
account at risk. Whether to use it is your call.

## What Accord is meant to do

**Sign in from the couch.** Accord puts a code on your TV. You scan it with the Discord app on
your phone and approve the login there — your password is never typed on the console and never
stored on it.

**Your servers, one at a time.** The rail on the left lists the servers you have joined; open one
and its channels appear with an unread marker on the ones you have not read yet.

**Read and write.** A channel opens as a normal chat view, and a message is written with the
on-screen keyboard, so you do not need a USB keyboard plugged into the console.

**Voice channels.** Join the channel you want, see at a glance who is in it and who is talking,
mute your microphone, and leave when you are done.

**Played with the pad, not a mouse.** Moving around, going back, and writing all live on the
DualSense — including the on-screen keyboard, which opens wherever text can be typed.

**Made to be stepped away from.** The interface runs as a homebrew title, and the work that talks
to Discord was meant to live in a background service on the console, so the app could be closed and
opened again without logging in a second time.

## What you need

- A PlayStation 5 that can run homebrew. Accord has been built and tested on firmware 13.60; other
  firmware versions are simply untested — whether one works depends on the homebrew support
  available for it, not on Accord.
- A Discord account. Accord logs in as you, a normal user — not as a bot.
- Nobody watching over your shoulder while you type a message, ideally.

Accord is delivered as a `.ffpkg` package — the format homebrew titles on the console use.

## Where Accord is today

Accord is **paused**, and this section is the honest record of how far it got.

**What runs, and was checked on a console.** The interface: all six screens draw, text included,
the on-screen keyboard opens wherever text can be typed, and moving through the screens with the
DualSense works — a session walked all six and ran for minutes without a crash. A background
service starts on the console as a payload, and the title reaches it over the loopback address: its
own status line has shown `HTTP 200` from the service in a couple of milliseconds.

**What is built, but has never met Discord.** The HTTP client with TLS, the pinned JSON parser,
the reader for Discord's compressed stream, and the voice encryption library are in the tree, with
tests that run on a PC. The Discord session itself is not implemented: no login, no Gateway, no
REST, no joining a channel. The screens are mock-ups with sample data, and the login screen does
not yet log anybody in.

**Why it stopped: voice chat during a game is not supported.** That was the point of Accord, and on
firmware 13.60 the console does not allow it. Audio goes to one application, the one in the
foreground; a payload that opens an audio port is given a port but no output, and the port reads
silent whatever is played through it, on every port type that opens at all. A homebrew title cannot
run beside a game either — starting a game suspends another running application. What is left is a
narrower product, Discord between games, which did not look worth building on its own.

**What would bring it back.** A public route to audio that a background process can use, a firmware
that allows it, or a loader that does.

Development happened in the open, and the commit history is the honest record of what runs.

## Licence

GPL-3.0-or-later — see `LICENSE`. The interface kit Accord draws with is
[ps5-homebrew-ui](https://github.com/blackbearreloaded/ps5-homebrew-ui), also GPL-3.0-or-later,
which is why the whole app carries that licence. Licences of every bundled component are listed in
`THIRD_PARTY.md`.
