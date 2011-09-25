<?php
backup_migrate_include('destinations.filesource');

/**
 * @file
 * A destination type for saving locally to the server.
 */

/**
 * A destination type for saving locally to the server.
 *
 * @ingroup backup_migrate_destinations
 */

class backup_migrate_files_destination_archivesource extends backup_migrate_destination_filesource {
  var $supported_ops = array('source');

  function type_name() {
    return t("Site Archive Source");
  }

  /**
   * Declare the current files directory as a backup source..
   */
  function destinations() {
    $out  = array();
    $out['archive'] = backup_migrate_create_destination('archive', array('destination_id' => 'archive', 'location' => '.', 'name' => t('Entire Site')));
    return $out;
  }

  /**
   * Return a list of backup filetypes.
   */
  function file_types() {
    return array(
      "sitearchve" => array(
        "extension" => "sitearchive.tar",
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
    $out = parent::backup_settings_default();
    $excludes = explode("\n", $out['exclude_filepaths']);
    foreach ($excludes as $i => $exclude) {
      $excludes[$i] = file_directory_path() . '/' . $exclude;
    }
    $excludes[] = 'sites/default/files/settings.php';
    $excludes[] = file_directory_temp();

    return array(
      'exclude_filepaths' => implode("\n", $excludes),
    );
  }

  /**
   * Backup from this source.
   */
  function _backup_to_file_php($file, $settings) {
    if ($this->check_libs()) {
      $excluded_paths = empty($settings->filters['exclude_filepaths']) ? '' : $settings->filters['exclude_filepaths'];
      $exclude = $this->get_excluded_paths($excluded_paths);
      var_dump($this->get_location());
      $files = $this->get_files_to_backup($this->get_location(), $settings, $exclude, realpath('.') . '/');
      if ($files) {
        $manifest = $this->generate_manifest();
        $db = $this->get_db();

        $file->push_type('sitearchve');
        $gz = new Archive_Tar($file->filepath(), false);

        $gz->addModify(array($manifest), $file->name .'/', dirname($manifest));
        $gz->addModify($files, $file->name .'/docroot', $this->get_location());
        $gz->addModify($db, $file->name . '/', dirname($db));

        unlink($manifest);
        rmdir(dirname($manifest));
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

      $manifest = $this->generate_manifest();
      $db = $this->get_db();
      rename($db, $temp . '/database.sql');
      rename($manifest, $temp . '/MANIFEST.ini');

      $file->push_type('tar'); 
      $link = $temp . '/docroot';
      $input = realpath($this->get_location());
      backup_migrate_exec("ln -s %input %link; tar --dereference -C %temp -rf %output $exclude .", array('%output' => $file->filepath(), '%input' => $input, '%temp' => $temp, '%link' => $link));

      return $file;
    }
    return FALSE;
  }

  /**
   * Generate a manifest file.
   */
  function generate_manifest() {
    $info = array(
      'Global' => array(
        'datestamp' => time(),
        'formatversion' => '2011-07-02',
        'generator' => 'Backup and Migrate (http://drupal.org/project/backup_migrate)',
        'generatorversion' => BACKUP_MIGRATE_VERSION, 
      ),
      'Site 0' => array(
        'version' => VERSION,
        'name' => variable_get('site_name', ''),
        'docroot' => 'docroot',
        'sitedir' => 'docroot/' . conf_path(),
        'database-file-default' => 'database.sql',
        'database-file-driver' => 'mysql',
      ),
    );
    if (variable_get('file_downloads', FILE_DOWNLOADS_PUBLIC) == FILE_DOWNLOADS_PRIVATE) {
      $sites['files']['private'] = file_directory_path();
      $sites['files']['mode'] = 'private';
    }
    else {
      $sites['files']['public'] = file_directory_path();
      $sites['files']['mode'] = 'public';
    }
    $ini = $this->_array_to_ini($info);

    $tmpdir = backup_migrate_temp_directory();
    $filepath = $tmpdir . '/MANIFEST.ini';
    file_put_contents($filepath, $ini);
    return $filepath;
  }

  /**
   * Get a database dump to add to the archive.
   */
  function get_db() {
    backup_migrate_include('destinations', 'files', 'filters', 'profiles');

    $file = new backup_file();
    $settings = _backup_migrate_profile_saved_default_profile();
    $settings->source_id = 'db';
    $settings->filters['compression'] = 'none';
    $file = backup_migrate_filters_backup($file, $settings);

    // Generate a tmp file with the correct final title (because ArchiveTar doesn't seem to allow renaming).
    $tmpdir = backup_migrate_temp_directory();
    $filepath = $tmpdir . '/database.sql';
    rename($file->filepath(), $filepath);

    return $filepath;
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
}

