# What the licence covers, and what it does not

Short version: **the tooling in this repository is MIT. The game is not ours
and is not licensed by us.** Those are two separate things and it matters that
you can tell them apart.

## Covered by [LICENSE](LICENSE) (MIT)

Everything in this repository that was written for this project:

| | |
|---|---|
| `files/admin_panel.py` | the web admin panel |
| `files/items.json` | the panel's item index |
| `files/packs/*.pack` | the server-file profiles |
| `files/web_admin.quest`, `files/speed_boost.quest` | the panel's in-game bridge (compiled into the game image), and a standalone speed buff |
| `files/web_admin_schema.sql` | the two tables the panel adds |
| `installer/install.sh`, `installer/install.ps1` | the one-command installers |
| `linux-port/patches/*.patch` | the Linux port itself, and the `mysql_direct_query` quest binding |
| `linux-port/fetch-sources.sh`, `linux-port/patches/make-patch.sh` | the scripts that acquire and regenerate it |
| `linux-port/*.md` | the Linux porting write-ups |
| `linux-port/docker/` | the Docker packaging |
| `linux-port/overlays/playerbot/` | the server-side Playerbot manager, integration patches and reproducible bot seed tooling |
| `linux-port/client/serverinfo.py` | the client configuration template |
| `tools/fast-game-build/` | the optional incremental game-core build workflow |
| all `.md` documentation | |

Use it, change it, sell it, fold it into something else. Keep the copyright
notice. There is no warranty — and given that this software installs Docker,
generates database passwords and opens firewall ports on a live server, please
take that clause seriously and try it somewhere disposable first.

The Linux port work is a special case worth stating plainly: **the patches are
ours, the code they patch is not.** A diff against the game server source is
our work and is MIT. Applying it produces a derivative of code we have no
licence to, so the *result* is no more redistributable than the original.

## NOT covered

**The Metin2 game server source** (`Source/`) and **the game client and its
assets** (`Client/`, `FreeBSD/`). Metin2 is the property of **Ymir Interactive
and Webzen**. The server files this project installs are a redistributed
package that leaked in 2014 and was later reconstructed by third parties. None
of it is ours to license, and MIT does not and cannot apply to it.

That is why those directories are `.gitignore`d rather than committed — it is a
licensing decision first and a repository-size decision second.

**The third-party server-file package itself** — the `[40250] Reference
Serverfile` by TMP4. The `packs/*.pack` profiles in this repository are *our*
description of that package — where its numbers live and how its client is
assembled — not any part of the package. It is downloaded from its own
publisher's link at install time and is not hosted here. Its authors' own terms
apply to it.

**Vendored third-party libraries** inside the game source (Python 2.7, LZ4,
MariaDB Connector/C, Boost fragments, and others) carry their own licences,
which are in their own files.

## What this means in practice

- Running a Metin2 private server is, in most jurisdictions, a copyright
  infringement against Webzen regardless of what any file in this repository
  says. Plenty of people do it anyway for a small private or single-player
  server and are left alone. **That is a risk you take, not a permission this
  project grants.** Do not charge money for access, and do not pretend to be
  official.
- If you fork this repository: fork the tooling. Do not commit the game files
  into your fork — you would be redistributing Webzen's work under your own
  name, and GitHub will reject the large ones anyway.
- If you are Ymir, Webzen or a rights holder and want something here removed,
  open an issue. This is a hobby project and nothing here is worth arguing
  about.
