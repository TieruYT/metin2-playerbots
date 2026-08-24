# Contributing

Bug reports and fixes are welcome. This file is short and is mostly about one
thing: **how to change something and find out whether it works, without
breaking a live server.**

## The thing to understand first

This repository is Linux and Docker only. But `files/` is not container
plumbing — it is shared material, and one file in it is shared with a machine
this repository cannot see:

```
files/admin_panel.py            ──>  prepare-context.sh copies it into the panel image
files/packs/tmp4-r40250.pack    ──┬─>  the game image        (pack_apply_rates)
                                  ├─>  the client-builder    (pack_prepare_client)
                                  └─>  a production server outside this repository,
                                       byte for byte the same file
```

**Do not edit `files/packs/tmp4-r40250.pack`.** It carries a lot of machinery
that only the old FreeBSD installer ever called — deploying a database, patching
source, starting and stopping the server. That installer is gone from this
repository; the file is not, because it is still in use elsewhere and a diverged
copy is exactly the failure this arrangement prevents. Sourcing it has no side
effects, so the unused half costs nothing.

There is one copy of the panel and one copy of the rate arithmetic. If the
images each had their own they would drift, and nobody would notice until a
player did.

## Where to make a change

| You want to change | Edit | Affects |
|---|---|---|
| Panel behaviour or UI | `files/admin_panel.py` | both images that carry it |
| The port itself (C++) | regenerate `linux-port/patches/0001-*.patch` with `linux-port/patches/make-patch.sh` | the build |
| Container plumbing | `linux-port/docker/` | the stack |
| The one-command installers | `installer/install.sh`, `installer/install.ps1` | keep them in step — they are the same install described twice |
| Acquiring and staging the upstream source | `linux-port/fetch-sources.sh` | both installers call it |
| The in-game bridge | `files/web_admin.quest` | the game image — it is staged and compiled at build time |

The last row has a trap in it. `web_admin.quest` is compiled during the image
build by a `qc` built from the same source, and `qc` refuses any function name
that is not listed in `quest_functions` — with the message "Calls undeclared
function!" and nothing about which name it means. It also refuses helper
functions of your own, calls through a variable, and `sys_err`. The rules are
written out at the top of the quest file; keep it inside them, and rebuild the
game image (not just restart it) to test a change. Read
[files/ADD_SQL_BINDING.md](files/ADD_SQL_BINDING.md) first — particularly
section 5, which is the list of what the quest is allowed to ask the database
for.

## Testing a change

**Never test on a server people play on.** That includes the maintainer's.

### The cheap loop — the panel

The panel is a single Flask file and needs no game server to start. Point it at
a scratch database (or none at all — pages that need it degrade honestly rather
than crashing) and run it:

```sh
M2PANEL_CONF=/tmp/m2panel.conf \
M2PANEL_DB_HOST=127.0.0.1 M2PANEL_DB_USER=... M2PANEL_DB_PASS=... \
python3 files/admin_panel.py
```

Every setting has an `M2PANEL_`-prefixed environment variable, so you never have
to write a real config file — the table is in
[files/README.md](files/README.md#the-variables).

### The full loop — Docker

The honest test bed: a whole server you can throw away.

```sh
./linux-port/fetch-sources.sh   # upstream + patch + context. Needs the archive.
cd linux-port/docker
docker compose up -d --build
docker compose logs -f game
docker compose down -v          # -v also drops the database. Start clean.
```

`fetch-sources.sh` is idempotent and skips every stage whose output already
exists, so the second run costs seconds. If you are only changing the panel or
the compose file, `./prepare-context.sh` on its own is enough — it restages
`files/` into the build context without touching the source tree.

**You cannot do this without a compatible upstream archive.** It is not
distributed or downloaded automatically; pass `--archive /path/to/it` or
`--reference-dir /path/to/unpacked`.

### Shell changes

The installers, `fetch-sources.sh` and the pack profile are POSIX `sh`, not bash
(`prepare-context.sh` is the exception and says so). Before anything else:

```sh
sh -n installer/install.sh
sh -n linux-port/fetch-sources.sh
sh -n files/packs/tmp4-r40250.pack
sh -c '. files/packs/tmp4-r40250.pack'    # must print nothing and create nothing
```

That last one matters: the pack is *sourced* by two container images just to
reach two or three of its functions. A pack with a side effect fires every time
either image starts.

No bashisms — no `[[`, no arrays, no `local` unless you have checked the target
`/bin/sh`. The containers run Debian, where `/bin/sh` is dash.

### Changes to the port itself

C++ changes are not committed as files; they are committed as a regenerated
patch. Keep Linux changes inside `__linux__` guards and leave the `_WIN32` and
FreeBSD branches alone — the same source still has to build on FreeBSD for the
people running it there, and a patch that breaks them is a patch that gets
reverted. `linux-port/patches/make-patch.sh` regenerates the file; the patch
must apply with zero fuzz and zero rejects, which `fetch-sources.sh` enforces.

## Rules that exist because something went wrong once

- **Do not "fix" `tar -xf` into `tar -xzf`** in the pack or in
  `fetch-sources.sh`. Several server-file packages ship bzip2 data under a `.gz`
  name; `tar -xf` lets tar sniff the real format, `-z` forces gzip and breaks
  every install.
- **Do not repair the `-I/../../libthecore/src` path in `db/src/Makefile`.** It
  looks like a typo. It is what keeps libthecore off `db`'s include path and
  avoids a `<sys/signal.h>` shadowing trap. There is a comment saying so.
- **Verified over assumed.** Something that compiles but does not link, or runs
  but misbehaves, costs more than it saves. End every step with something
  actually executed.
- **Do not describe a feature the stack does not install.** Teleport and running
  speed are the standing example: the panel has the buttons, the quest that
  would carry them out is not deployed, and for a long time the docs said they
  merely needed the player to be online. Check what actually ships before you
  write down what it does.

## Releasing a change

Every change that reaches a user carries a version with it. This is not
ceremony: the panel checks the published `VERSION`, tells operators they are
behind, and shows them `CHANGELOG.md` so they can see what they would be
getting before they take it. A change that skips this is a change nobody is
told about.

So each release touches three things together, in the same commit:

**1. `VERSION`** — one line, `MAJOR.MINOR.PATCH`.

| Bump | When | Example |
|---|---|---|
| PATCH | a fix; nothing an operator does changes | the download returned 500 |
| MINOR | something new, or something works better than it did | the update checker |
| MAJOR | the operator has to act — a setting moves, a command goes away, a step is needed during the update | — |

When in doubt between MINOR and MAJOR, ask whether somebody who updates while
asleep wakes up to a working server. If not, it is MAJOR.

**2. `CHANGELOG.md`** — a new section at the top, dated, grouped under
`Added` / `Changed` / `Fixed` / `Security` (only the ones you need).

Write each entry for the person running the server, not for the person who
wrote the patch — what they saw, or would have seen:

> - The client download returned 500 on every request.

not

> - Fixed permissions bug in panel-data volume

One line each. **No reasoning, no design discussion, no explaining why one
approach was taken over another** — that belongs in the commit message, where
the people who need it will look. The changelog answers "does this affect me?"
and nothing else.

If the release needs a manual step, it goes **at the top of the section**, in
full, before anything else.

**3. The commit message.** Subject line: what changed, in the imperative, under
~70 characters. Then a blank line, then why — the symptom, the cause, and
anything the next person would otherwise have to rediscover. The commits worth
imitating are the ones that answer "why on earth is this like this?" a year
later.

    Fix client download failing with 500 on every request

    The panel runs as uid 2001 but its data directory was owned by root, so
    sqlite3 could not create downloads.db and every request to /download
    raised. The client itself built fine, which made this look like a
    download problem rather than a permissions one.

    Docker seeds an empty named volume from the image directory it is first
    mounted on, ownership included, and the client builder is usually first
    to write into that volume -- so /out in the builder image now belongs to
    2001:2001 as well.

## Never commit

- Real IPs, hostnames, passwords, passphrases or SSH keys. Use `203.0.113.10`
  and `example.com` in documentation. If you are writing up a real deployment,
  sanitise it — `linux-port/VPS-DEPLOYMENT.md` is the pattern to copy.
- `.env`, `m2panel.conf`, `panel.env` or anything else generated on a real box.
  The root `.gitignore` covers these; do not `-f` past it.
- Game files (`Client/`, `Source/`, `FreeBSD/`), the client `.zip`, or any
  archive of them. Not ours to publish, and too big regardless — see
  [NOTICE.md](NOTICE.md).

If a secret does get committed, say so immediately. Rotating a password is
cheap; a password sitting in git history is not fixed by deleting it in a later
commit.

## Style

Match what is there. The docs are written plainly, in full sentences, and admit
what does not work — that is a deliberate choice, not an accident, and it is
worth more than polish. Comments explain *why*, especially where the code looks
wrong and is not.
