<?php
// $Id$


/**
 * @file
 * General file handling code for Backup and Migrate.
 */

/**
 * Add a file to the temporary files list for deletion when we're done.
 */
function backup_migrate_temp_files_add($filepath = NULL) {
  static $files = array();
  if (!$filepath) {
    return $files;
  }
  else {
    $files[] = $filepath;
  }
}

/**
 * Delete all temporary files.
 */
function _backup_migrate_temp_files_delete() {
  foreach (backup_migrate_temp_files_add() as $file) {
    if (file_exists($file)) {
      @unlink($file);
    }
  }
}

/**
 * Return a list of backup filetypes.
 */
function _backup_migrate_filetypes() {
  require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/filters.inc';
  $out = array(
    "sql" => array(
      "extension" => "sql",
      "filemime" => "text/x-sql",
      "backup" => TRUE,
      "restore" => TRUE,
    ),
    "mysql" => array(
      "extension" => "mysql",
      "filemime" => "text/x-sql",
      "backup" => TRUE,
      "restore" => TRUE,
    ),
  );
  $out += backup_migrate_filters_file_types();
  return $out;
}

/**
 * Construct a filename using token and some cleaning.
 */
function _backup_migrate_clean_filename($filename) {
  if (module_exists('token') && function_exists('token_replace')) {
    $filename = token_replace($filename, 'global');
  }
  $filename = preg_replace("/[^a-zA-Z0-9\.\-_]/", "", $filename);
  if (drupal_strlen($filename) > 50) {
    $filename = drupal_substr($filename, 0, 50);
  }
  if (drupal_strlen($filename) == 0) {
    $filename = 'untitled';
  }
  return $filename;
}

/**
 * Construct a default filename using the site's name.
 */
function _backup_migrate_default_filename() {
  if (module_exists('token')) {
    return '[site-name]';
  }
  else {
    return _backup_migrate_clean_filename(variable_get(site_name, "backup_migrate"));
  }
}

/**
 * A backup file which allows for saving to and reading from the server.
 */
class backup_file {
  var $file_info = array();
  var $type = array();
  var $ext = array();
  var $path = "";
  var $name = "";
  var $handle = NULL;

  /**
   * Construct a file object given a file path, or create a temp file for writing.
   */
  function backup_file($params = array()) {
    if (isset($params['filepath']) && file_exists($params['filepath'])) {
      $this->set_filepath($params['filepath']);
    }
    else {
      $this->set_file_info($params);
      $this->temporary_file();
    }
  }

  /**
   * Get the file_id if the file has been saved to a destination.
   */
  function file_id() {
    // The default file_id is the filename. Destinations can override the file_id if needed.
    return isset($this->file_info['file_id']) ? $this->file_info['file_id'] : $this->filename();
  }

  /**
   * Get the current filepath.
   */
  function filepath() {
    return $this->path;
  }

  /**
   * Get the final filename.
   */
  function filename($name = NULL) {
    if ($name) {
      $this->name = $name;
    }
    return $this->name .'.'. $this->extension();
  }

  /**
   * Set the current filepath.
   */
  function set_filepath($path) {
    $this->path = $path;
    $params = array(
      'filename' => basename($path),
    );
    if (file_exists($path)) {
      $params['filesize'] = filesize($path);
      $params['filetime'] = filectime($path);
    }
    $this->set_file_info($params);
  }

  /**
   * Get one or all pieces of info for the file.
   */
  function info($key = NULL) {
    if ($key) {
      return @$this->file_info[$key];
    }
    return $this->file_info;
  }

  /**
   * Get the file extension.
   */
  function extension() {
    return implode(".", $this->ext);
  }

  /**
   * Get the file type.
   */
  function type() {
    return $this->type;
  }

  /**
   * Get the file mimetype.
   */
  function mimetype() {
    return @$this->type['filemime'] ? $this->type['filemime'] : 'application/octet-stream';
  }

  /**
   * Get the file mimetype.
   */
  function type_id() {
    return @$this->type['id'];
  }


  /**
   * Can this file be used to backup to.
   */
  function can_backup() {
    return @$this->type['backup'];
  }

  /**
   * Can this file be used to restore to.
   */
  function can_restore() {
    return @$this->type['restore'];
  }

  /**
   * Can this file be used to restore to.
   */
  function is_recognized_type() {
    return @$this->type['restore'] || @$this->type['backup'];
  }

  /**
   * Open a file for reading or writing.
   */
  function open($write = FALSE, $binary = FALSE) {
    if (!$this->handle) {
      $mode = ($write ? "w" : "r") . ($binary ? "b" : "");
      $this->handle = fopen($this->filepath(), $mode);
      return $this->handle;
    }
    return NULL;
  }

  /**
   * Close a file when we're done reading/writing.
   */
  function close() {
    fclose($this->handle);
    $this->handle = NULL;
  }

  /**
   * Write a line to the file.
   */
  function write($data) {
    if (!$this->handle) {
      $this->handle = $this->open(TRUE);
    }
    if ($this->handle) {
      fwrite($this->handle, $data);
    }
  }

  /**
   * Read a line from the file.
   */
  function read($size = NULL) {
    if (!$this->handle) {
      $this->handle = $this->open();
    }
    if ($this->handle && !feof($this->handle)) {
      return $size ? fread($this->handle, $size) : fgets($this->handle);
    }
    return NULL;
  }

  /**
   * Write data to the file.
   */
  function put_contents($data) {
    file_put_contents($this->filepath(), $data);
  }

  /**
   * Read data from the file.
   */
  function get_contents() {
    return file_get_contents($this->filepath());
  }

  /**
   * Transfer file using http to client. Similar to the built in file_transfer,
   *  but it calls module_invoke_all('exit') so that temp files can be deleted.
   */
  function transfer() {
    $headers = array(
      'Content-Type: '. $this->mimetype(),
      'Content-Disposition: attachment; filename="'. $this->filename() .'"',
    );
    if ($size = $this->info('filesize')) {
      $headers[] = 'Content-Length: '. $size;
    }

    // Suppress the warning you get when the buffer is empty.
    @ob_end_clean();

    if ($this->open(FALSE, TRUE)) {
      foreach ($headers as $header) {
        // To prevent HTTP header injection, we delete new lines that are
        // not followed by a space or a tab.
        // See http://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.2
        $header = preg_replace('/\r?\n(?!\t| )/', '', $header);
        drupal_set_header($header);
      }
      // Transfer file in 1024 byte chunks to save memory usage.
      while ($data = $this->read(1024)) {
        print $data;
      }
      $this->close();

      // Ask devel.module not to print it's footer.
      $GLOBALS['devel_shutdown'] = FALSE;
    }
    else {
      drupal_not_found();
    }

    ob_start();
    module_invoke_all('exit');
    exit();
  }

  /**
   * Push a file extension onto the file and return the previous file path.
   */
  function push_type($extension) {
    $types = _backup_migrate_filetypes();
    if ($type = @$types[$extension]) {
      $this->push_filetype($type);
    }

    $out = $this->filepath();
    $this->temporary_file();
    return $out;
  }

  /**
   * Push a file extension onto the file and return the previous file path.
   */
  function pop_type() {
    $out = new backup_file(array('filepath' => $this->filepath()));
    $this->pop_filetype();
    $this->temporary_file();
    return $out;
  }

  /**
   * Set the current file type.
   */
  function set_filetype($type) {
    $this->type = $type;
    $this->ext = array($type['extension']);
  }

  /**
   * Set the current file type.
   */
  function push_filetype($type) {
    $this->ext[] = $type['extension'];
    $this->type = $type;
  }

  /**
   * Pop the current file type.
   */
  function pop_filetype() {
    array_pop($this->ext);
    $this->detect_filetype_from_extension();
  }

  /**
   * Set the file info.
   */
  function set_file_info($file_info) {
    $this->file_info = $file_info;

    $this->ext = explode('.', @$this->file_info['filename']);
    $this->filename(array_shift($this->ext));
    $this->detect_filetype_from_extension();
  }

  /**
   * Get the filetype info of the given file, or false if the file is not a valid type.
   */
  function detect_filetype_from_extension() {
    $ext = end($this->ext);
    $this->type = array();
    $types = _backup_migrate_filetypes();
    foreach ($types as $key => $type) {
      if (trim($ext, "_0123456789") === $type['extension']) {
        $this->type = $type;
        $this->type['id'] = $key;
      }
    }
  }

  /**
   * Get a temporary file name with path.
   */
  function temporary_file() {
    $file = file_directory_temp() .'/'. uniqid('backup_migrate_');
    $file .= '.'. $this->extension();
    backup_migrate_temp_files_add($file);
    $this->path = $file;
  }
}
