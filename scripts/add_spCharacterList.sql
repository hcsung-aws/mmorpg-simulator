-- Add spCharacterList to mockdb
-- Run: mysql -h 127.0.0.1 -u admin -p mockdb < add_spCharacterList.sql

USE `mockdb`;

DELIMITER //

DROP PROCEDURE IF EXISTS `spCharacterList`//
CREATE PROCEDURE `spCharacterList`(
  IN par_AccountUid BIGINT
)
BEGIN
  SELECT CharUid, CharName, CharType, Level, Exp
  FROM `character`
  WHERE AccountUid = par_AccountUid
  ORDER BY CharUid
  LIMIT 5;
END//

DELIMITER ;
