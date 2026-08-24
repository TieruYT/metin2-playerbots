# Metin2 r40250 — the Linux port

The r40250 game server, built and running on **Linux** instead of FreeBSD, so it
runs on any ordinary VPS — and, because it is now an ordinary Linux binary, in
Docker.

**It is finished.** The stack in [`docker/`](docker/) — `mariadb`, `game`,
`panel`, plus a `client-builder` that only runs when you ask for it — has been
running on a Debian VPS and was verified again on Windows and in WSL. A real
client logs in and plays.

The FreeBSD installer in [`../files/`](../files/) is untouched and stays
supported. This is a second path, not a replacement.

---

## What is actually in this directory

Nothing copyrighted, and that is the whole design. The port itself is **one
patch**:

| | |
|---|---|
| [`patches/0001-r40250-linux-port.patch`](patches/0001-r40250-linux-port.patch) | 119 KB, **30 files**, 68 hunks, +1999 / −40. Every source change made to the server: the port, plus one clearly-labelled feature — see *WHAT IS NOT PART OF THE PORT* in the patch's own header. |
| [`build-deps-40250.sh`](build-deps-40250.sh) | Builds the 32-bit dependency tree the r40250 Makefiles expect (Crypto++ 8.4.0, DevIL 1.8.0, MariaDB Connector/C 3.3.10, OpenSSL, zlib, libmd, and 40250's own forked Lua 5.0.3). |
| [`fetch-sources.sh`](fetch-sources.sh) | Turns a clean checkout into a buildable Docker context: obtains the upstream archive, extracts source + `share/` + SQL dumps, applies the patch, runs `docker/prepare-context.sh`. |
| [`docker/`](docker/) | The deployment. See [docker/README.md](docker/README.md). |
| [`client/serverinfo.py`](client/serverinfo.py) | Client-side config template. Put your own address in it. |

The r40250 source, its `share/` data tree and its SQL dumps belong to
Ymir/Webzen and to whoever assembled the server-file package. None of it is
here and none of it ever will be.

**There is no release tarball and there never will be one.** A checkout plus
`fetch-sources.sh` is the only way to get a buildable tree.

### The patch is provably the whole of it

A fresh extraction of the upstream archive plus this patch is **byte-identical**
to the tree the running server was built from: the sorted per-file checksum
manifest matches, `patch` exits 0 with zero fuzz and zero rejects, and the
result compiles to a 32-bit ELF that links `epoll_*` and contains no `kqueue`
reference at all. The patch header names the baseline archive's sha256 and the
manifest hash so anyone can repeat that check.

## Building and running it

```sh
./fetch-sources.sh --archive /path/Reference_Server.zip
cd docker && cp .env.example .env        # then edit .env
docker compose up -d --build
```

`installer/install.sh` (Linux, public) and `installer/install.ps1` (Windows,
loopback only) do all of that for you, including installing Docker.

> The repository does not distribute or automatically download the r40250
> baseline. Pass `--reference-dir`, `--archive`, or an explicit `--url` that
> you are authorised to use.

## Known limits

- **The panel's Teleport and Running speed need a character who is in game.**
  They work by writing a row into `player.web_admin_queue` for an in-game quest
  to pick up, and there is no way to fake either one in the database. The quest
  (`files/web_admin.quest`) and the `mysql_direct_query` binding it needs are
  both installed by the build, but a character who is offline cannot be moved or
  sped up by anything, so the panel refuses those two and says why. Items, yang
  and level work either way — in game if the helper answers, straight to the
  database if it does not. Detail in
  [docker/README.md → What the panel can and cannot do here](docker/README.md#what-the-panel-can-and-cannot-do-here)
  and [files/ADD_SQL_BINDING.md](../files/ADD_SQL_BINDING.md).
- **`M2_INVENTORY_SLOTS` defaults to 45**, which is one inventory page; r40250
  has four. Set it to `180` in `.env` or the panel will decide a character's
  inventory is full when it is not.
- **32-bit only.** Moving to 64-bit would change the size of packed structs that
  are simultaneously wire format and database layout, so it is not a flag change
  and is not planned.

## Why

The FreeBSD installer works well but only on hosts that offer FreeBSD.
Providers like Strato, 1blu and Contabo offer neither FreeBSD images nor nested
virtualisation on their normal VPS products, and "upload your own ISO" is beyond
what a non-technical user will do. A Linux build removes that constraint at the
root — and only then does a `docker compose up` deployment become possible
(Docker containers share the host kernel, so a FreeBSD binary can never run in a
container on a Linux host; that is why containerising the FreeBSD server is
impossible).

## Why this looked feasible enough to try

The audit that opened the project found the codebase **already multi-platform**:

| Finding | Meaning |
|---|---|
| A complete `_WIN32` port exists (~41 files) | Platform abstraction boundaries are already in the code. Linux is the *third* platform, not the second. |
| `kqueue` is confined to **one file** (`libthecore/src/fdwatch.c`) | That file was already split into `#ifdef _WIN32` (select-based) / `#else` (kqueue). A Linux branch slots into the existing structure. |
| `__FreeBSD__` appears in a handful of files | Very little BSD-specific conditional code. |
| `setproctitle`, `sysctl` unused | Two classic porting headaches absent. |
| Real `Makefile`s exist per module | No Visual-Studio-only build to reverse-engineer. |

All of that held. The awkward part was `-m32`: the server builds 32-bit, which
needs multilib and 32-bit builds of every dependency. Moving to 64-bit was
tempting but risky for this code (packed structs that are simultaneously wire
format and database layout), so 32-bit it stayed — and stays. See
[PORT40250.md](PORT40250.md#standing-rule-never-define-_time_bits64).

## Ground rules the port was held to

1. **The FreeBSD build must keep working.** Every Linux change sits inside
   `__linux__` guards or `uname -s` build logic; the `_WIN32` and FreeBSD
   branches were not touched. Demonstrated, not asserted — see
   [PORT40250.md](PORT40250.md#freebsd-build-proven-untouched). The one feature
   in the patch (`M2_FEATURE quest-sql-binding`) is deliberately *not* guarded:
   it is not platform-specific, and hiding it behind `__linux__` would mean the
   two platforms no longer built the same server.
2. **Verified over assumed.** A library that compiles but does not link, or an
   event loop that builds but misbehaves, costs more than it saves. Every step
   ended with something actually executed.
3. **No silent workarounds.** A blocker got reported, not hacked around.

## The documents here

Two are current. The rest are records of how it was done, kept because the
findings in them cost real time to get.

| | |
|---|---|
| [docker/README.md](docker/README.md) | **Current.** How to run the server. Start here. |
| [PORT40250.md](PORT40250.md) | **Current.** What the port does to the r40250 source, and why. The companion to the patch. |
| [FDWATCH-BUG.md](FDWATCH-BUG.md) | The defect that blocked login, and how it was found. The single most instructive thing in the project. |
| [VPS-DEPLOYMENT.md](VPS-DEPLOYMENT.md) | A sanitised worked example of a real deployment, kept for the problems it ran into. |
| [PLAYTEST.md](PLAYTEST.md) | The first successful client login, and how to play the Docker stack today. |
| [RUNTIME.md](RUNTIME.md) | Record of preparing the r40250 runtime tree and database by hand. Its findings are what the Docker images automate. |

The port's first attempt targeted a *different* Metin2 fork before the target
was corrected to r40250. Its method and its engineering findings carried over
into the documents above; its working notes are not kept here, because every
file name, line number and dependency version in them belongs to the other fork
and would mislead anyone building this one.

## The two diagnoses that turned out wrong

Recorded because both would have cost real time, and both were caught by
insisting on execution over reasoning. Both survived the change of target and
are in the shipped patch.

1. **`optreset = 1` → `optind = 0` was wrong.** BSD `getopt()` uses `optreset`
   to drop its saved intra-element scan pointer; glibc has no such variable.
   The obvious translation, `optind = 0`, means something else entirely to
   glibc — *reinitialise and rescan from `argv[1]`*, not "resume here".
   Reproducing the argument loop standalone showed a **runaway loop**: the
   server would have hung at startup with no error message. A dummy
   `int optreset;` is the other trap — it compiles, links, and does nothing.
   The correct translation is to drop the reset entirely, because glibc's
   `getopt()` re-reads `argv[optind]` on its own and therefore honours the
   manual `optind++` the code already does.
2. **`-lssl -lcrypto` are not droppable.** No source file uses OpenSSL
   directly, but the static MariaDB Connector/C bundles `openssl.c.o` /
   `ma_tls.c.o` and leaves dozens of undefined OpenSSL symbols. Linking both
   ways proved it.

Also reversed, and worth keeping straight because the two forks differ here:
in the fork audited first, a typo'd `-I/../../libthecore/src` in `db/src/Makefile`
looked like an obvious thing to fix, and fixing it would have walked straight
into a header-shadowing trap — glibc's `<sys/signal.h>` is literally
`#include <signal.h>`, which with libthecore on the bracket-include path
resolves to libthecore's *own* `signal.h` and makes `SIGPIPE`, `signal()` and
`sigaction()` vanish. **r40250 has no such line at all**, so there is nothing to
preserve there — only something not to add. The ported `db/src/Makefile` carries
a comment saying so.
