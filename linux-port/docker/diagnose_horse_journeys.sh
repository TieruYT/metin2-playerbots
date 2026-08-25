#!/usr/bin/env bash
set -euo pipefail

# Read-only audit for the complete Playerbot horse journey:
# M1 -> M2 -> easy Monkey Dungeon -> real medal pickup -> Stable Boy -> riding.
# Run from linux-port/docker while the stack is online.

docker exec -i metin2-db sh -c \
    'mariadb -u "${M2_DB_USER}" -p"${M2_DB_PASSWORD}" --table player' <<'SQL'
SELECT
    COUNT(*) AS bot_records,
    MIN(id) AS minimum_id,
    MAX(id) AS maximum_id
FROM player
WHERE name LIKE 'bot%';

SELECT
    CASE map_index
        WHEN 21 THEN 'Chunjo M1'
        WHEN 23 THEN 'Chunjo M2'
        WHEN 24 THEN 'Chunjo M3 / Waryong'
        WHEN 25 THEN 'Easy Monkey Dungeon'
        ELSE CONCAT('map ', map_index)
    END AS location,
    COUNT(*) AS bots
FROM player
WHERE name LIKE 'bot%'
GROUP BY map_index
ORDER BY map_index;

SELECT
    SUM(horse_level > 0) AS bots_with_horse,
    SUM(horse_level >= 11) AS bots_with_combat_horse,
    SUM(horse_riding > 0) AS bots_currently_riding,
    (SELECT COUNT(DISTINCT i.owner_id)
       FROM item i JOIN player owner ON owner.id = i.owner_id
      WHERE owner.name LIKE 'bot%' AND i.vnum = 50050) AS bots_carrying_medal,
    (SELECT COUNT(*)
       FROM quest q JOIN player owner ON owner.id = q.dwPID
      WHERE owner.name LIKE 'bot%' AND q.szName = 'playerbot'
        AND q.szState = 'horse_medals_looted' AND q.lValue > 0) AS bots_with_logged_loot,
    (SELECT COUNT(*)
       FROM quest q JOIN player owner ON owner.id = q.dwPID
      WHERE owner.name LIKE 'bot%' AND q.szName = 'playerbot'
        AND q.szState = 'horse_medals_delivered' AND q.lValue > 0) AS bots_with_logged_delivery
FROM player
WHERE name LIKE 'bot%';

SELECT
    p.id,
    p.name,
    p.level,
    p.map_index,
    p.x,
    p.y,
    p.horse_level,
    MAX(CASE WHEN q.szName = 'make_herb_lv25' AND q.szState = '__status'
             THEN q.lValue ELSE NULL END) AS bio25_status
FROM player p
LEFT JOIN quest q ON q.dwPID = p.id
WHERE p.name LIKE 'bot%'
GROUP BY p.id, p.name, p.level, p.map_index, p.x, p.y, p.horse_level
ORDER BY p.level DESC, p.name
LIMIT 40;

SELECT
    p.id,
    p.name,
    p.level,
    p.map_index,
    p.x,
    p.y,
    p.horse_level,
    p.horse_riding,
    COALESCE(medals.count, 0) AS medals_in_inventory,
    COALESCE(looted.lValue, 0) AS medals_looted_by_ai,
    COALESCE(last_map.lValue, 0) AS last_medal_map,
    CASE WHEN COALESCE(last_loot.lValue, 0) = 0 THEN NULL
         ELSE FROM_UNIXTIME(last_loot.lValue) END AS last_medal_loot,
    COALESCE(delivered.lValue, 0) AS medals_delivered,
    CASE WHEN COALESCE(last_delivery.lValue, 0) = 0 THEN NULL
         ELSE FROM_UNIXTIME(last_delivery.lValue) END AS last_delivery
FROM player p
LEFT JOIN (
    SELECT owner_id, SUM(count) AS count
    FROM item
    WHERE vnum = 50050
    GROUP BY owner_id
) medals ON medals.owner_id = p.id
LEFT JOIN quest looted
    ON looted.dwPID = p.id
   AND looted.szName = 'playerbot'
   AND looted.szState = 'horse_medals_looted'
LEFT JOIN quest last_map
    ON last_map.dwPID = p.id
   AND last_map.szName = 'playerbot'
   AND last_map.szState = 'horse_last_loot_map'
LEFT JOIN quest last_loot
    ON last_loot.dwPID = p.id
   AND last_loot.szName = 'playerbot'
   AND last_loot.szState = 'horse_last_loot_time'
LEFT JOIN quest delivered
    ON delivered.dwPID = p.id
   AND delivered.szName = 'playerbot'
   AND delivered.szState = 'horse_medals_delivered'
LEFT JOIN quest last_delivery
    ON last_delivery.dwPID = p.id
   AND last_delivery.szName = 'playerbot'
   AND last_delivery.szState = 'horse_last_delivery_time'
WHERE p.name LIKE 'bot%'
  AND (p.map_index IN (23, 24, 25)
       OR p.horse_level > 0
       OR COALESCE(medals.count, 0) > 0
       OR COALESCE(looted.lValue, 0) > 0
       OR COALESCE(delivered.lValue, 0) > 0)
ORDER BY p.horse_level DESC, p.map_index DESC, p.level DESC, p.name;
SQL
