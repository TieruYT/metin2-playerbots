-- Four game masters on the tester account, one per class, fully equipped.
--
-- The mt2009 package ships an empty gmlist and no character on `admin'.
-- This file creates Admin (warrior), AdminNinja, AdminSura and AdminSzaman
-- at level ninety, each with IMPLEMENTOR rights, the best +9 set the
-- CLIENT can show for its class with strong bonus lines, a second weapon,
-- a bag of potions and scrolls, the Oil of Banishment (71054, changes the
-- kingdom), and a level-21 horse with its summon book (50053, what the
-- stable keeper hands out for a grade-3 horse).
--
-- "The best the client can show" is not "the best item_proto has": the
-- server's proto goes to level 90 (Pancerz Diabelskiego Rogu, shape 13;
-- Zbroja z Niebieskiej Stali, shape 26) and to level-87 weapons (Runiczny
-- Miecz 460...), but the client's pc2/*.msm know shapes 0-12, 14-22 and 24
-- only and its item pack carries weapon models up to the level-75 set
-- (00180, 00190, 01130, 02170, 03160, 05120, 07180). A character in an
-- armour the client cannot draw is invisible and cannot move (Iwakura,
-- 11 September, on 2.0.4's set). So: the level-66 "black" armour (shape 12)
-- and the level-75 weapons, and the repair block at the end swaps them in
-- on a world that already has the four with the old set.
--
-- Runs on a fresh world from initdb.d (10-import-dumps.sh) and on every
-- start from apply.sh. Until 2.0.8 it did nothing unless the admin account
-- had no character at all, so the operator who had made his own character
-- on it before the four existed never got them ("na moim koncie admin nie
-- ma postaci GM, tylko moja Tieru"). Now it fills the account's free slots
-- (an account holds four characters) with the classes the account does
-- not have yet, in the order above; a class somebody already plays there
-- is skipped, and an account with four characters gets nothing. Whatever
-- is already on the account is left exactly as it is. PIDs 9001-9004: the
-- playerbot seed uses 4..4503 with explicit ids, and player.player's
-- AUTO_INCREMENT would have handed a fresh world 1..4 and collided with
-- the seed's pid 4.
--
-- The new characters stand in their kingdom's first village - the market
-- pitch of playerbot_empire_rules.h (Joan for Chunjo, Yongan for Shinsoo,
-- Pyongmoo for Jinno) - because player_index carries one empire for the
-- whole account and a character in the wrong kingdom's town is stuck there.
--
-- Columns of player.player as playerbots_seed.sql writes them; the rest
-- takes the table's defaults. Bonus lines are POINT numbers (common/length.h):
-- this engine stores POINT ids in item.attrtype, not APPLY ids. Slots 5 and
-- 6 of a weapon carry the two damage lines (122 average, 121 skill) the way
-- item_addon.cpp writes them.

SET @admin_id = (SELECT id FROM account.account WHERE login = 'admin');
SET @admin_chars = IFNULL((SELECT COUNT(*) FROM player.player WHERE account_id = @admin_id), 0);
SET @empire = IFNULL((SELECT empire FROM player.player_index WHERE id = @admin_id), 2);
SET @home_map = CASE @empire WHEN 1 THEN 1 WHEN 3 THEN 41 ELSE 21 END;
SET @home_x = CASE @empire WHEN 1 THEN 473625 WHEN 3 THEN 961212 ELSE 59513 END;
SET @home_y = CASE @empire WHEN 1 THEN 954925 WHEN 3 THEN 270162 ELSE 171123 END;

-- The classes the account is short of, in order, as many as it has room for.
-- player.job is the race (0..7); the class is race % 4.
DROP TEMPORARY TABLE IF EXISTS player.tmp_gm;
SET @n := 0;
CREATE TEMPORARY TABLE player.tmp_gm AS
SELECT c.id, c.name, c.job, (@n := @n + 1) AS rn
  FROM (SELECT 9001 AS id, 'Admin'       AS name, 0 AS job UNION ALL
        SELECT 9002,       'AdminNinja',         1        UNION ALL
        SELECT 9003,       'AdminSura',          2        UNION ALL
        SELECT 9004,       'AdminSzaman',        3) AS c
 WHERE @admin_id IS NOT NULL
   AND NOT EXISTS (SELECT 1 FROM player.player AS p WHERE p.id = c.id OR p.name = c.name)
   AND NOT EXISTS (SELECT 1 FROM player.player AS p WHERE p.account_id = @admin_id AND p.job % 4 = c.job)
 ORDER BY c.id;
DELETE FROM player.tmp_gm WHERE rn > 4 - @admin_chars;
SET @created = (SELECT COUNT(*) FROM player.tmp_gm);
SET @created_names = (SELECT GROUP_CONCAT(name ORDER BY id SEPARATOR ', ') FROM player.tmp_gm);

-- Level 21 horse: c_aHorseStat[21] in horse_rider.cpp is 35 health, 120 stamina.
INSERT INTO player.player
    (id, account_id, name, job, voice, dir, x, y, z, map_index,
     exit_x, exit_y, exit_map_index, hp, mp, stamina, level, level_step,
     st, ht, dx, iq, exp, gold, stat_point, skill_point, skill_group,
     sub_skill_point, stat_reset_count, horse_hp, horse_stamina,
     horse_level, horse_hp_droptime, horse_riding, horse_skill_point,
     last_play)
SELECT c.id, @admin_id, c.name, c.job, 0, 0, @home_x, @home_y, 0, @home_map,
       @home_x, @home_y, @home_map, 20000, 5000, 800, 90, 0,
       90, 90, 90, 90, 0, 500000000, 0, 0, 1,
       0, 0, 35, 120,
       21, 0, 0, 0,
       UTC_TIMESTAMP()
  FROM player.tmp_gm AS c;

-- The character screen reads player_index: the characters already there
-- keep their order, the new ones take the free slots after them.
SET @p1 = IFNULL((SELECT pid1 FROM player.player_index WHERE id = @admin_id), 0);
SET @p2 = IFNULL((SELECT pid2 FROM player.player_index WHERE id = @admin_id), 0);
SET @p3 = IFNULL((SELECT pid3 FROM player.player_index WHERE id = @admin_id), 0);
SET @p4 = IFNULL((SELECT pid4 FROM player.player_index WHERE id = @admin_id), 0);
DROP TEMPORARY TABLE IF EXISTS player.tmp_gm_slots;
SET @k := 0;
CREATE TEMPORARY TABLE player.tmp_gm_slots AS
SELECT s.pid, (@k := @k + 1) AS slot
  FROM (SELECT @p1 AS pid, 1 AS ord UNION ALL
        SELECT @p2, 2 UNION ALL
        SELECT @p3, 3 UNION ALL
        SELECT @p4, 4 UNION ALL
        SELECT id, 10 + rn FROM player.tmp_gm) AS s
 WHERE s.pid > 0
 ORDER BY s.ord;
SET @s1 = IFNULL((SELECT pid FROM player.tmp_gm_slots WHERE slot = 1), 0);
SET @s2 = IFNULL((SELECT pid FROM player.tmp_gm_slots WHERE slot = 2), 0);
SET @s3 = IFNULL((SELECT pid FROM player.tmp_gm_slots WHERE slot = 3), 0);
SET @s4 = IFNULL((SELECT pid FROM player.tmp_gm_slots WHERE slot = 4), 0);
INSERT INTO player.player_index (id, pid1, pid2, pid3, pid4, empire)
SELECT @admin_id, @s1, @s2, @s3, @s4, @empire
  FROM DUAL
 WHERE @created > 0
ON DUPLICATE KEY UPDATE pid1 = VALUES(pid1), pid2 = VALUES(pid2), pid3 = VALUES(pid3), pid4 = VALUES(pid4);

INSERT INTO common.gmlist (mAccount, mName, mContactIP, mServerIP, mAuthority)
SELECT 'admin', c.name, '', 'ALL', 'IMPLEMENTOR'
  FROM player.tmp_gm AS c
 WHERE NOT EXISTS (SELECT 1 FROM common.gmlist AS g WHERE g.mName = c.name);

-- Worn set. pos is the wear slot: 0 body, 1 head, 2 foots, 3 wrist, 4 weapon,
-- 5 neck, 6 ear, 10 shield. Jewellery, bracelet and shield are the same for
-- everybody (antiflag 256 = no class limit); body, head, boots and the weapon
-- follow the class.
INSERT INTO player.item
    (owner_id, window, pos, count, vnum,
     attrtype0, attrvalue0, attrtype1, attrvalue1, attrtype2, attrvalue2,
     attrtype3, attrvalue3, attrtype4, attrvalue4, attrtype5, attrvalue5, attrtype6, attrvalue6)
SELECT g.owner_id, 'EQUIPMENT', g.pos, 1, g.vnum,
       g.a0, g.v0, g.a1, g.v1, g.a2, g.v2, g.a3, g.v3, g.a4, g.v4, g.a5, g.v5, g.a6, g.v6
  FROM (
    -- weapons: crit 10, pierce 10, vs monsters 20, vs humans 10, casting speed 20; average 45, skill 20
    SELECT 9001 AS owner_id, 4 AS pos,  189 AS vnum, 40 AS a0, 10 AS v0, 41 AS a1, 10 AS v1, 53 AS a2, 20 AS v2, 43 AS a3, 10 AS v3, 21 AS a4, 20 AS v4, 122 AS a5, 45 AS v5, 121 AS a6, 20 AS v6 UNION ALL
    SELECT 9002, 4, 1139, 40, 10, 41, 10, 53, 20, 43, 10, 21, 20, 122, 45, 121, 20 UNION ALL
    SELECT 9003, 4,  199, 40, 10, 41, 10, 53, 20, 43, 10, 21, 20, 122, 45, 121, 20 UNION ALL
    SELECT 9004, 4, 5129, 40, 10, 41, 10, 53, 20, 43, 10, 21, 20, 122, 45, 121, 20 UNION ALL
    -- body: hp 1500, steal hp 10, attack value 50, casting speed 20, magic resistance 15
    SELECT 9001, 0, 11299, 6, 1500, 63, 10, 95, 50, 21, 20, 77, 15, 0, 0, 0, 0 UNION ALL
    SELECT 9002, 0, 11499, 6, 1500, 63, 10, 95, 50, 21, 20, 77, 15, 0, 0, 0, 0 UNION ALL
    SELECT 9003, 0, 11699, 6, 1500, 63, 10, 95, 50, 21, 20, 77, 15, 0, 0, 0, 0 UNION ALL
    SELECT 9004, 0, 11899, 6, 1500, 63, 10, 95, 50, 21, 20, 77, 15, 0, 0, 0, 0 UNION ALL
    -- head: hp regen 12, attack speed 8, dodge 15, magic resistance 15, vs humans 10
    SELECT 9001, 1, 12289, 32, 12, 17, 8, 68, 15, 77, 15, 43, 10, 0, 0, 0, 0 UNION ALL
    SELECT 9002, 1, 12409, 32, 12, 17, 8, 68, 15, 77, 15, 43, 10, 0, 0, 0, 0 UNION ALL
    SELECT 9003, 1, 12549, 32, 12, 17, 8, 68, 15, 77, 15, 43, 10, 0, 0, 0, 0 UNION ALL
    SELECT 9004, 1, 12689, 32, 12, 17, 8, 68, 15, 77, 15, 43, 10, 0, 0, 0, 0 UNION ALL
    -- boots: hp 1500, attack speed 8, movement speed 20, crit 10, dodge 15
    SELECT 9001, 2, 15379, 6, 1500, 17, 8, 19, 20, 40, 10, 68, 15, 0, 0, 0, 0 UNION ALL
    SELECT 9002, 2, 15399, 6, 1500, 17, 8, 19, 20, 40, 10, 68, 15, 0, 0, 0, 0 UNION ALL
    SELECT 9003, 2, 15419, 6, 1500, 17, 8, 19, 20, 40, 10, 68, 15, 0, 0, 0, 0 UNION ALL
    SELECT 9004, 2, 15439, 6, 1500, 17, 8, 19, 20, 40, 10, 68, 15, 0, 0, 0, 0 UNION ALL
    -- bracelet: hp 1500, sp 250, pierce 10, steal hp 10, vs humans 10
    SELECT 9001, 3, 14529, 6, 1500, 8, 250, 41, 10, 63, 10, 43, 10, 0, 0, 0, 0 UNION ALL
    SELECT 9002, 3, 14529, 6, 1500, 8, 250, 41, 10, 63, 10, 43, 10, 0, 0, 0, 0 UNION ALL
    SELECT 9003, 3, 14529, 6, 1500, 8, 250, 41, 10, 63, 10, 43, 10, 0, 0, 0, 0 UNION ALL
    SELECT 9004, 3, 14529, 6, 1500, 8, 250, 41, 10, 63, 10, 43, 10, 0, 0, 0, 0 UNION ALL
    -- necklace: hp 1500, sp 250, crit 10, pierce 10, hp regen 12
    SELECT 9001, 5, 16529, 6, 1500, 8, 250, 40, 10, 41, 10, 32, 12, 0, 0, 0, 0 UNION ALL
    SELECT 9002, 5, 16529, 6, 1500, 8, 250, 40, 10, 41, 10, 32, 12, 0, 0, 0, 0 UNION ALL
    SELECT 9003, 5, 16529, 6, 1500, 8, 250, 40, 10, 41, 10, 32, 12, 0, 0, 0, 0 UNION ALL
    SELECT 9004, 5, 16529, 6, 1500, 8, 250, 40, 10, 41, 10, 32, 12, 0, 0, 0, 0 UNION ALL
    -- earrings: movement speed 20, vs humans 10, vs animals 20, vs orcs 20, vs undead 20
    SELECT 9001, 6, 17529, 19, 20, 43, 10, 44, 20, 45, 20, 47, 20, 0, 0, 0, 0 UNION ALL
    SELECT 9002, 6, 17529, 19, 20, 43, 10, 44, 20, 45, 20, 47, 20, 0, 0, 0, 0 UNION ALL
    SELECT 9003, 6, 17529, 19, 20, 43, 10, 44, 20, 45, 20, 47, 20, 0, 0, 0, 0 UNION ALL
    SELECT 9004, 6, 17529, 19, 20, 43, 10, 44, 20, 45, 20, 47, 20, 0, 0, 0, 0 UNION ALL
    -- shield: block 15, str 12, vit 12, vs humans 10, reflect melee 10
    SELECT 9001, 10, 13149, 67, 15, 12, 12, 13, 12, 43, 10, 79, 10, 0, 0, 0, 0 UNION ALL
    SELECT 9002, 10, 13149, 67, 15, 12, 12, 13, 12, 43, 10, 79, 10, 0, 0, 0, 0 UNION ALL
    SELECT 9003, 10, 13149, 67, 15, 12, 12, 13, 12, 43, 10, 79, 10, 0, 0, 0, 0 UNION ALL
    SELECT 9004, 10, 13149, 67, 15, 12, 12, 13, 12, 43, 10, 79, 10, 0, 0, 0, 0
  ) AS g
 WHERE g.owner_id IN (SELECT id FROM player.tmp_gm)
   AND NOT EXISTS (SELECT 1 FROM player.item AS i WHERE i.owner_id = g.owner_id);

-- The bag (5 columns; a weapon is three cells tall, so the second weapon sits
-- at 10 and covers 10, 15, 20). Every stack at what item_proto allows: the
-- potions two hundred, the scrolls twenty (a row of fifty was a stack the
-- engine cannot hold), the teleport ring ten. No horse medals: a level-21
-- horse has nothing to do with them, and fifty of them in a bag was noise.
-- What a character still lacks at login, gm_profile.quest gives through the
-- engine - this block only makes a fresh character complete on the
-- character screen.
INSERT INTO player.item
    (owner_id, window, pos, count, vnum,
     attrtype0, attrvalue0, attrtype1, attrvalue1, attrtype2, attrvalue2,
     attrtype3, attrvalue3, attrtype4, attrvalue4, attrtype5, attrvalue5, attrtype6, attrvalue6)
SELECT c.id, 'INVENTORY', b.pos, b.cnt, b.vnum,
       b.a0, b.v0, b.a1, b.v1, b.a2, b.v2, b.a3, b.v3, b.a4, b.v4, b.a5, b.v5, b.a6, b.v6
  FROM player.tmp_gm AS c
  JOIN (
    SELECT 0 AS pos, 200 AS cnt, 27007 AS vnum, 0 AS a0, 0 AS v0, 0 AS a1, 0 AS v1, 0 AS a2, 0 AS v2, 0 AS a3, 0 AS v3, 0 AS a4, 0 AS v4, 0 AS a5, 0 AS v5, 0 AS a6, 0 AS v6 UNION ALL
    SELECT 1, 200, 27008, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 UNION ALL
    SELECT 2,  20, 25040, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 UNION ALL
    SELECT 3,  20, 25045, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 UNION ALL
    SELECT 4,  10, 22030, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 UNION ALL
    SELECT 6,   1, 50053, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 UNION ALL
    SELECT 8,   1, 71054, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
  ) AS b
 WHERE NOT EXISTS (SELECT 1 FROM player.item AS i WHERE i.owner_id = c.id AND i.window = 'INVENTORY');

-- Second weapon per class: the two-handed sword, the bow (with arrows), the fan.
INSERT INTO player.item
    (owner_id, window, pos, count, vnum,
     attrtype0, attrvalue0, attrtype1, attrvalue1, attrtype2, attrvalue2,
     attrtype3, attrvalue3, attrtype4, attrvalue4, attrtype5, attrvalue5, attrtype6, attrvalue6)
SELECT w.owner_id, 'INVENTORY', w.pos, w.cnt, w.vnum,
       w.a0, w.v0, w.a1, w.v1, w.a2, w.v2, w.a3, w.v3, w.a4, w.v4, w.a5, w.v5, w.a6, w.v6
  FROM (
    SELECT 9001 AS owner_id, 10 AS pos, 1 AS cnt, 3169 AS vnum, 40 AS a0, 10 AS v0, 41 AS a1, 10 AS v1, 53 AS a2, 20 AS v2, 43 AS a3, 10 AS v3, 21 AS a4, 20 AS v4, 122 AS a5, 45 AS v5, 121 AS a6, 20 AS v6 UNION ALL
    SELECT 9002, 10,   1, 2179, 40, 10, 41, 10, 53, 20, 43, 10, 21, 20, 122, 45, 121, 20 UNION ALL
    SELECT 9002,  7, 1000, 8009,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   0,  0,   0,  0 UNION ALL
    SELECT 9004, 10,   1, 7189, 40, 10, 41, 10, 53, 20, 43, 10, 21, 20, 122, 45, 121, 20
  ) AS w
 WHERE w.owner_id IN (SELECT id FROM player.tmp_gm)
   AND NOT EXISTS (SELECT 1 FROM player.item AS i WHERE i.owner_id = w.owner_id AND i.window = 'INVENTORY' AND i.pos = w.pos);

-- Repair for a world that got the four in 2.0.4 with the set the client
-- cannot draw: the same slot, the same bonus lines, the drawable vnum.
-- Keyed on owner, slot and the old vnum, so a second run changes nothing
-- and a GM who has since chosen other gear is left alone.
UPDATE player.item AS i
  JOIN player.player AS p ON p.id = i.owner_id AND p.id BETWEEN 9001 AND 9004
   SET i.vnum = CASE i.vnum
                  WHEN 20009 THEN 11299 WHEN 20259 THEN 11499 WHEN 20509 THEN 11699 WHEN 20759 THEN 11899
                  WHEN   469 THEN   189 WHEN  1349 THEN  1139 WHEN   479 THEN   199 WHEN  5349 THEN  5129
                  WHEN  3199 THEN  3169 WHEN  2379 THEN  2179 WHEN  7379 THEN  7189
                  ELSE i.vnum END
 WHERE i.vnum IN (20009, 20259, 20509, 20759, 469, 1349, 479, 5349, 3199, 2379, 7379);

INSERT INTO player.item (owner_id, window, pos, count, vnum)
SELECT p.id, 'INVENTORY', 8, 1, 71054
  FROM player.player AS p
 WHERE p.id BETWEEN 9001 AND 9004
   AND p.name IN ('Admin', 'AdminNinja', 'AdminSura', 'AdminSzaman')
   AND NOT EXISTS (SELECT 1 FROM player.item AS i WHERE i.owner_id = p.id AND i.vnum = 71054)
   AND NOT EXISTS (SELECT 1 FROM player.item AS i WHERE i.owner_id = p.id AND i.window = 'INVENTORY' AND i.pos = 8);

SELECT CONCAT('gm characters: ',
              IF(@created > 0, CONCAT('created ', @created_names, ' on the admin account'),
                 IF(@admin_id IS NULL, 'no admin account, nothing to do',
                    IF(@admin_chars >= 4, 'the admin account holds four characters already, nothing to add',
                       'the admin account already has every class the four would add (or their names are taken), left as it is')))) AS note;

DROP TEMPORARY TABLE IF EXISTS player.tmp_gm_slots;
DROP TEMPORARY TABLE IF EXISTS player.tmp_gm;
