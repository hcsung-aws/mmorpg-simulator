-- Add spCharacterCreate to mockdb (based on mysql_script_mockdb_ai.sql)
-- Run: mysql -h 127.0.0.1 -u admin -p mockdb < add_spCharacterCreate.sql

USE `mockdb`;

DELIMITER //

DROP PROCEDURE IF EXISTS `spCharacterCreate`//
CREATE PROCEDURE `spCharacterCreate`(
  IN par_AccountUid BIGINT,
  IN par_CharUid BIGINT,
  IN par_CharName VARCHAR(20),
  IN par_CharType TINYINT
)
BEGIN
  DECLARE EXIT HANDLER FOR SQLEXCEPTION
  BEGIN
    ROLLBACK;
    RESIGNAL;
  END;

  IF EXISTS(SELECT 1 FROM `character` WHERE CharName = par_CharName) THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Character name already exists';
  END IF;

  START TRANSACTION;

  INSERT INTO `character` (CharUid, AccountUid, CharName, CharType)
  VALUES (par_CharUid, par_AccountUid, par_CharName, par_CharType);

  SELECT par_CharUid AS CharUid;
  
  SELECT CharName, CharType, Level, Exp 
  FROM `character` 
  WHERE AccountUid = par_AccountUid AND CharUid = par_CharUid;

  UPDATE `account`
  SET CharUid = par_CharUid
  WHERE AccountUid = par_AccountUid;

  COMMIT;
END//

DELIMITER ;
