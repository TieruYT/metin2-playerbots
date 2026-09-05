# -*- coding: utf-8 -*-
"""Package the running 1.24.9 installation as a distributable zip.

The installation is the operator's own live folder, so it carries things a
download must not: the passwords its database was built with, the path to the
client on their F: drive, the update backups the launcher keeps, and the logs.
Everything else is exactly what a player needs, which is why the package is
built from the installation rather than from the repository - the repository
does not carry the 550 MB of build context the first launch needs.
"""
import io, os, re, secrets, sys, zipfile

SRC = ("C:/Users/dawio/Downloads/Metin2_Singleplayer_Server_r40250_FIXED/"
       "Metin2_Singleplayer_Server_r40250_FIXED")
OUT = "C:/Users/dawio/Downloads/Metin2_Singleplayer_Server_r40250_FIXED_1.25.0.zip"
ROOT = "Metin2_Singleplayer_Server_r40250_FIXED"

# Local state, not part of the product.
SKIP_DIRS = {"backups", "launcher-logs", "support-bundles"}
SKIP_FILES = {".m2install.json", ".m2launcher.json", ".m2launcher-state.json"}

# Freshly generated, so the download does not carry the operator's own
# credentials. start-server.ps1 refuses to run without a .env at all, so the
# package has to ship one rather than only the example.
SECRETS = {
    "M2_DB_ROOT_PASSWORD": secrets.token_hex(24),
    "M2_DB_PASSWORD": secrets.token_hex(24),
    # Empty means the panel generates one on first start and prints it once.
    "M2_PANEL_PASSWORD": "",
    "M2_ADMINPAGE_PASSWORD": secrets.token_hex(16),
}
# The compose project name identifies one installation's volumes. Left out, so
# every download gets its own instead of all of them sharing "m2fresh".
DROP = ("M2_COMPOSE_PROJECT_NAME", "M2_CONTAINER_PREFIX")


def sanitise_env(path):
    text = io.open(path, encoding="utf-8", newline="").read()
    for key, value in SECRETS.items():
        text, n = re.subn(r"(?m)^%s=.*$" % key, "%s=%s" % (key, value), text)
        assert n == 1, "%s wystapil %d razy" % (key, n)
    for key in DROP:
        text, n = re.subn(r"(?m)^%s=.*$" % key, "# %s= (nadawane przy pierwszym uruchomieniu)" % key, text)
        assert n == 1, "%s wystapil %d razy" % (key, n)
    return text.replace("\r\n", "\n").encode("utf-8")


count = 0
total = 0
empties = []
with zipfile.ZipFile(OUT, "w", zipfile.ZIP_DEFLATED, allowZip64=True) as z:
    for dirpath, dirnames, filenames in os.walk(SRC):
        rel = os.path.relpath(dirpath, SRC).replace("\\", "/")
        if rel == ".":
            dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
            rel = ""
        # An empty directory has to be written as an entry of its own or it is
        # simply not in the archive. Eight of them exist here and the build
        # needs at least four: serverfiles/share/package, which the Dockerfile
        # COPYs outright, and the three lib/ directories the support archives
        # are assembled into. The first package shipped without them and no
        # server could be built from it.
        if rel and not filenames and not dirnames:
            z.writestr("%s/%s/" % (ROOT, rel), "")
            empties.append(rel)

        for name in sorted(filenames):
            relpath = ("%s/%s" % (rel, name)) if rel else name
            if relpath in SKIP_FILES:
                continue
            full = os.path.join(dirpath, name)
            arc = "%s/%s" % (ROOT, relpath)
            if relpath == "linux-port/docker/.env":
                z.writestr(arc, sanitise_env(full))
            else:
                z.write(full, arc)
            count += 1
            total += os.path.getsize(full)
            if count % 5000 == 0:
                print("  %d plikow..." % count, flush=True)

print("plikow: %d" % count)
print("pustych katalogow: %d" % len(empties))
for e in empties:
    print("   " + e)
print("rozmiar zrodel: %.1f MB" % (total / 1048576.0))
print("rozmiar zip:    %.1f MB" % (os.path.getsize(OUT) / 1048576.0))
print(OUT)
