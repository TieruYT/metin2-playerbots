FROM python:3.13-slim
WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
# VERSION with a star, and that star is the whole of this fix: the file is
# in .gitignore and is written by prepare-context.sh and the Windows
# launcher, so it is there on every machine that has ever run one of them
# and on no other. A fresh install that simply builds - a VPS from the
# guide, a clone, anything - had no VERSION and the whole compose bake
# stopped at "failed to compute cache key ... /VERSION: not found", taking
# the game, the panel and the itemshop down with it (superjerry925,
# 20 September, on 2.0.88 where 2.0.8 had worked). A COPY whose other
# patterns match tolerates one that matches nothing, so the file is used
# where it exists and missed where it does not - and app.py has always
# answered "dev" when it cannot read it.
COPY app.py collector.py item_grants.py gm_commands.txt CHANGELOG.md VERSION* ./
COPY templates ./templates
COPY static ./static
ENV PYTHONUNBUFFERED=1
EXPOSE 7789
CMD ["gunicorn", "--bind", "0.0.0.0:7789", "--workers", "2", "--threads", "4", "app:app"]
