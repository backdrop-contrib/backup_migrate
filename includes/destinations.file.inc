<?php
// $Id$


/**
 * @file
 * Functions to handle the local server directory backup destinations.
 */

/**
 * File save destination callback.
 */
function backup_migrate_destination_file_save($destination, $file, &$settings) {
  if ($dir = @$destination['location']) {
    if (_backup_migrate_destination_file_check_dir($dir)) {
      $filepath = rtrim($dir, "/") ."/". $settings['filename'];
      rename($file, $filepath);
      return $file;
    }
  }
}

/**
 * File load destination callback.
 */
function backup_migrate_destination_file_load($destination, $file_id) {
  $filepath = rtrim($destination['location'], '/') .'/'. $file_id;
  if (file_exists($filepath)) {
    require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/files.inc';
    return backup_migrate_file_info($filepath);
  }
}

/**
 * File delete destination callback.
 */
function backup_migrate_destination_file_delete($destination, $file_id) {
  $filepath = rtrim($destination['location'], '/') .'/'. $file_id;
  file_delete($filepath);
}

/**
 * File list destination callback.
 */
function backup_migrate_destination_files_list($destination) {
  $files = array();
  if ($dir = @$destination['location']) {
    if ($handle = opendir($dir)) {
      require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/files.inc';
      while (FALSE !== ($file = readdir($handle))) {
        $filepath = $dir ."/". $file;
        if ($info = backup_migrate_file_info($filepath)) {
          $files[$file] = $info;
        }
      }
    }
  }
  return $files;
}

/**
 * Destination configuration callback.
 */
function backup_migrate_destination_file_conf($destination, $form) {
  $form['location'] = array(
    "#type" => "textfield",
    "#title" => t("Directory path"),
    "#default_value" => $destination['location'],
    "#required" => TRUE,
    "#description" => t('Enter the path to the directory to save the backups to. Use a relative path to pick a path relative to your Drupal root directory. The web server must be able to write to this path.'),
  );
  return $form;
}

/**
 * Prepare the destination directory for the backups.
 */
function _backup_migrate_destination_file_check_dir($directory) {
  $out = TRUE;
  $dirs = array();
  foreach (explode('/', $directory) as $dir) {
    $dirs[] = $dir;
    $path = implode($dirs, '/');
    if ($path && !is_dir(realpath($path)) && !file_check_directory($path, FILE_CREATE_DIRECTORY)) {
      $out = FALSE;
    }
  }

  if (!$out || !file_check_directory($directory)) {
    // Unable to create destination directory.
    _backup_migrate_message("Unable to create or write to the save directory '%directory'. Please check the file permissions that directory and try again.", array('%directory' => $directory), "error");
    return FALSE;
  }

  // If the destination directory is within the webroot, then secure it as best we can.
  if (_backup_migrate_dir_in_webroot($directory)) {
    $directory = _backup_migrate_destination_file_check_web_dir($directory);
  }

  return $directory;
}

/**
 * Check that a web accessible directory has been properly secured, othewise attempt to secure it.
 */
function _backup_migrate_destination_file_check_web_dir($directory) {
  // Check for a htaccess file which adequately protects the backup files.
  $htaccess_lines = "order allow,deny\ndeny from all\n";
  if (!is_file($directory .'/.htaccess') || strpos(file_get_contents($directory .'/.htaccess'), $htaccess_lines) === FALSE) {
    // Attempt to protect the backup files from public access using htaccess.
    if (($fp = @fopen($directory .'/.htaccess', 'w')) && @fputs($fp, $htaccess_lines)) {
      fclose($fp);
      chmod($directory .'/.htaccess', 0664);
    }
    // Unable to create htaccess... warn the user.
    else {
      $message = "Security warning: Couldn't modify .htaccess file. Please create a .htaccess file in your %directory directory which contains the following lines: <code>!htaccess</code> or add them to the existing .htaccess file";
      $replace = array('%directory' => $directory, '!htaccess' => '<br />'. nl2br(check_plain($htaccess_lines)));
      drupal_set_message(t($message, $replace), "error");
      watchdog('security', t($message, $replace), WATCHDOG_ERROR);
      return FALSE;
    }
  }

  // Check the user agent to make sure we're not responding to a request from drupal itself.
  // That should prevent infinite loops which could be caused by poormanscron in some circumstances.
  if (strpos($_SERVER['HTTP_USER_AGENT'], 'Drupal') !== FALSE) {
    return FALSE;
  }

  // Check to see if the destination is publicly accessible
  $test_contents = "this file should not be publicly accesible";
  // Create the the text.txt file if it's not already there.
  if (!is_file($directory .'/test.txt') || file_get_contents($directory .'/test.txt') != $test_contents) {
    if ($fp = fopen($directory .'/test.txt', 'w')) {
      @fputs($fp, $test_contents);
      fclose($fp);
    }
    else {
      $message = t("Security notice: Backup and Migrate was unable to write a test text file to the destination directory %directory, and is therefore unable to check the security of the backup destination. Backups to the server will be disabled until the destination becomes writable and secure.", array('%directory' => $directory));
      drupal_set_message($message, "error");
      return FALSE;
    }
  }

  // Attempt to read the test file via http. This may fail for other reasons,
  // so it's not a bullet-proof check.
  $path = trim(drupal_substr($directory .'/test.txt', drupal_strlen(file_directory_path())), '\\/');
  if (_backup_migrate_test_file_readable_remotely($filename, $contents)) {
    $message = t("Security notice: Backup and Migrate will not save backup files to the server because the destination directory is publicly accessible. If you want to save files to the server, please secure the '%directory' directory", array('%directory' => $directory));
    drupal_set_message($message, "error");
    return FALSE;
  }
  return $directory;
}

/**
 * Check if the given directory is within the webroot and is therefore web accessible.
 */
function _backup_migrate_dir_in_webroot($directory) {
  if (strpos(realpath($directory), realpath($_SERVER['DOCUMENT_ROOT'])) !== FALSE) {
    return TRUE;
  }
  return FALSE;
}

/**
 * Check if a file can be read remotely via http.
 */
function _backup_migrate_test_file_readable_remotely($path, $contents) {
  $url = $GLOBALS['base_url'] .'/'. file_directory_path() .'/'. str_replace('\\', '/', $path);
  $result = drupal_http_request($url);
  if (strpos($result->data, $contents) !== FALSE) {
    return TRUE;
  }
  return FALSE;
}

/**
 * Return the path on the server to save the dump files.
 */
function _backup_migrate_path_is_in_save_dir($path, $mode = "") {
  $backup_dir = _backup_migrate_get_save_path($mode);
  return file_exists($backup_dir) && file_check_location($path, $backup_dir);
}

/**
 * Check the default backup desitantion directory. Included as legacy support.
 */
function _backup_migrate_check_destination_dir($mode = "manual") {
  return _backup_migrate_destination_file_check_dir(_backup_migrate_get_save_path($mode));
}

/**
 * Return the path on the server to save the dump files.
 */
function _backup_migrate_get_save_path($mode = "") {
  $dir = file_directory_path() ."/backup_migrate";
  if ($mode) {
    $dir .= $mode == "manual" ? "/manual" : "/scheduled";
  }
  return $dir;
}

