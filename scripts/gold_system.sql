-- Gold System
-- Run: mysql -h 172.29.176.1 -u admin -pflfhelem1! mockdb < gold_system.sql

USE mockdb;

-- Add Gold column to character table
ALTER TABLE `character` ADD COLUMN Gold BIGINT DEFAULT 0;

-- Update spAttendanceCheck to add gold
DROP PROCEDURE IF EXISTS spAttendanceCheck;

DELIMITER //

CREATE PROCEDURE spAttendanceCheck(IN p_CharUid BIGINT)
BEGIN
    DECLARE v_RewardGold INT DEFAULT 100;
    DECLARE v_Already TINYINT DEFAULT 0;
    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        SELECT 0 AS Success, 0 AS RewardGold;
    END;
    
    START TRANSACTION;
    
    SELECT 1 INTO v_Already
    FROM attendance
    WHERE CharUid = p_CharUid AND AttendDate = CURDATE()
    LIMIT 1;
    
    IF v_Already = 1 THEN
        COMMIT;
        SELECT 0 AS Success, 0 AS RewardGold;
    ELSE
        INSERT INTO attendance (CharUid, AttendDate, RewardGold)
        VALUES (p_CharUid, CURDATE(), v_RewardGold);
        
        UPDATE `character` SET Gold = Gold + v_RewardGold WHERE CharUid = p_CharUid;
        
        COMMIT;
        SELECT 1 AS Success, v_RewardGold AS RewardGold;
    END IF;
END //

-- Update spCharacterLogin to return character info with Gold
DROP PROCEDURE IF EXISTS spCharacterLogin //

CREATE PROCEDURE spCharacterLogin(
    IN par_AccountUid BIGINT,
    IN par_CharUid BIGINT
)
BEGIN
    DECLARE exit handler for sqlexception
    BEGIN
        ROLLBACK;
        RESIGNAL;
    END;

    START TRANSACTION;

    IF NOT EXISTS(SELECT 1 FROM `character` WHERE AccountUid = par_AccountUid AND CharUid = par_CharUid) THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Character not found';
    END IF;

    UPDATE `character` SET LoginDate = NOW() WHERE AccountUid = par_AccountUid AND CharUid = par_CharUid;

    -- Return character basic info (first result set)
    SELECT Level, Exp, Gold FROM `character` WHERE CharUid = par_CharUid;

    SELECT ItemUid, Slot FROM equipment WHERE CharUid = par_CharUid;

    UPDATE `inventory` SET DeleteDate = NOW(), DeleteReason = 999 
    WHERE CharUid = par_CharUid AND DeleteDate IS NULL AND (ExpireDate IS NOT NULL AND ExpireDate < NOW());

    SELECT ItemUid, ItemTid, `Type`, `Level`, `Exp`, `Count` 
    FROM `inventory` 
    WHERE CharUid = par_CharUid AND DeleteDate IS NULL AND (ExpireDate IS NULL OR ExpireDate > NOW());

    SELECT CurrencyUid, CurrencyTid, `Value` FROM currency WHERE CharUid = par_CharUid;

    SELECT HeroUid, HeroTid, Grade, `Level`, `Exp`, Enchant 
    FROM hero 
    WHERE CharUid = par_CharUid AND DeleteDate IS NULL;

    SELECT QuestUid, QuestTid, Category1, Category2, Value1, Value2, `State` 
    FROM quest 
    WHERE CharUid = par_CharUid;

    SELECT AchieveUid, AchieveTid, Category1, Category2, Value1, Value2, `State` 
    FROM achievement 
    WHERE CharUid = par_CharUid;

    SELECT CollectionUid, CollectionTid, `Type`, `State`, Value1, Value2, Value3, Value4, Value5, Value6 
    FROM collection 
    WHERE CharUid = par_CharUid;

    UPDATE post SET DeleteDate = NOW() 
    WHERE CharUid = par_CharUid AND DeleteDate IS NULL AND ExpireDate IS NOT NULL AND ExpireDate < NOW();

    SELECT PostUid, PostTid, RewardTid, RewardValue, isReaded, ExpireDate 
    FROM post 
    WHERE CharUid = par_CharUid AND DeleteDate IS NULL AND (ExpireDate IS NULL OR ExpireDate > NOW());

    COMMIT;
END //

DELIMITER ;
