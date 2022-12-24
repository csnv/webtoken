-- Append web_auth_token and web_auth_token_enabled columns to login table
ALTER TABLE `login`
	ADD COLUMN `web_auth_token` VARCHAR(17) NULL AFTER `pincode_change`,
	ADD COLUMN `web_auth_token_enabled` tinyint(2) NOT NULL default '0' AFTER `web_auth_token`
;