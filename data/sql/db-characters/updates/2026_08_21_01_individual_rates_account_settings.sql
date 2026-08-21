CREATE TABLE IF NOT EXISTS `account_individual_rates` (
  `AccountID` int(11) unsigned NOT NULL,
  `RateKey` varchar(64) NOT NULL,
  `RateValue` float NOT NULL DEFAULT 1,
  `Enabled` tinyint(1) unsigned NOT NULL DEFAULT 1,
  PRIMARY KEY (`AccountID`, `RateKey`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS `individual_rate_settings` (
  `CharacterGUID` int(11) unsigned NOT NULL,
  `SettingKey` varchar(64) NOT NULL,
  `Enabled` tinyint(1) unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`CharacterGUID`, `SettingKey`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS `account_individual_rate_settings` (
  `AccountID` int(11) unsigned NOT NULL,
  `SettingKey` varchar(64) NOT NULL,
  `Enabled` tinyint(1) unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`AccountID`, `SettingKey`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
