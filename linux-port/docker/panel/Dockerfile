# syntax=docker/dockerfile:1.7
# =============================================================================
#  Metin2 admin panel -- Flask + PyMySQL.
#
#  Single stage: it is a Python application with two pure-Python dependencies,
#  so there is nothing to compile and nothing a builder stage would strip out.
#
#  The panel is the one public HTTP surface in this stack and the only piece a
#  non-technical operator interacts with daily, so it gets a real WSGI server
#  (waitress) rather than Flask's development server.
# =============================================================================
FROM python:3.12-slim AS runtime

ENV PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    PIP_NO_CACHE_DIR=1

# mariadb-client: the entrypoint applies the panel's schema (the two non-stock
# tables it needs) at every start, so the panel works against a database that
# already existed as well as a freshly initialised one.
# curl: the healthcheck.
RUN apt-get update \
 && apt-get install -y --no-install-recommends mariadb-client curl \
 && rm -rf /var/lib/apt/lists/*

RUN pip install --no-cache-dir flask PyMySQL waitress

RUN groupadd -r -g 2001 panel && useradd -r -u 2001 -g panel -d /opt/panel -s /usr/sbin/nologin panel

# The spool group: shared with the containers on the other side of the panel's
# two request/response directories -- the game container (rates) and, when the
# operator has switched it on, the updater (updates).  Both run as a different
# user, and the gid must be the same number in every image -- the kernel checks
# the number, not the name.  (See game/Dockerfile, which creates the same group
# and puts its own service account in it.)
RUN groupadd -r -g 2050 m2spool && usermod -a -G m2spool panel

WORKDIR /opt/panel

# app/ is staged by ../prepare-context.sh from the panel source directory. That
# includes VERSION and CHANGELOG.md, copied there from the repository root --
# see the "version" block below for why they matter.
COPY app/ /opt/panel/
COPY schema/ /opt/panel/schema/

# ---- which build is this? ---------------------------------------------------
# The panel reports its own version and compares it with the one published in
# the repository, so it has to know it, and a container cannot look it up: the
# repository is not in the image and there is no network at build time worth
# trusting for it. So the number is baked in here, from the file staged into the
# context, and is fixed for the life of the image -- which is exactly right,
# because the image IS the version.
#
# M2_PANEL_VERSION overrides the file, for building a test image that claims to
# be some other version:
#     docker compose build --build-arg M2_PANEL_VERSION=1.0.0 panel
#
# If neither is available the file is simply absent and the panel says
# "unknown". It never guesses, and it never treats unknown as up to date.
ARG M2_PANEL_VERSION=""
RUN if [ -n "$M2_PANEL_VERSION" ]; then \
        printf '%s\n' "$M2_PANEL_VERSION" > /opt/panel/VERSION ; \
    fi ; \
    printf 'panel version: %s\n' "$(cat /opt/panel/VERSION 2>/dev/null || echo unknown)"
LABEL org.opencontainers.image.title="Metin2 admin panel" \
      org.opencontainers.image.source="https://github.com/TieruYT/metin2-playerbots"
COPY bin/entrypoint.sh /usr/local/bin/entrypoint.sh
# The panel starts this one itself, through M2PANEL_RATES_SCRIPT, whenever
# somebody saves the rates page.  It lives in /usr/local/bin and NOT on the
# data volume on purpose: a stack that is upgraded keeps its old volume, and a
# file placed there by an image would never appear.
COPY bin/apply_rates.sh /usr/local/bin/apply_rates.sh
RUN sed -i 's/\r$//' /usr/local/bin/entrypoint.sh /usr/local/bin/apply_rates.sh \
 && chmod 0755 /usr/local/bin/entrypoint.sh /usr/local/bin/apply_rates.sh

# Two separate volumes, because they have completely different lifecycles:
#
#   /usr/local/etc      m2panel.conf -- generated once at first run and then
#                       never regenerated, because it holds the password hash
#                       and the Flask secret. Regenerating it would log every
#                       operator out and invalidate every session cookie.
#
#   /usr/local/m2panel  client.zip (the ~1.2 GB game client the panel serves to
#                       players), downloads.db (the per-IP download quota),
#                       rates.status. client.zip is far too big to bake into an
#                       image and is not ours to redistribute.
RUN mkdir -p /usr/local/etc /usr/local/m2panel \
 && chown -R panel:panel /usr/local/etc /usr/local/m2panel /opt/panel
VOLUME ["/usr/local/etc", "/usr/local/m2panel"]

EXPOSE 7788/tcp

HEALTHCHECK --interval=20s --timeout=5s --start-period=40s --retries=3 \
    CMD curl -fsS -o /dev/null "http://127.0.0.1:${M2_PANEL_PORT:-7788}/" || exit 1

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
