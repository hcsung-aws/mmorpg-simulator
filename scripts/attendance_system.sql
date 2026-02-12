-- Attendance System (Simple Version A)
-- Run on mockdb database

USE mockdb;

-- Attendance table
CREATE TABLE IF NOT EXISTS attendance (
    CharUid BIGINT NOT NULL,
    AttendDate DATE NOT NULL,
    RewardGold INT DEFAULT 0,
    CreatedAt DATETIME DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (CharUid, AttendDate)
);

-- Drop existing procedures if any
DROP PROCEDURE IF EXISTS spAttendanceInfo;
DROP PROCEDURE IF EXISTS spAttendanceCheck;

DELIMITER //

-- Get attendance info for today
CREATE PROCEDURE spAttendanceInfo(IN p_CharUid BIGINT)
BEGIN
    DECLARE v_Attended TINYINT DEFAULT 0;
    DECLARE v_RewardGold INT DEFAULT 100;
    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        SELECT 0 AS TodayAttended, 0 AS RewardGold;
    END;
    
    START TRANSACTION;
    
    SELECT 1 INTO v_Attended
    FROM attendance
    WHERE CharUid = p_CharUid AND AttendDate = CURDATE()
    LIMIT 1;
    
    COMMIT;
    SELECT IFNULL(v_Attended, 0) AS TodayAttended, v_RewardGold AS RewardGold;
END //

-- Check attendance and give reward
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
        
        COMMIT;
        SELECT 1 AS Success, v_RewardGold AS RewardGold;
    END IF;
END //

DELIMITER ;
