
-- 
-- Create the table chilren
-- 
DROP TABLE submit;
DROP TABLE user;
DROP TABLE contest;
DROP TABLE problem;
DROP TABLE schedule;

CREATE TABLE submit (
    RunID INT(11) NOT NULL auto_increment PRIMARY KEY,
    ProID INT(11) NOT NULL DEFAULT 1,
    submittime DATETIME,
    status SMALLINT(6) DEFAULT 0,
    time INT(11) DEFAULT 0,
    memory INT(11) DEFAULT 0,
    codelen INT(11),
    language TINYINT(4),
    authorID varchar(30) NOT NULL,
    contest INT(11) NOT NULL DEFAULT 0
);

CREATE TABLE problem (
    ProID INT(11) NOT NULL auto_increment PRIMARY KEY,
    timelimit INT(11) NOT NULL DEFAULT 1000,
    casetimelimit INT(11) DEFAULT 0,
    memlimit INT(11) DEFAULT 0,
    contest INT(11) NOT NULL DEFAULT 0,
    spj TINYINT(4) DEFAULT 0
);

