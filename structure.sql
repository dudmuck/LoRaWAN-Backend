-- phpMyAdmin SQL Dump
-- version 4.2.12deb2+deb8u2
-- http://www.phpmyadmin.net
--
-- Host: localhost
-- Generation Time: Feb 20, 2018 at 05:43 PM
-- Server version: 5.5.59-0+deb8u1
-- PHP Version: 5.6.33-0+deb8u1

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
SET time_zone = "+00:00";


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;

--
-- Database: `lora_app`
--
CREATE DATABASE IF NOT EXISTS `lora_app` DEFAULT CHARACTER SET latin1 COLLATE latin1_swedish_ci;
USE `lora_app`;

-- --------------------------------------------------------

--
-- Table structure for table `activemotes`
--

DROP TABLE IF EXISTS `activemotes`;
CREATE TABLE IF NOT EXISTS `activemotes` (
  `eui` bigint(20) unsigned DEFAULT NULL,
  `AppSKey` binary(16) DEFAULT NULL,
  `DevAddr` int(10) unsigned DEFAULT NULL,
  `AFCntDown` int(10) unsigned DEFAULT '0',
  `downlinkStatus` text,
`ID` int(10) unsigned NOT NULL,
  `forwardTo` varchar(32) DEFAULT NULL
) ENGINE=InnoDB AUTO_INCREMENT=13 DEFAULT CHARSET=armscii8;

-- --------------------------------------------------------

--
-- Table structure for table `configuration`
--

DROP TABLE IF EXISTS `configuration`;
CREATE TABLE IF NOT EXISTS `configuration` (
  `name` varchar(50) NOT NULL,
  `value` varchar(50) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=armscii8;

-- --------------------------------------------------------

--
-- Table structure for table `frames`
--

DROP TABLE IF EXISTS `frames`;
CREATE TABLE IF NOT EXISTS `frames` (
  `ID` int(10) unsigned NOT NULL,
  `DataRate` tinyint(2) unsigned DEFAULT NULL,
  `ULFreq` int(10) unsigned DEFAULT NULL,
  `RSSI` tinyint(3) DEFAULT NULL,
  `SNR` tinyint(3) DEFAULT NULL,
  `GWCnt` tinyint(3) unsigned DEFAULT NULL,
  `GWID` int(10) unsigned DEFAULT NULL,
  `FCntUp` int(5) unsigned NOT NULL,
  `RecvTime` datetime NOT NULL,
  `Confirmed` tinyint(1) NOT NULL,
  `FRMPayload` varbinary(244) DEFAULT NULL,
  `FPort` tinyint(3) unsigned NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Indexes for dumped tables
--

--
-- Indexes for table `activemotes`
--
ALTER TABLE `activemotes`
 ADD PRIMARY KEY (`ID`);

--
-- Indexes for table `configuration`
--
ALTER TABLE `configuration`
 ADD PRIMARY KEY (`name`);

--
-- AUTO_INCREMENT for dumped tables
--

--
-- AUTO_INCREMENT for table `activemotes`
--
ALTER TABLE `activemotes`
MODIFY `ID` int(10) unsigned NOT NULL AUTO_INCREMENT,AUTO_INCREMENT=13;--
-- Database: `lora_join`
--
CREATE DATABASE IF NOT EXISTS `lora_join` DEFAULT CHARACTER SET latin1 COLLATE latin1_swedish_ci;
USE `lora_join`;

-- --------------------------------------------------------

--
-- Table structure for table `configuration`
--

DROP TABLE IF EXISTS `configuration`;
CREATE TABLE IF NOT EXISTS `configuration` (
  `name` varchar(50) NOT NULL,
  `value` varchar(50) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=armscii8;

-- --------------------------------------------------------

--
-- Table structure for table `joinmotes`
--

DROP TABLE IF EXISTS `joinmotes`;
CREATE TABLE IF NOT EXISTS `joinmotes` (
  `eui` bigint(20) unsigned NOT NULL,
  `NwkKey` binary(16) DEFAULT NULL,
  `AppKey` binary(16) DEFAULT NULL,
  `RJcount1_last` int(10) unsigned DEFAULT NULL,
  `DevNonce_last` int(10) unsigned DEFAULT NULL,
  `JSIntKey` binary(16) DEFAULT NULL,
  `JSEncKey` binary(16) DEFAULT NULL,
  `homeNetID` int(8) unsigned DEFAULT NULL,
`ID` int(10) unsigned NOT NULL,
  `Lifetime` int(10) unsigned NOT NULL
) ENGINE=InnoDB AUTO_INCREMENT=12 DEFAULT CHARSET=armscii8;

-- --------------------------------------------------------

--
-- Table structure for table `nonces`
--

DROP TABLE IF EXISTS `nonces`;
CREATE TABLE IF NOT EXISTS `nonces` (
  `mote` bigint(20) unsigned NOT NULL,
  `nonce` smallint(5) unsigned NOT NULL,
  `time` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=MyISAM DEFAULT CHARSET=armscii8;

-- --------------------------------------------------------

--
-- Table structure for table `roamingNetIDs`
--

DROP TABLE IF EXISTS `roamingNetIDs`;
CREATE TABLE IF NOT EXISTS `roamingNetIDs` (
  `moteID` int(10) unsigned NOT NULL,
  `NetID` int(8) unsigned NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Indexes for dumped tables
--

--
-- Indexes for table `configuration`
--
ALTER TABLE `configuration`
 ADD PRIMARY KEY (`name`);

--
-- Indexes for table `joinmotes`
--
ALTER TABLE `joinmotes`
 ADD PRIMARY KEY (`ID`);

--
-- Indexes for table `nonces`
--
ALTER TABLE `nonces`
 ADD PRIMARY KEY (`mote`,`nonce`);

--
-- AUTO_INCREMENT for dumped tables
--

--
-- AUTO_INCREMENT for table `joinmotes`
--
ALTER TABLE `joinmotes`
MODIFY `ID` int(10) unsigned NOT NULL AUTO_INCREMENT,AUTO_INCREMENT=12;--
-- Database: `lora_network`
--
CREATE DATABASE IF NOT EXISTS `lora_network` DEFAULT CHARACTER SET armscii8 COLLATE armscii8_general_ci;
USE `lora_network`;

-- --------------------------------------------------------

--
-- Table structure for table `DeviceProfiles`
--

DROP TABLE IF EXISTS `DeviceProfiles`;
CREATE TABLE IF NOT EXISTS `DeviceProfiles` (
  `DeviceProfileID` int(10) unsigned NOT NULL,
  `SupportsClassB` tinyint(1) unsigned NOT NULL,
  `ClassBTimeout` int(11) NOT NULL,
  `PingSlotPeriod` int(11) NOT NULL,
  `PingSlotDR` int(10) unsigned NOT NULL,
  `PingSlotFreq` float NOT NULL,
  `SupportsClassC` tinyint(4) NOT NULL,
  `ClassCTimeout` int(10) unsigned NOT NULL,
  `MACVersion` varchar(32) NOT NULL,
  `RegParamsRevision` varchar(16) NOT NULL,
  `SupportsJoin` tinyint(1) unsigned NOT NULL,
  `RXDelay1` int(10) unsigned NOT NULL,
  `RXDROffset1` int(10) unsigned NOT NULL,
  `RXDataRate2` int(10) unsigned NOT NULL,
  `RXFreq2` float NOT NULL,
  `FactoryPresetFreqs` text NOT NULL,
  `MaxEIRP` int(4) NOT NULL,
  `MaxDutyCycle` float NOT NULL,
  `RFRegion` varchar(32) NOT NULL,
  `Supports32bitFCnt` tinyint(1) unsigned NOT NULL,
  `timestamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=armscii8;

-- --------------------------------------------------------

--
-- Table structure for table `RoutingProfiles`
--

DROP TABLE IF EXISTS `RoutingProfiles`;
CREATE TABLE IF NOT EXISTS `RoutingProfiles` (
  `RoutingProfileID` int(10) unsigned NOT NULL,
  `AS_ID` varchar(64) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=armscii8;

-- --------------------------------------------------------

--
-- Table structure for table `ServiceProfiles`
--

DROP TABLE IF EXISTS `ServiceProfiles`;
CREATE TABLE IF NOT EXISTS `ServiceProfiles` (
  `ServiceProfileID` int(10) unsigned NOT NULL,
  `ULRate` int(4) unsigned NOT NULL,
  `ULBucketSize` int(4) unsigned NOT NULL,
  `ULRatePolicy` varchar(16) NOT NULL,
  `DLRate` int(4) unsigned NOT NULL,
  `DLBucketSize` int(10) unsigned NOT NULL,
  `DLRatePolicy` varchar(16) NOT NULL,
  `AddGWMetadata` tinyint(1) NOT NULL,
  `DevStatusReqFreq` int(10) unsigned NOT NULL,
  `ReportDevStatusBattery` tinyint(1) unsigned NOT NULL,
  `ReportDevStatusMargin` tinyint(1) unsigned NOT NULL,
  `DRMin` int(4) unsigned NOT NULL,
  `DRMax` int(4) unsigned NOT NULL,
  `ChannelMask` varchar(32) NOT NULL,
  `PRAllowed` tinyint(1) unsigned NOT NULL,
  `HRAllowed` tinyint(1) unsigned NOT NULL,
  `RAAllowed` tinyint(1) unsigned NOT NULL,
  `NwkGeoLoc` tinyint(1) unsigned NOT NULL,
  `TargetPER` float NOT NULL,
  `MinGWDiversity` int(4) unsigned NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=armscii8;

-- --------------------------------------------------------

--
-- Table structure for table `configuration`
--

DROP TABLE IF EXISTS `configuration`;
CREATE TABLE IF NOT EXISTS `configuration` (
  `name` varchar(50) NOT NULL,
  `value` varchar(50) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=armscii8;

-- --------------------------------------------------------

--
-- Table structure for table `gateways`
--

DROP TABLE IF EXISTS `gateways`;
CREATE TABLE IF NOT EXISTS `gateways` (
  `eui` bigint(20) unsigned NOT NULL,
  `RFRegion` text,
  `maxTxPower_dBm` smallint(6) DEFAULT NULL,
  `allowGpsToSetPosition` tinyint(1) NOT NULL DEFAULT '1',
  `time` timestamp NULL DEFAULT NULL,
  `latitude` double(9,6) DEFAULT NULL,
  `longitude` double(9,6) DEFAULT NULL,
  `altitude` double DEFAULT NULL,
  `ntwkMaxDelayUp_ms` smallint(5) unsigned DEFAULT NULL COMMENT 'Max expected delay in ms from GW to NS',
  `ntwkMaxDelayDn_ms` smallint(5) unsigned DEFAULT NULL COMMENT 'Max expected delay in ms from NS to Gw',
  `uppacketsreceived` int(10) unsigned DEFAULT '0',
  `gooduppacketsreceived` int(10) unsigned NOT NULL DEFAULT '0',
  `uppacketsforwarded` int(10) unsigned NOT NULL DEFAULT '0',
  `uppacketsacknowedgedratio` float NOT NULL DEFAULT '0',
  `downpacketsreceived` int(10) unsigned NOT NULL DEFAULT '0',
  `packetstransmitted` int(10) unsigned NOT NULL DEFAULT '0',
  `lastuppacketid` bigint(20) unsigned DEFAULT NULL,
  `dspVersion` smallint(5) unsigned DEFAULT NULL COMMENT 'Version of GW DSP',
  `fpgaVersion` smallint(5) unsigned DEFAULT NULL COMMENT 'Version of GW FPGA',
  `halVersion` varchar(10) DEFAULT NULL COMMENT 'Version of GW Hardware abstraction layer'
) ENGINE=MyISAM DEFAULT CHARSET=armscii8 ROW_FORMAT=FIXED;

-- --------------------------------------------------------

--
-- Table structure for table `motes`
--

DROP TABLE IF EXISTS `motes`;
CREATE TABLE IF NOT EXISTS `motes` (
`ID` int(10) unsigned NOT NULL,
  `DevEUI` bigint(20) unsigned DEFAULT NULL COMMENT 'NULL for ABP',
  `OptNeg` tinyint(1) DEFAULT NULL COMMENT 'TRUE for 1v1',
  `fwdToNetID` int(8) unsigned DEFAULT NULL COMMENT 'NS this mote belongs to (NULL for this NS)',
  `roamState` enum('NONE','fPASSIVE','DEFERRED','sPASSIVE','sHANDOVER','hHANDOVER') NOT NULL DEFAULT 'NONE',
  `roamUntil` datetime DEFAULT NULL COMMENT 'roam start time + Lifetime',
  `roamingWithNetID` int(8) unsigned DEFAULT NULL,
  `enable_fNS_MIC` tinyint(1) DEFAULT NULL COMMENT 'for fNS passive',
  `sNSLifetime` int(8) unsigned DEFAULT NULL,
  `classC` tinyint(1) NOT NULL DEFAULT '0',
  `RJcount0_last` int(10) unsigned DEFAULT NULL,
  `JoinEUI` bigint(20) unsigned DEFAULT NULL
) ENGINE=MyISAM AUTO_INCREMENT=12 DEFAULT CHARSET=armscii8;

-- --------------------------------------------------------

--
-- Table structure for table `roaming`
--

DROP TABLE IF EXISTS `roaming`;
CREATE TABLE IF NOT EXISTS `roaming` (
  `NetID` int(8) unsigned DEFAULT NULL,
  `PRAllowed` tinyint(1) NOT NULL DEFAULT '0',
  `HRAllowed` tinyint(1) NOT NULL DEFAULT '0',
  `RAAllowed` tinyint(1) NOT NULL DEFAULT '0',
  `fMICup` tinyint(1) NOT NULL,
  `KEKlabel` varchar(128) DEFAULT NULL,
  `KEK` binary(16) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=armscii8;

-- --------------------------------------------------------

--
-- Table structure for table `servers`
--

DROP TABLE IF EXISTS `servers`;
CREATE TABLE IF NOT EXISTS `servers` (
  `appEui` bigint(20) unsigned NOT NULL,
  `address` varchar(250) NOT NULL COMMENT 'Because ''''.'''' is a special character in SQL searches, it is replaced in this field by ''''@''''',
  `active` tinyint(1) NOT NULL COMMENT 'active connections attempt to create a TCP connection to the remote port',
  `serviceText` varchar(100) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=armscii8;

-- --------------------------------------------------------

--
-- Table structure for table `sessions`
--

DROP TABLE IF EXISTS `sessions`;
CREATE TABLE IF NOT EXISTS `sessions` (
  `ID` int(10) unsigned NOT NULL,
  `Until` datetime DEFAULT NULL COMMENT 'NULL for ABP',
  `DevAddr` int(10) unsigned NOT NULL,
  `NFCntDown` int(10) unsigned NOT NULL DEFAULT '0',
  `FCntUp` int(10) unsigned NOT NULL,
  `FNwkSIntKey` binary(16) DEFAULT NULL,
  `SNwkSIntKey` binary(16) DEFAULT NULL,
  `NwkSEncKey` binary(16) DEFAULT NULL,
  `AS_KEK_label` varchar(256) DEFAULT NULL,
  `AS_KEK_key` varbinary(256) DEFAULT NULL,
  `createdAt` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=armscii8;

--
-- Indexes for dumped tables
--

--
-- Indexes for table `configuration`
--
ALTER TABLE `configuration`
 ADD PRIMARY KEY (`name`);

--
-- Indexes for table `motes`
--
ALTER TABLE `motes`
 ADD PRIMARY KEY (`ID`);

--
-- Indexes for table `servers`
--
ALTER TABLE `servers`
 ADD PRIMARY KEY (`appEui`,`address`);

--
-- AUTO_INCREMENT for dumped tables
--

--
-- AUTO_INCREMENT for table `motes`
--
ALTER TABLE `motes`
MODIFY `ID` int(10) unsigned NOT NULL AUTO_INCREMENT,AUTO_INCREMENT=12;--
-- Database: `lora_network6000ff`
--
CREATE DATABASE IF NOT EXISTS `lora_network6000ff` DEFAULT CHARACTER SET armscii8 COLLATE armscii8_general_ci;
USE `lora_network6000ff`;

-- --------------------------------------------------------

--
-- Table structure for table `DeviceProfiles`
--

DROP TABLE IF EXISTS `DeviceProfiles`;
CREATE TABLE IF NOT EXISTS `DeviceProfiles` (
  `DeviceProfileID` int(10) unsigned NOT NULL,
  `SupportsClassB` tinyint(1) unsigned NOT NULL,
  `ClassBTimeout` int(11) NOT NULL,
  `PingSlotPeriod` int(11) NOT NULL,
  `PingSlotDR` int(10) unsigned NOT NULL,
  `PingSlotFreq` float NOT NULL,
  `SupportsClassC` tinyint(4) NOT NULL,
  `ClassCTimeout` int(10) unsigned NOT NULL,
  `MACVersion` varchar(32) NOT NULL,
  `RegParamsRevision` varchar(16) NOT NULL,
  `SupportsJoin` tinyint(1) unsigned NOT NULL,
  `RXDelay1` int(10) unsigned NOT NULL,
  `RXDROffset1` int(10) unsigned NOT NULL,
  `RXDataRate2` int(10) unsigned NOT NULL,
  `RXFreq2` float NOT NULL,
  `FactoryPresetFreqs` text NOT NULL,
  `MaxEIRP` int(4) NOT NULL,
  `MaxDutyCycle` float NOT NULL,
  `RFRegion` varchar(32) NOT NULL,
  `Supports32bitFCnt` tinyint(1) unsigned NOT NULL,
  `timestamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=armscii8;

-- --------------------------------------------------------

--
-- Table structure for table `RoutingProfiles`
--

DROP TABLE IF EXISTS `RoutingProfiles`;
CREATE TABLE IF NOT EXISTS `RoutingProfiles` (
  `RoutingProfileID` int(10) unsigned NOT NULL,
  `AS_ID` varchar(64) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=armscii8;

-- --------------------------------------------------------

--
-- Table structure for table `ServiceProfiles`
--

DROP TABLE IF EXISTS `ServiceProfiles`;
CREATE TABLE IF NOT EXISTS `ServiceProfiles` (
  `ServiceProfileID` int(10) unsigned NOT NULL,
  `ULRate` int(4) unsigned NOT NULL,
  `ULBucketSize` int(4) unsigned NOT NULL,
  `ULRatePolicy` varchar(16) NOT NULL,
  `DLRate` int(4) unsigned NOT NULL,
  `DLBucketSize` int(10) unsigned NOT NULL,
  `DLRatePolicy` varchar(16) NOT NULL,
  `AddGWMetadata` tinyint(1) NOT NULL,
  `DevStatusReqFreq` int(10) unsigned NOT NULL,
  `ReportDevStatusBattery` tinyint(1) unsigned NOT NULL,
  `ReportDevStatusMargin` tinyint(1) unsigned NOT NULL,
  `DRMin` int(4) unsigned NOT NULL,
  `DRMax` int(4) unsigned NOT NULL,
  `ChannelMask` varchar(32) NOT NULL,
  `PRAllowed` tinyint(1) unsigned NOT NULL,
  `HRAllowed` tinyint(1) unsigned NOT NULL,
  `RAAllowed` tinyint(1) unsigned NOT NULL,
  `NwkGeoLoc` tinyint(1) unsigned NOT NULL,
  `TargetPER` float NOT NULL,
  `MinGWDiversity` int(4) unsigned NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=armscii8;

-- --------------------------------------------------------

--
-- Table structure for table `configuration`
--

DROP TABLE IF EXISTS `configuration`;
CREATE TABLE IF NOT EXISTS `configuration` (
  `name` varchar(50) NOT NULL,
  `value` varchar(50) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=armscii8;

-- --------------------------------------------------------

--
-- Table structure for table `gateways`
--

DROP TABLE IF EXISTS `gateways`;
CREATE TABLE IF NOT EXISTS `gateways` (
  `eui` bigint(20) unsigned NOT NULL,
  `RFRegion` text,
  `maxTxPower_dBm` smallint(6) DEFAULT NULL,
  `allowGpsToSetPosition` tinyint(1) NOT NULL DEFAULT '1',
  `time` timestamp NULL DEFAULT NULL,
  `latitude` double(9,6) DEFAULT NULL,
  `longitude` double(9,6) DEFAULT NULL,
  `altitude` double DEFAULT NULL,
  `ntwkMaxDelayUp_ms` smallint(5) unsigned DEFAULT NULL COMMENT 'Max expected delay in ms from GW to NS',
  `ntwkMaxDelayDn_ms` smallint(5) unsigned DEFAULT NULL COMMENT 'Max expected delay in ms from NS to Gw',
  `uppacketsreceived` int(10) unsigned DEFAULT '0',
  `gooduppacketsreceived` int(10) unsigned NOT NULL DEFAULT '0',
  `uppacketsforwarded` int(10) unsigned NOT NULL DEFAULT '0',
  `uppacketsacknowedgedratio` float NOT NULL DEFAULT '0',
  `downpacketsreceived` int(10) unsigned NOT NULL DEFAULT '0',
  `packetstransmitted` int(10) unsigned NOT NULL DEFAULT '0',
  `lastuppacketid` bigint(20) unsigned DEFAULT NULL,
  `dspVersion` smallint(5) unsigned DEFAULT NULL COMMENT 'Version of GW DSP',
  `fpgaVersion` smallint(5) unsigned DEFAULT NULL COMMENT 'Version of GW FPGA',
  `halVersion` varchar(10) DEFAULT NULL COMMENT 'Version of GW Hardware abstraction layer'
) ENGINE=MyISAM DEFAULT CHARSET=armscii8 ROW_FORMAT=FIXED;

-- --------------------------------------------------------

--
-- Table structure for table `motes`
--

DROP TABLE IF EXISTS `motes`;
CREATE TABLE IF NOT EXISTS `motes` (
`ID` int(10) unsigned NOT NULL,
  `DevEUI` bigint(20) unsigned DEFAULT NULL COMMENT 'NULL for ABP',
  `OptNeg` tinyint(1) DEFAULT NULL COMMENT 'TRUE for 1v1',
  `fwdToNetID` int(8) unsigned DEFAULT NULL COMMENT 'NS this mote belongs to (NULL for this NS)',
  `roamState` enum('NONE','fPASSIVE','DEFERRED','sPASSIVE','sHANDOVER','hHANDOVER') NOT NULL DEFAULT 'NONE',
  `roamUntil` datetime DEFAULT NULL COMMENT 'roam start time + Lifetime',
  `roamingWithNetID` int(8) unsigned DEFAULT NULL,
  `enable_fNS_MIC` tinyint(1) DEFAULT NULL COMMENT 'for fNS passive',
  `sNSLifetime` int(8) unsigned DEFAULT NULL,
  `classC` tinyint(1) NOT NULL DEFAULT '0',
  `RJcount0_last` int(10) unsigned DEFAULT NULL,
  `JoinEUI` bigint(20) unsigned DEFAULT NULL
) ENGINE=MyISAM AUTO_INCREMENT=3 DEFAULT CHARSET=armscii8;

-- --------------------------------------------------------

--
-- Table structure for table `roaming`
--

DROP TABLE IF EXISTS `roaming`;
CREATE TABLE IF NOT EXISTS `roaming` (
  `NetID` int(8) unsigned DEFAULT NULL,
  `PRAllowed` tinyint(1) NOT NULL DEFAULT '0',
  `HRAllowed` tinyint(1) NOT NULL DEFAULT '0',
  `RAAllowed` tinyint(1) NOT NULL DEFAULT '0',
  `fMICup` tinyint(1) NOT NULL,
  `KEKlabel` varchar(128) DEFAULT NULL,
  `KEK` binary(16) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=armscii8;

-- --------------------------------------------------------

--
-- Table structure for table `servers`
--

DROP TABLE IF EXISTS `servers`;
CREATE TABLE IF NOT EXISTS `servers` (
  `appEui` bigint(20) unsigned NOT NULL,
  `address` varchar(250) NOT NULL COMMENT 'Because ''''.'''' is a special character in SQL searches, it is replaced in this field by ''''@''''',
  `active` tinyint(1) NOT NULL COMMENT 'active connections attempt to create a TCP connection to the remote port',
  `serviceText` varchar(100) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=armscii8;

-- --------------------------------------------------------

--
-- Table structure for table `sessions`
--

DROP TABLE IF EXISTS `sessions`;
CREATE TABLE IF NOT EXISTS `sessions` (
  `ID` int(10) unsigned NOT NULL,
  `Until` datetime DEFAULT NULL COMMENT 'NULL for ABP',
  `DevAddr` int(10) unsigned NOT NULL,
  `NFCntDown` int(10) unsigned NOT NULL DEFAULT '0',
  `FCntUp` int(10) unsigned NOT NULL,
  `FNwkSIntKey` binary(16) DEFAULT NULL,
  `SNwkSIntKey` binary(16) DEFAULT NULL,
  `NwkSEncKey` binary(16) DEFAULT NULL,
  `AS_KEK_label` varchar(256) DEFAULT NULL,
  `AS_KEK_key` varbinary(256) DEFAULT NULL,
  `createdAt` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=armscii8;

--
-- Indexes for dumped tables
--

--
-- Indexes for table `configuration`
--
ALTER TABLE `configuration`
 ADD PRIMARY KEY (`name`);

--
-- Indexes for table `motes`
--
ALTER TABLE `motes`
 ADD PRIMARY KEY (`ID`);

--
-- Indexes for table `servers`
--
ALTER TABLE `servers`
 ADD PRIMARY KEY (`appEui`,`address`);

--
-- AUTO_INCREMENT for dumped tables
--

--
-- AUTO_INCREMENT for table `motes`
--
ALTER TABLE `motes`
MODIFY `ID` int(10) unsigned NOT NULL AUTO_INCREMENT,AUTO_INCREMENT=3;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
