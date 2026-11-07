# Rewind

![Rewind downloading two Floating Points episodes](docs/screenshot.png)

Rewind saves past NTS Radio episodes as MP3s for offline listening. Paste an
nts.live show link to browse its archive, or an episode link to download it
straight away. Each episode is saved with its artwork, tags (title, show,
date, genres), and a tracklist text file when NTS lists one.

```
~/Music/NTS/Floating Points/2025-12-15 - David Wrench.mp3
~/Music/NTS/Floating Points/2025-12-15 - David Wrench.txt
```

Downloads run one at a time. Episodes already saved are never overwritten,
and an unfinished download never carries the real file name.

It uses NTS's public API for shows, episodes, and tracklists, `yt-dlp` to
fetch the audio from wherever NTS hosts it (SoundCloud or Mixcloud), and
`ffmpeg` to write the MP3. The audio belongs to NTS and its hosts; Rewind is
for personal offline listening, not redistribution.

## Shortcuts

| Key | Action |
| --- | --- |
| `Ctrl+L` | Paste or type a link |
| `Enter` | Open the link |
| `J` / `K` | Move through episodes |
| `Space` | Download the highlighted episode |
| `A` | Download every listed episode that isn't saved yet |
| `M` | Load more episodes |
| `X` | Cancel the running download |
| `R` | Retry failed downloads |
| `C` | Clear finished downloads |
| `O` | Open the download folder |
| `,` | Change the download folder |
| `Esc` | Leave the link field |
| `?` | Show or hide shortcuts |
| `Q` | Quit (asks first while downloading) |

## Command line

```sh
rewind open https://www.nts.live/shows/floating-points
rewind episodes floating-points --limit 10 --json
rewind download https://www.nts.live/shows/<show>/episodes/<episode> --output ~/Music/NTS --json
rewind skill install   # teaches coding agents to use the commands above
```

Each command exits 0 or 1, with errors on stderr.

## Build

Needs `qmake6`, Qt 6 (base, declarative, wayland), `yt-dlp`, and `ffmpeg`,
all part of Omarchy or one `omarchy-pkg-add` away.

```sh
bin/build     # build/rewind
bin/test      # Qt Test, offscreen
bin/install   # makepkg -fsi, so it shows up in the app launcher
```

Rewind follows the Omarchy theme accent and window rounding, and the
language picked under Setup > Language (English, Danish, German, and
Mexican Spanish so far).

## License

MIT.
