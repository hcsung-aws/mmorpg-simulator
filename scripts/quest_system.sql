-- Quest System Tables and Stored Procedures

CREATE TABLE IF NOT EXISTS quest_progress (
    CharUid BIGINT NOT NULL,
    QuestId INT NOT NULL,
    Status TINYINT NOT NULL DEFAULT 0,   -- 0=Available, 1=Accepted, 2=Completable, 3=Completed
    Progress INT NOT NULL DEFAULT 0,
    UpdatedAt DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (CharUid, QuestId)
);

-- spQuestLoad: Load all quest progress for a character
DROP PROCEDURE IF EXISTS spQuestLoad;
DELIMITER //
CREATE PROCEDURE spQuestLoad(IN p_CharUid BIGINT)
BEGIN
    SELECT QuestId, Status, Progress
    FROM quest_progress
    WHERE CharUid = p_CharUid;
END //
DELIMITER ;

-- spQuestSave: Upsert quest progress
DROP PROCEDURE IF EXISTS spQuestSave;
DELIMITER //
CREATE PROCEDURE spQuestSave(IN p_CharUid BIGINT, IN p_QuestId INT, IN p_Status TINYINT, IN p_Progress INT)
BEGIN
    INSERT INTO quest_progress (CharUid, QuestId, Status, Progress)
    VALUES (p_CharUid, p_QuestId, p_Status, p_Progress)
    ON DUPLICATE KEY UPDATE Status = p_Status, Progress = p_Progress;
END //
DELIMITER ;
