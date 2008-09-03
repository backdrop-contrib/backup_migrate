<?php
// $Id$


/**
 * @file
 * General database dump/restore code for Backup and Migrate.
 */

/**
 * Restore from a previously backed up files. File must be a decompressed SQL file.
 */
function backup_migrate_db_restore($file) {
  // Open the file (with fopen or gzopen depending on file format).
  if ($handle = @fopen($file['filepath'], "r")) {
    $num = 0;

    // Read one line at a time and run the query.
    while ($line = fgets($handle)) {
      $line = trim($line);
      if ($line) {
        // Use the helper instead of the api function to avoid substitution of '{' etc.
        _db_query($line);
        $num++;
      }
    }
    // Close the file with fclose/gzclose.
    fclose($handle);
  }
  else {
    drupal_set_message(t("Unable to open file %file to restore database", array("%file" => $file['filepath'])), 'error');
  }
  return $num;
}

/**
 * Build the database dump file. Takes a list of tables to exclude and some formatting options.
 */
function backup_migrate_db_backup($source, $file, &$settings) {
  $settings['filename'] .= ".sql";
  $settings['filemime'] = 'text/x-sql';

  // Switch to a different db if specified.
  _backup_migrate_db_switch_db(_backup_migrate_destination_glue_url($source, FALSE));

  // Dump the database.
  $success = _backup_migrate_get_dump_sql($file, $settings['exclude_tables'], $settings['nodata_tables']);

  // Switch back to the previous db.
  _backup_migrate_db_switch_db();

  return $success ? $file : FALSE;
}

/**
 * Get the sql dump file. Returns a list of sql commands, one command per line.
 *  That makes it easier to import without loading the whole file into memory.
 *  The files are a little harder to read, but human-readability is not a priority
 */
function _backup_migrate_get_dump_sql($file, $exclude, $nodata) {
  if ($dst = fopen($file, "w")) {
    fwrite($dst, _backup_migrate_get_sql_file_header());
    $alltables = _backup_migrate_get_tables();
    foreach ($alltables as $table) {
      if ($table['Name'] && !isset($exclude[$table['Name']])) {
        fwrite($dst, _backup_migrate_get_table_structure_sql($table));
        if (!in_array($table['Name'], $nodata)) {
          _backup_migrate_dump_table_data_sql_to_handle($dst, $table);
        }
      }
    }
    fwrite($dst, _backup_migrate_get_sql_file_footer());
    fclose($dst);
    return TRUE;
  }
  else {
    return FALSE;
  }
}

/**
 * Get the sql for the structure of the given table.
 */
function _backup_migrate_get_table_structure_sql($table) {
  $out = "";
  $result = db_query("SHOW CREATE TABLE `". $table['Name'] ."`");
  if ($create = db_fetch_array($result)) {
    $out .= "DROP TABLE IF EXISTS `". $table['Name'] ."`;\n";
    $out .= strtr($create['Create Table'], "\n", " ");
    if ($table['Auto_increment']) {
      $out .= " AUTO_INCREMENT=". $table['Auto_increment'];
    }
    $out .= ";\n";
  }
  return $out;
}

/**
 *  Get the sql to insert the data for a given table
 */
function _backup_migrate_dump_table_data_sql_to_handle($dst, $table) {
  $data = db_query("SELECT * FROM `". $table['Name'] ."`");
  while ($row = db_fetch_array($data)) {
    $items = array();
    foreach ($row as $key => $value) {
      $items[] = is_null($value) ? "null" : "'". db_escape_string($value) ."'";
    }
    if ($items) {
      fwrite(
        $dst,
        "INSERT INTO `". $table['Name'] ."` VALUES (". implode(",", $items) .");\n"
        );
    }
  }
}

/**
 * The header for the top of the sql dump file. These commands set the connection
 *  character encoding to help prevent encoding conversion issues.
 */
function _backup_migrate_get_sql_file_header() {
  return "
/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE=NO_AUTO_VALUE_ON_ZERO */;

SET NAMES utf8;
";
}

/**
 * The footer of the sql dump file.
 */
function _backup_migrate_get_sql_file_footer() {
  return "

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
";
}

/**
 * Get a list of tables in the db. Works with MySQL, Postgres not tested.
 */
function _backup_migrate_get_tables() {
  $out = array();
  // get auto_increment values and names of all tables
  $tables = db_query("show table status");
  while ($table = db_fetch_array($tables)) {
    $out[$table['Name']] = $table;
  }
  return $out;
}

/**
 * Get the list of table names.
 */
function _backup_migrate_get_table_names() {
  $out = array();
  // Get auto_increment values and names of all tables.
  $tables = db_query("show table status");
  while ($table = db_fetch_array($tables)) {
    $out[$table['Name']] = $table['Name'];
  }
  return $out;
}

/**
 * Tables to ingore altogether. None by default.
 */
function _backup_migrate_default_exclude_tables() {
  return array();
}

/**
 * Return the default tables whose data can be ignored. These tables mostly contain
 *  info which can be easily reproducted (such as cache or search index)
 *  but also tables which can become quite bloated but are not necessarily extremely
 *  important to back up or migrate during development (such ass access log and watchdog)
 */
function _backup_migrate_default_structure_only_tables() {
  $core = array(
    'cache',
    'cache_filter',
    'cache_calendar_ical',
    'cache_menu',
    'cache_page',
    'cache_views',
    'sessions',
    'search_dataset',
    'search_index',
    'search_keywords_log',
    'search_total',
    'watchdog',
    'accesslog',
    'devel_queries',
    'devel_times',
  );
  $alltables = array_merge($core, module_invoke_all('devel_caches'));
  return $alltables;
}

/**
 * Switch to the db described by the DB URL, or back to previous if none selected.
 */
function _backup_migrate_db_switch_db($in_url = NULL) {
  static $db_stack = array();
  global $db_url;

  // If no DB URL is specified, pop the previous one and set to it.
  if ($in_url === NULL && $db_stack) {
    db_set_active(array_pop($db_stack));
  }

  // If there is a valid DB URL, switch to it.
  if ($in_url) {
    // Make the db_url into an array if needed.
    if (!is_array($db_url)) {
      $db_url = array('default' => $db_url);
    }
    // Add the new db to the db_url array.
    $db_url[$in_url] = $in_url;
    // Switch to the new db and push the old one on the stack
    $db_stack[] = db_set_active($in_url);
  }
}
