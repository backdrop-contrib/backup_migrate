<?php
backup_migrate_include('sources.filesource');

/**
 * @file
 * A destination type for saving locally to the server.
 */

/**
 * A destination type for saving db and active config folder locally to the server.
 *
 * @ingroup backup_migrate_destinations
 */

class BackupMigrateFilesDestinationDbConfirSource extends backup_migrate_files_destination_archivesource {

  function type_name() {
    return t("DB and Config Archive Source");
  }

  /**
   * Declare the current files directory as a backup source.
   */
  function sources() {
    $out  = array();
    $out['db_config'] = backup_migrate_create_destination('db_config', array(
      'machine_name' => 'db_config',
      'location' => config_get_config_directory(),
      'name' => t('Database and active config directory'),
      'show_in_list' => FALSE,
    ));
    return $out;
  }

  /**
   * Return a list of backup filetypes.
   */
  function file_types() {
    return array(
      "db_config" => array(
        "extension" => "db_config.tar",
        "filemime" => "application/x-tar",
        "backup" => TRUE,
        "restore" => FALSE,
      ),
    );
  }

  /**
   * Get the form for the settings for this destination.
   */
  function backup_settings_default() {
    return array(
      'exclude_filepaths' => '',
    );
  }
  /**
   * Restore to this source.
   */
  function _restore_from_file_php($file, &$settings) {
    $success = false;
    if ($this->check_libs()) {
      $from = $file->pop_type();
      $temp = backup_migrate_temp_directory();

      $tar = new Archive_Tar($from->filepath());
      $tar->extractModify($temp, $file->name);

      // Parse the manifest
      $manifest = $this->read_manifest($temp);

      // Currently only the first site in the archive is supported.
      $site = $manifest['Site 0'];

      $docroot  = $temp . '/' . $site['docroot'];
      $sqlfile  = $temp . '/' . $site['database-file-default'];
      $filepath = NULL;
      if (isset($site['files-private'])) {
        $filepath = $temp . '/' . $site['files-private'];
      }
      else if (isset($site['files-public'])) {
        $filepath = $temp . '/' . $site['files-public'];
      }

      // Move the files from the temp directory.
      if ($filepath && file_exists($filepath)) {
        _backup_migrate_move_files($filepath, config_get('backup_migrate.settings','file_public_path') );
      }
      else {
        _backup_migrate_message('Files were not restored because the archive did not seem to contain a files directory or was in a format that Backup and Migrate couldn\'t read', array(), 'warning');
      }

      // Restore the sql db.
      if ($sqlfile && file_exists($sqlfile)) {
        $db_settings = clone($settings);
        $db_settings->source_id = 'db';
        $file = new backup_file(array('filepath' => $sqlfile));
        $success = backup_migrate_filters_restore($file, $db_settings);
      }
      else {
        _backup_migrate_message('The database was not restored because the archive did not seem to contain a database backup or was in a format that Backup and Migrate couldn\'t read', array(), 'warning');
      }

      if ($docroot) {
        _backup_migrate_message('Backup and Migrate cannot restore the php code of the site for security reasons. You will have to copy the code to the server by hand if you wish to restore the full site.', array(), 'warning');
      }

      return $success && $file;
    }
    return FALSE;
  }

  /**
   * Restore to this source.
   */
  function _restore_from_file_cli($file, &$settings) {
    // @TODO: implement the cli version of the restore.
    return FALSE;
  }

  /**
   * Generate a manifest file.
   */
  function read_manifest($directory) {
    // Assume some defaults if values ore the manifest is missing.
    $defaults = array(
      'docroot' => 'docroot',
      'database-file-default' => 'database.sql',
      'database-file-driver' => 'mysql',
    );

    $out = $this->_ini_to_array($directory . '/MANIFEST.ini');

    // Set the defaults.
    $out['Site 0'] = isset($out['Site 0']) ? $out['Site 0'] : array();
    $out['Site 0'] += $defaults;

    return $out;
  }

  /**
   * Convert an associated array to an ini format string. Only allows 2 levels of depth to allow parse_ini_file to parse.
   */
  function _array_to_ini($sections) {
    $content = ""; 
    foreach ($sections as $section => $data) {
      $content .= '['. $section .']' . "\n";
      foreach ($data as $key => $val) {
        $content .= $key . " = \"". $val ."\"\n";
      }
      $content .= "\n";
    }
    return $content;
  }

  /**
   * Convert an associated array to an ini format string. Only allows 2 levels of depth to allow parse_ini_file to parse.
   */
  function _ini_to_array($path) {
    return parse_ini_file($path, TRUE);
  }
}

