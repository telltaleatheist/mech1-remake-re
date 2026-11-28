# MechWarrior 3 Rebuild 1.7.0 (2026-11-27)

Getting the game files is easier, and a crash at quit is fixed. Nothing in
play changes.

## New

- **Started without its game files, MechWarrior 3 offers to download them.**
  When you start the game on its own (not from the launcher) and it has no
  game files yet, it looks for a copy at once. If it finds none, it asks
  whether to download the original game files (the US disc's, from the
  Internet Archive or reverse-engineered.com), with DOWNLOAD THE GAME FILES, PICK MY OWN COPY and PICK
  A FOLDER. The download is checked against its checksum, installed, then
  deleted. A copy it finds is offered as before, and the download is not
  mentioned.
- **A MechWarrior 3 disc in a drive is found.** When no installed copy is
  found, the game's search offers the disc in your CD or DVD drive, and so
  does the launcher, so nothing is downloaded that your disc already has. A
  Pirate's Moon disc is not offered for MechWarrior 3.
- **A compact copy of the original game files.** reverse-engineered.com hosts
  a smaller copy of the original files (310 MB against the disc image's
  625 MB), which the launcher, or the game started on its own, downloads when
  the Internet Archive cannot be reached. It leaves out only files the rebuild never reads (the software
  renderer's textures and Windows' own libraries) and keeps the movies as
  VP9 and FLAC. The setup checks every file before it installs anything, and
  the first start converts the movies as it does a disc's. A game installed
  from it is the same as one installed from the disc, file for file; in the
  movies the encoding differs by less than you can see.

## Fixed

- **A dropped download picks up where it stopped** instead of starting over:
  it is tried again 5 seconds later, up to five times while each try gets
  further.
- **A crash at quit after the sound output changed.** The game now uses SDL
  3.4.20, which fixes a crash on quitting after the computer's default sound
  output changed while it ran (headphones or a headset plugged in or out).
  The launcher 1.8.0 crashed this way; the games get the same fix.

## Versions

- MechWarrior 3: 1.7.0. Your pilots, saves, settings, mods and game files are
  kept.
- The launcher offers the compact copy to MechWarrior 3 1.7.0 or newer: after
  updating the launcher, press UPDATE ALL.
