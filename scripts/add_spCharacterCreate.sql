-- spCharacterCreate: charUid를 AUTO_INCREMENT로 서버 생성
-- Run: mysql -h 127.0.0.1 -u admin -p mockdb < add_spCharacterCreate.sql

USE `mockdb`;

DELIMITER //

DROP PROCEDURE IF EXISTS `spCharacterCreate`//
CREATE PROCEDURE `spCharacterCreate`(
  IN par_AccountUid BIGINT,
  IN par_CharName VARCHAR(20),
  IN par_CharType TINYINT
)
BEGIN
  DECLARE v_CharUid BIGINT DEFAULT 0;

  DECLARE EXIT HANDLER FOR SQLEXCEPTION
  BEGIN
    ROLLBACK;
    RESIGNAL;
  END;

  IF EXISTS(SELECT 1 FROM `character` WHERE CharName = par_CharName) THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Character name already exists';
  END IF;

  START TRANSACTION;

  INSERT INTO `character` (AccountUid, CharName, CharType)
  VALUES (par_AccountUid, par_CharName, par_CharType);

  SET v_CharUid = LAST_INSERT_ID();

  SELECT v_CharUid AS CharUid;
  
  SELECT CharName, CharType, Level, Exp 
  FROM `character` 
  WHERE AccountUid = par_AccountUid AND CharUid = v_CharUid;

  UPDATE `account`
  SET CharUid = v_CharUid
  WHERE AccountUid = par_AccountUid;

  COMMIT;
END//

DELIMITER ;
