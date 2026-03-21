-- Inventory SP for mmorpg_simulator (uses existing inventory/equipment tables)
-- Run: mysql -h 127.0.0.1 -u admin -p mockdb < inventory_system.sql

USE `mockdb`;

DELIMITER //

-- Load character's items + equipment
DROP PROCEDURE IF EXISTS `spInventoryLoad`//
CREATE PROCEDURE `spInventoryLoad`(
  IN par_CharUid BIGINT
)
BEGIN
  -- Result set 1: owned items (not deleted)
  SELECT ItemUid, ItemTid FROM `inventory`
  WHERE CharUid = par_CharUid AND DeleteDate IS NULL
  ORDER BY ItemUid;

  -- Result set 2: equipped items (Slot 0=weapon, 1=armor)
  SELECT Slot, e.ItemUid, i.ItemTid FROM `equipment` e
  JOIN `inventory` i ON e.ItemUid = i.ItemUid
  WHERE e.CharUid = par_CharUid;
END//

-- Add item to inventory, returns new ItemUid
DROP PROCEDURE IF EXISTS `spInventoryAdd`//
CREATE PROCEDURE `spInventoryAdd`(
  IN par_CharUid BIGINT,
  IN par_ItemTid BIGINT
)
BEGIN
  INSERT INTO `inventory` (CharUid, ItemTid, Type, Level, Exp, Count)
  VALUES (par_CharUid, par_ItemTid, 0, 1, 0, 1);
  SELECT LAST_INSERT_ID() AS ItemUid;
END//

-- Remove item (soft delete)
DROP PROCEDURE IF EXISTS `spInventoryRemove`//
CREATE PROCEDURE `spInventoryRemove`(
  IN par_ItemUid BIGINT
)
BEGIN
  UPDATE `inventory` SET DeleteDate = NOW(3), DeleteReason = 1
  WHERE ItemUid = par_ItemUid;
  DELETE FROM `equipment` WHERE ItemUid = par_ItemUid;
END//

-- Equip item (Slot: 0=weapon, 1=armor)
DROP PROCEDURE IF EXISTS `spEquipItem`//
CREATE PROCEDURE `spEquipItem`(
  IN par_CharUid BIGINT,
  IN par_Slot SMALLINT,
  IN par_ItemUid BIGINT
)
BEGIN
  INSERT INTO `equipment` (ItemUid, CharUid, Slot)
  VALUES (par_ItemUid, par_CharUid, par_Slot)
  ON DUPLICATE KEY UPDATE ItemUid = par_ItemUid;
END//

-- Unequip item
DROP PROCEDURE IF EXISTS `spUnequipItem`//
CREATE PROCEDURE `spUnequipItem`(
  IN par_CharUid BIGINT,
  IN par_Slot SMALLINT
)
BEGIN
  DELETE FROM `equipment` WHERE CharUid = par_CharUid AND Slot = par_Slot;
END//

DELIMITER ;
