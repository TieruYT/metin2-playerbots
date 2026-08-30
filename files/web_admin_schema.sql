-- =============================================================
-- Metin2 Admin Panel — the two tables the panel adds to the game
--
-- These are NOT part of any stock Metin2 server files. The panel
-- container applies this file to the player database on every start,
-- and anyone with only a mysql client can do the same:
--
--   mysql -u metin2 -p player < web_admin_schema.sql
--
-- Running it twice changes nothing — every statement is idempotent.
-- It is written for the conventional layout where the character
-- database is called "player". If yours is called something else,
-- point the client at that database instead (the file never names
-- a schema itself, on purpose).
--
-- EN: safe to run again at any time.
-- DE: kann jederzeit erneut ausgeführt werden.
-- TR: istediğin zaman tekrar çalıştırabilirsin.
-- =============================================================

-- ---- the command queue ------------------------------------------------
-- The panel writes one row per admin action; the in-game quest
-- (web_admin.quest) polls it and carries the action out on the player.
--
-- 'status' holds pending / done / player_offline / unknown_cmd / bad_args /
-- cancelled, and ALSO the short claim token a game core stamps in while it
-- takes ownership of a row. That token is why the column is 24 characters
-- and not 8 — a truncated token would collide between cores and the same
-- action would be delivered twice.
--
-- Every core polls filtered on status, hence the index.
--
-- NOTE: CREATE TABLE IF NOT EXISTS deliberately leaves an OLDER table alone.
-- A queue table from before the claim token existed (status was narrower, no
-- index) is NOT upgraded by this file and nothing in this repository upgrades
-- it either — if you are pointing the panel at a database from an older
-- install, widen player.web_admin_queue.status to VARCHAR(24) and add the
-- status index yourself. Only plain SQL that every MySQL and MariaDB accepts
-- lives in here, so this file can be fed to a fresh database by anything,
-- including a container entrypoint that has no shell logic to fall back on.
CREATE TABLE IF NOT EXISTS web_admin_queue (
  id INT AUTO_INCREMENT PRIMARY KEY,
  player_name VARCHAR(24) NOT NULL,
  cmd VARCHAR(16) NOT NULL,
  arg1 VARCHAR(64), arg2 VARCHAR(64),
  status VARCHAR(24) DEFAULT 'pending',
  created DATETIME DEFAULT CURRENT_TIMESTAMP,
  KEY status_idx (status)
);

-- ---- server-wide rates ------------------------------------------------
-- What the panel's "Server rates" page wants; apply_rates.sh reads it back
-- and hands it to the server-files profile. The project deliberately starts
-- with 650% EXP so the bot population can progress beyond M2 in a reasonable
-- test cycle; drop and Yang retain their neutral fresh-install defaults.
CREATE TABLE IF NOT EXISTS web_admin_rates (
  name  VARCHAR(24) PRIMARY KEY,
  value INT NOT NULL DEFAULT 100
);

INSERT IGNORE INTO web_admin_rates (name, value)
  VALUES ('exp', 650), ('drop', 100), ('yang', 100);
