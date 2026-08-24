# syntax=docker/dockerfile:1.7
# =============================================================================
#  ws2tcp -- the WebSocket end of the browser client.
#
#  One file, no pip step, no compiler. The bridge is standard-library Python,
#  so this image is the official Python image plus a script and a service
#  account. That is deliberate: it sits between the internet and the game
#  ports, and everything that is not in it cannot have a vulnerability.
# =============================================================================
FROM python:3.12-slim AS runtime

ENV PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1

RUN groupadd -r -g 2002 wsbridge \
 && useradd -r -u 2002 -g wsbridge -d /nonexistent -s /usr/sbin/nologin wsbridge

COPY bin/ws2tcp.py /usr/local/bin/ws2tcp.py
RUN sed -i 's/\r$//' /usr/local/bin/ws2tcp.py && chmod 0555 /usr/local/bin/ws2tcp.py

# Nothing is written anywhere: no volume, no state, no config file. Restarting
# it costs the players their connection and nothing else.
USER wsbridge

EXPOSE 7789/tcp

# No curl in this image, and none is added for one request a minute.
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD ["python3", "-c", "import os,urllib.request;urllib.request.urlopen('http://127.0.0.1:%s/health' % os.environ.get('M2_BRIDGE_LISTEN_PORT','7789'), timeout=4).read()"]

LABEL org.opencontainers.image.title="Metin2 WebSocket-to-TCP bridge" \
      org.opencontainers.image.source="https://github.com/TieruYT/metin2-playerbots"

ENTRYPOINT ["python3", "/usr/local/bin/ws2tcp.py"]
