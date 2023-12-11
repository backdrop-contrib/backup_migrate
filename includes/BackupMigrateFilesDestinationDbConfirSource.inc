<?php
/**
 * @file
 * A destination type for saving locally to the server.
 */

/**
 * A destination type for saving db and active config folder locally to the server.
 *
 * @ingroup backup_migrate_destinations
 */

class BackupMigrateFilesDestinationDbConfirSource extends backup_migrate_destination_filesource {

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
   * Backup from this source.
   */
  function _backup_to_file_php($file, $settings) {
    if ($this->check_libs()) {
      $base_dir = $this->get_realpath();

      $excluded_paths = empty($settings->filters['exclude_filepaths']) ? '' : $settings->filters['exclude_filepaths'];
      $exclude = $this->get_excluded_paths($settings);
      $files = $this->get_files_to_backup($this->get_realpath(), $settings, $exclude);
      if ($files) {
        $db = $this->get_db();

        $file->push_type('db_config');
        $gz = new Archive_Tar($file->filepath(), false);

        $gz->addModify($files, $file->name .'/active', $base_dir);
        $gz->addModify($db, $file->name . '/', dirname($db));

        unlink($db);
        rmdir(dirname($db));

        return $file;
      }
      backup_migrate_backup_fail('No files available.', array(), $settings);
      return FALSE;
    }
    return FALSE;
  }

  /**
   * Backup from this source.
   */
  function _backup_to_file_cli($file, $settings) {
    if (!empty($settings->filters['use_cli']) && function_exists('backup_migrate_exec') && function_exists('escapeshellarg')) {
      $excluded_paths = empty($settings->filters['exclude_filepaths']) ? '' : $settings->filters['exclude_filepaths'];
      foreach ($this->get_excluded_paths($excluded_paths) as $path) {
        $exclude[] = '--exclude=' . escapeshellarg($path);
      }
      $exclude = implode(' ', $exclude);

      // Create a symlink in a temp directory so we can rename the file in the archive.
      $temp = backup_migrate_temp_directory();

      $db = $this->get_db();
      rename($db, $temp . '/database.sql');

      $file->push_type('db_config');
      $link = $temp . '/active';
      $input = realpath($this->get_location());
      backup_migrate_exec("ln -s %input %link; tar --dereference -C %temp -rf %output $exclude .", array('%output' => $file->filepath(), '%input' => $input, '%temp' => $temp, '%link' => $link));

      return $file;
    }
    return FALSE;
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

      $sqlfile  = $temp . '/database.sql';
      $filepath = $temp . '/active';
      // Move the files from the temp directory.
      _backup_migrate_move_files($filepath, config_get_config_directory('active'));

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
      return $success && $file;
    }
    return FALSE;
  }


  /**
   * Get a database dump to add to the archive.
   */
  function get_db() {
    backup_migrate_include('destinations', 'files', 'filters', 'profiles');

    $file = new backup_file();
    // Clone the default settings so we can make changes without them leaking out of this function.
    $settings = clone _backup_migrate_profile_saved_default_profile();
    $settings->source_id = 'db';
    $settings->filters['compression'] = 'none';

    // Execute the backup on the db with the default settings.
    $file = backup_migrate_filters_backup($file, $settings);

    // Generate a tmp file with the correct final title (because ArchiveTar doesn't seem to allow renaming).
    $tmpdir = backup_migrate_temp_directory();
    $filepath = $tmpdir . '/database.sql';
    rename($file->filepath(), $filepath);

    return $filepath;
  }

  
  /**
   * Restore to this source.
   */
  function _restore_from_file_cli($file, &$settings) {
    // @TODO: implement the cli version of the restore.
    return FALSE;
  }
}
