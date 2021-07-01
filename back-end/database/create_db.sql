-- MySQL Workbench Forward Engineering

SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0;
SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0;
SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION';

-- -----------------------------------------------------
-- Schema LaundryDB
-- -----------------------------------------------------

-- -----------------------------------------------------
-- Schema LaundryDB
-- -----------------------------------------------------
CREATE SCHEMA IF NOT EXISTS `LaundryDB` DEFAULT CHARACTER SET utf8 ;
USE `LaundryDB` ;

-- -----------------------------------------------------
-- Table `LaundryDB`.`sensors`
-- -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `LaundryDB`.`sensors` (
  `sensor_id` INT AUTO_INCREMENT,
  `sensor_name` VARCHAR(45) NOT NULL,
  PRIMARY KEY (`sensor_id`),
  UNIQUE INDEX `sensor_name_UNIQUE` (`sensor_name` ASC) VISIBLE)
ENGINE = InnoDB;

CREATE USER 'laundry_backend' IDENTIFIED BY 'admin';

GRANT SELECT ON TABLE `LaundryDB`.* TO 'laundry_backend';

SET SQL_MODE=@OLD_SQL_MODE;
SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS;
SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS;
