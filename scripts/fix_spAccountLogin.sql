-- Fix spAccountLogin to always return AccountUid, CharUid
-- Run this in MySQL: mysql -h 127.0.0.1 -u admin -p mockdb < fix_spAccountLogin.sql

USE `mockdb`;

DELIMITER //

DROP PROCEDURE IF EXISTS `spAccountLogin`//
CREATE PROCEDURE `spAccountLogin`(
  IN par_ChannelType TINYINT,
  IN par_ChannelId BIGINT
)
BEGIN
  DECLARE v_AccountUid BIGINT DEFAULT 0;
  DECLARE v_CharUid BIGINT DEFAULT 0;

  -- Find existing account
  SELECT AccountUid INTO v_AccountUid
  FROM `account` 
  WHERE ChannelType = par_ChannelType AND ChannelId = par_ChannelId
  LIMIT 1;

  IF v_AccountUid = 0 THEN
    -- New account: use ChannelId as AccountUid
    INSERT INTO `account` (AccountUid, ChannelType, ChannelId)
    VALUES (par_ChannelId, par_ChannelType, par_ChannelId);
    
    SET v_AccountUid = par_ChannelId;
  END IF;

  -- Get character if exists
  SELECT CharUid INTO v_CharUid
  FROM `character`
  WHERE AccountUid = v_AccountUid
  LIMIT 1;

  -- Always return AccountUid and CharUid (CharUid=0 if no character)
  SELECT v_AccountUid AS AccountUid, IFNULL(v_CharUid, 0) AS CharUid;
END//

DELIMITER ;
