# Removing it, and starting over

Sometimes the quickest way forward is a clean machine. This page removes an
installation completely and puts a fresh one in its place.

Read the first section before you delete anything. An installation is spread
over a few places, and knowing which is which is the difference between "I
started over" and "I lost every character on the server".

---

## What an installation is made of

| Where | What is in it | Roughly |
| --- | --- | --- |
| `/opt/metin2/stack` | `docker-compose.yml`, your `.env`, and the staged server tree the images are built from | 2 GB |
| Docker volumes named `metin2_*` | **the database**, the panel's settings, the browser client, the built desktop client | 1–5 GB |
| Docker images named `metin2/*` | game, panel, bridge, and the two builders | 1 GB |
| `/var/cache/m2src` | the downloaded server-file archive and this project's checkout | 3.5 GB |
| `/etc/nginx/conf.d/metin2.conf`, `metin2-cloudflare.conf` | the website in front of the panel | tiny |
| `/etc/nginx/ssl/<your domain>` | your certificate | tiny |
| `/root/.acme.sh`, `/var/www/acme` | the Let's Encrypt client that renews it, and its challenge folder | tiny |

Everything on that list can be downloaded or rebuilt again — **except the
database.** Accounts, characters, items, guilds and the panel's own settings
live in the `metin2_db-data` volume and exist nowhere else.

---

## Before you delete: save the database

Skip this only if you genuinely want every character gone.

```sh
cd /opt/metin2/stack
docker compose exec -T mariadb \
  mariadb-dump -uroot -p"$(grep M2_DB_ROOT_PASSWORD .env | cut -d= -f2)" \
  --databases account player common log hotbackup \
  | gzip > ~/metin2-backup-$(date +%F).sql.gz
ls -lh ~/metin2-backup-*.sql.gz
```

Copy that file off the server before you carry on. A backup on the disk you are
about to wipe is not a backup.

---

## Removing it

### 1. Stop everything and destroy the volumes

```sh
cd /opt/metin2/stack
docker compose down -v --remove-orphans
```

`-v` is the important part: without it the containers go and the database
stays, and the next install picks the old one back up — which is a fine way to
keep your characters, and a confusing one if you wanted a clean slate.

Check that nothing survived:

```sh
docker volume ls | grep metin2 || echo "no volumes left"
```

### 2. Remove the images

```sh
docker rmi $(docker images 'metin2/*' -q) 2>/dev/null
docker image prune -f
```

The base images (`mariadb`, `ubuntu`, `alpine`) are left alone. They cost a few
hundred MB and save the next install from downloading them again.

### 3. Remove the files on disk

**Leave the directory first.** You are standing in `/opt/metin2/stack` from the
backup step, and you are about to delete it.

```sh
cd /
rm -rf /opt/metin2
rm -rf /var/cache/m2src /var/cache/m2webclient
```

Skip that `cd /` and nothing appears to go wrong — the shell keeps printing the
old directory name, because Linux keeps the directory alive for as long as a
process is standing in it. It is the *next* command that fails, and it fails in
a way that points nowhere near the cause:

```
fatal: Unable to read current working directory: No such file or directory
fatal: remote helper 'https' aborted session
```

That is a shell in a directory that no longer has a path, not a network fault.
`cd /` and run the command again. Installers from 1.12.2 onwards notice this
themselves and carry on.

That is the ~5.5 GB. If you are reinstalling straight away and your connection
is slow, keep `/var/cache/m2src` — the next install reuses the archive in it
instead of downloading it again, and it is checked against its checksum before
it is trusted.

### 4. Remove the website and the certificate

Replace `example.com` with the domain you installed with.

```sh
rm -f /etc/nginx/conf.d/metin2.conf /etc/nginx/conf.d/metin2-cloudflare.conf
rm -rf /etc/nginx/ssl/example.com
nginx -t && systemctl reload nginx
```

**If you are reinstalling on the same domain, stop here.** Keep the certificate
and keep `/root/.acme.sh`. Let's Encrypt allows five certificates per week for
the same name, and reinstalling a few times in an afternoon is a good way to
find that out the hard way. A kept certificate is simply reused.

Only if you are done with the domain for good:

```sh
/root/.acme.sh/acme.sh --remove -d example.com
/root/.acme.sh/acme.sh --uninstall     # also removes its cron entry
rm -rf /root/.acme.sh /var/www/acme
```

### 5. Docker itself

The installer installs Docker if it is not there. Removing it is almost never
what you want — but if this machine is going back to being something else:

```sh
apt-get purge -y docker-ce docker-ce-cli containerd.io docker-compose-plugin
rm -rf /var/lib/docker
```

> Do not reach for `docker system prune -a --volumes` to shortcut any of this.
> It deletes volumes belonging to **everything else** on the machine too, and
> people discover that afterwards.

---

## Installing again

```sh
curl -fsSL https://raw.githubusercontent.com/TieruYT/metin2-playerbots/main/installer/install.sh \
  | sudo sh -s -- --archive /path/Reference_Server.zip --no-client \
      --domain example.com --email you@example.com
```

Leave off `--domain` and `--email` for a server reachable by IP address only;
you then get plain HTTP and no certificate.

The installer does not download game files. Supply the r40250 server baseline
yourself and use a compatible native Windows client.

### Putting your characters back

```sh
cd /opt/metin2/stack
gunzip -c ~/metin2-backup-2026-08-12.sql.gz | docker compose exec -T mariadb \
  mariadb -uroot -p"$(grep M2_DB_ROOT_PASSWORD .env | cut -d= -f2)"
docker compose restart game
```

Restore into a stack that is already running and healthy, not into one that is
still building.

---

## The smaller hammer

A full wipe is rarely necessary. Three cheaper things, in order:

**Rebuild the server from files already on the machine.** Nothing is
downloaded; the tree is staged again and the image rebuilt.

```sh
cd /opt/metin2/stack
sh /var/cache/m2src/repo/linux-port/fetch-sources.sh --force restage
docker compose build game && docker compose up -d game
```

**Download everything again as well**, when you suspect the archive itself:

```sh
sh /var/cache/m2src/repo/linux-port/fetch-sources.sh --force redownload
```

Both operations restart the game, which disconnects everyone who is playing. Say so
in advance if anybody is.

---

## If you changed the server files by hand

Anything you edited under `/opt/metin2/stack/game/src/serverfiles/share` — drop
tables, shop contents, `item_proto.txt` — or in the server's C++ source is
**staged from the archive again** by `--force restage`, `--force redownload`
and by a reinstall. It is overwritten without a warning, because from the
outside it looks like a file that was never touched.

Keep a copy of your edits somewhere outside `/opt/metin2` and `/var/cache`, or
better, keep the script that produced them. A diff you can replay survives
everything on this page; a hand-edited file does not.
