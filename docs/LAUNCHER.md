# Launcher, updates and support bundles

`Metin2-Launcher-GUI.bat` is the recommended entry point for Windows testers.
It opens a native graphical window with large Polish buttons and no additional
runtime to install. `Metin2-Launcher.bat` remains available as the text-mode
fallback. The GUI can:

- prepare the package and create a desktop shortcut;
- start Docker Desktop by itself, including a cold start;
- start the server and then the selected native client with one **Play** button;
- safely stop the Compose services and Docker Desktop without deleting volumes;
- start and stop the local Docker server;
- check a release manifest;
- update server files without touching `.env` or Docker database volumes;
- update a separately unpacked native client;
- build a ZIP with Docker diagnostics and a redacted copy of `.env`;
- optionally upload that ZIP to a configured HTTPS support endpoint, but only
  after the user explicitly confirms the upload.

## First setup

For the graphical launcher, run `Metin2-Launcher-GUI.bat`, click
**Zainstaluj / przygotuj**, and select the client's executable when asked.

For text mode, run `Metin2-Launcher.bat`, choose **Configuration**, and set:

1. the HTTPS URL of `update-manifest.json`;
2. the full path to the native client directory;
3. optionally, a private Discord webhook or another HTTPS support-upload
   endpoint. Never publish the webhook URL or paste it into an issue.

The settings are stored in `.m2launcher.json`. This local file should not be
committed or included in public update ZIPs.

## Manifest format

See `launcher/update-manifest.example.json`. Each component has a version, a
direct HTTPS download URL and the exact SHA-256 of its ZIP. MEGA share pages are
not direct file URLs and are therefore unsuitable for unattended updates. Small
differential ZIPs can be attached to a GitHub Release or served from another
HTTPS endpoint that returns the file directly.

**Check for updates** is read-only: it downloads only the manifest and compares
version numbers. Files are replaced only after an explicit **Install updates**
action and confirmation. The launcher also cannot publish local developer
changes by itself. A maintainer must first build and upload an update ZIP, then
publish its URL and checksum in the configured manifest. Only files present in
that reviewed ZIP are installed.

The launcher rejects:

- HTTP downloads;
- ZIPs whose SHA-256 differs from the manifest;
- absolute paths and `..` path traversal inside ZIPs;
- attempts to overwrite `.env`, `.git`, launcher state, backups or support
  bundles.

Before overwriting a managed file it creates a timestamped copy in `backups/`.
Server updates use `docker compose up -d --build`; no `down -v` operation is
performed, so accounts, characters and bot progression remain in their volume.

## Publishing a server update

Edit `launcher/server-update-files.txt`, then run:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\New-M2UpdatePackage.ps1 `
  -Type server `
  -Version 2026.08.31.1 `
  -SourceRoot . `
  -FileList .\launcher\server-update-files.txt `
  -OutputDirectory .\release `
  -DownloadUrl 'https://github.com/TieruYT/metin2-playerbots/releases/download/v2026.08.31.1/metin2-server-update-2026.08.31.1.zip'
```

Upload the generated ZIP, then copy its generated JSON fragment into the
`server` section of the published manifest. Do not edit its SHA-256 manually.

Client patches use the same command with `-Type client`, the unpacked client as
`-SourceRoot`, and a reviewed list based on
`launcher/client-update-files.example.txt`. Never include account files,
screenshots, logs, local configuration or unrelated executables.

## Support bundles

The **Create diagnostic ZIP** action stores a file in `support-bundles/`.
Passwords, tokens and secrets from `.env` are replaced with `<redacted>`.
The five newest GUI/action logs from `launcher-logs/` are sanitized and added
as well, so startup failures can be diagnosed from a single ZIP. The GUI also
has a button that opens the current launcher log directly in Notepad.
Without an upload endpoint the tester can attach this ZIP manually on Discord.
Automatic sending is disabled until an operator configures an HTTPS endpoint
or private Discord webhook, and the launcher still asks the tester before every
upload. The webhook stays only in `.m2launcher.json`; that file is ignored and
is not added to the diagnostic ZIP.
