---
name: rewind
description: Download past NTS Radio episodes, in their original audio with tags and artwork, using the rewind command. Use when asked to save, download, archive, or list NTS (nts.live) shows or episodes for offline listening.
---

# rewind

`rewind` saves past NTS Radio episodes in their original audio format
(usually `.m4a`, never re-encoded) with artwork, tags, and a tracklist text
file. It runs yt-dlp and ffmpeg under the hood and needs no
display for these commands. Every command exits 0 on success and 1 on
failure, with errors on stderr.

List a show's episodes (newest first, 24 at a time):

    rewind episodes https://www.nts.live/shows/<show> --json
    rewind episodes <show> --offset 24 --limit 24 --json

The JSON has `name`, `total`, and `episodes[]` with `alias`, `name`, `date`,
`url`, `genres`, and `hasAudio`. Episodes with `hasAudio: false` can't be
downloaded.

Download episodes, one after another:

    rewind download <episode url>... --json
    rewind download <episode url> --output ~/Music/NTS

Files land at `<output>/<Show>/<YYYY-MM-DD> - <Episode>.m4a` (default output
`~/Music/NTS`), with the tracklist beside it as `.txt` when NTS lists one.
An episode that's already saved, in any audio format, is skipped, never overwritten
(`"skipped": true`). Each JSON result has `ok`, `path`, `tracklist`, or
`error`. Episodes run for hours, so a download can take several minutes.

`rewind open <url>` opens the window on a show or episode for the user.

The audio belongs to NTS and its hosts: this is for personal offline
listening, not for redistribution.
