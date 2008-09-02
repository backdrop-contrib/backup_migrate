<?php
// $Id$


/**
 * @file
 * General file handling code for Backup and Migrate.
 */

/**
 * Transfer file using http to client. Similar to the built in file_transfer,
 *  but it calls module_invoke_all('exit') so that temp files can be deleted.
 *
 * @param $info An info array (as returned by backup_migrate_file_info) for the file.
 */
function backup_migrate_file_transfer($info) {
  $headers = array(
    'Content-Type: '. ($info['filemime'] ? $info['filemime'] : 'application/octet-stream'),
    'Content-Disposition: attachment; filename="'. $info['filename'] .'"',
  );
  if ($info['filesize']) {
    $headers[] = 'Content-Length: '. $info['filesize'];
  }

  ob_end_clean();

  foreach ($headers as $header) {
    // To prevent HTTP header injection, we delete new lines that are
    // not followed by a space or a tab.
    // See http://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.2
    $header = preg_replace('/\r?\n(?!\t| )/', '', $header);
    drupal_set_header($header);
  }

  // Transfer file in 1024 byte chunks to save memory usage.
  if ($fd = fopen($info['filepath'], 'rb')) {
    while (!feof($fd)) {
      print fread($fd, 1024);
    }
    fclose($fd);
  }
  else {
    drupal_not_found();
  }
  module_invoke_all('exit');
  exit();
}

/**
 * Decompress a file with the given settings.
 *  Also updates settings to reflect new file mime and file extension.
 */
function backup_migrate_file_decompress(&$file) {
  $success = FALSE;
  $temp_sql = backup_migrate_temp_file('sql');

  switch ($file['type']) {
    case "sql":
      return $file;
    break;

    case "gzip":
      $success = _backup_migrate_gzip_decode($file['filepath'], $temp_sql);
      break;

    case "bzip":
      $success = _backup_migrate_bzip_decode($file['filepath'], $temp_sql);
      break;

    case "zip":
      $success = _backup_migrate_zip_decode($file['filepath'], $temp_sql);
      break;
  }

  if ($success) {
    $file['filepath'] = $temp_sql;
    $file = backup_migrate_file_info($file['filepath']);
  }
  else {
    _backup_migrate_message("Could not decompress backup file. Please check that the file is valid.", array(), 'error');
  }
  return $success ? $file : NULL;
}

/**
 * Compress a file with the given settings.
 *  Also updates settings to reflect new file mime and file extension.
 */
function backup_migrate_file_compress($file, &$settings) {
  switch ($settings['compression']) {
    case "gzip":
      $temp_gz = backup_migrate_temp_file('gz');
      if ($success = _backup_migrate_gzip_encode($file, $temp_gz, 9)) {
        $file = $temp_gz;
        $settings['filename'] .= ".gz";
        $settings['filemime'] = 'application/x-gzip';
      }
      break;

    case "bzip":
      $temp_bz = backup_migrate_temp_file('bz');
      if ($success = _backup_migrate_bzip_encode($file, $temp_bz)) {
        $file = $temp_bz;
        $settings['filename'] .= ".bz";
        $settings['filemime'] = 'application/x-bzip';
      }
      break;

    case "zip":
      $temp_zip = backup_migrate_temp_file('zip');
      if ($success = _backup_migrate_zip_encode($file, $temp_zip, $filename)) {
        $file = $temp_zip;
        $settings['filename'] .= ".zip";
        $settings['filemime'] = 'application/zip';
      }
      break;
  }
  if (!$file) {
    _backup_migrate_message("Could not compress backup file. Try backing up without compression.", array(), 'error');
  }

  return $file;
}

/**
 * Get a temp file. Store it in the normal save path for slightly better security
 *   in shared environments.
 */
function backup_migrate_temp_file($extension = "", $delete_all = FALSE) {
  static $files = array();

  if ($delete_all) {
    _backup_migrate_temp_files_delete($files);
    $files = array();
    return;
  }
  else {
    $file = tempnam(file_directory_temp(), 'tmp_');
    $file .= $extension ? ".". $extension : "";
    $files[] = $file;
    return $file;
  }
}

/**
 * Get the basic info for a backup file on the server.
 */
function backup_migrate_file_info($path) {
  if ($type = _backup_migrate_file_get_filetype($path)) {
    $out              = $type;
    $out['filesize']  = filesize($path);
    $out['filename']  = basename($path);
    $out['file_id']   = basename($path);
    $out['filemtime'] = filemtime($path);
    $out['filectime'] = filectime($path);
    $out['filepath']  = $path;
    return $out;
  }
  return NULL;
}

/**
 * Return a list of backup filetypes.
 */
function _backup_migrate_filetypes() {
  return array(
    "sql" => array(
      "extension" => ".sql",
      "filemime" => "text/x-sql",
    ),
    "gzip" => array(
      "extension" => ".gz",
      "filemime" => "application/x-gzip",
    ),
    "bzip" => array(
      "extension" => ".bz",
      "filemime" => "application/x-bzip",
    ),
    "zip" => array(
      "extension" => ".zip",
      "filemime" => "application/zip",
    ),
  );
}

/**
 * Get the filetype info of the given file, or false if the file is not a valid type.
 */
function _backup_migrate_file_get_filetype($filename) {
  $types = _backup_migrate_filetypes();
  foreach ($types as $key => $type) {
    $extlen = drupal_strlen($type['extension']);
    if (drupal_substr($filename, -$extlen, $extlen) === $type['extension']) {
      return $type + array('type' => $key);
    }
  }
  return FALSE;
}

/**
 * Delete all temporary files.
 */
function _backup_migrate_temp_files_delete($files) {
  foreach ($files as $file) {
    if (file_exists($file)) {
      @unlink($file);
    }
  }
}

/**
 * Get the compression options as an options array for a form item.
 */
function _backup_migrate_get_compression_form_item_options() {
  $compression_options = array("none" => "None");
  if (@function_exists("gzencode")) {
    $compression_options['gzip'] = "GZip";
  }
  if (@function_exists("bzcompress")) {
    $compression_options['bzip'] = "BZip";
  }
  if (class_exists('ZipArchive')) {
    $compression_options['zip'] = "Zip";
  }
  return $compression_options;
}

/**
 * Gzip encode a file.
 */
function _backup_migrate_gzip_encode($source, $dest, $level = 9) {
  $success = FALSE;
  if (@function_exists("gzopen")) {
    if (($fp_out = gzopen($dest, 'wb'. $level)) && ($fp_in = fopen($source, 'rb'))) {
      while (!feof($fp_in)) {
        gzwrite($fp_out, fread($fp_in, 1024 * 512));
      }
      $success = TRUE;
    }
    @fclose($fp_in);
    @gzclose($fp_out);
  }
  return $success;
}

/**
 * Gzip decode a file.
 */
function _backup_migrate_gzip_decode($source, $dest) {
  $success = FALSE;
  if (@function_exists("gzopen")) {
    if (($fp_out = fopen($dest, 'wb')) && ($fp_in = gzopen($source, 'rb'))) {
      while (!feof($fp_in)) {
        fwrite($fp_out, gzread($fp_in, 1024 * 512));
      }
      $success = TRUE;
    }
    @gzclose($fp_in);
    @fclose($fp_out);
  }
  return $success;
}

/**
 * Bzip encode a file.
 */
function _backup_migrate_bzip_encode($source, $dest) {
  $success = FALSE;
  if (@function_exists("bzopen")) {
    if (($fp_out = bzopen($dest, 'w')) && ($fp_in = fopen($source, 'rb'))) {
      while (!feof($fp_in)) {
        bzwrite($fp_out, fread($fp_in, 1024 * 512));
      }
      $success = TRUE;
    }
    else {
      $error = TRUE;
    }
    @fclose($fp_in);
    @bzclose($fp_out);
  }
  return $success;
}

/**
 * Bzip decode a file.
 */
function _backup_migrate_bzip_decode($source, $dest) {
  $success = FALSE;
  if (@function_exists("bzopen")) {
    if (($fp_out = fopen($dest, 'w')) && ($fp_in = bzopen($source, 'rb'))) {
      while (!feof($fp_in)) {
        fwrite($fp_out, gzread($fp_in, 1024 * 512));
      }
      $success = TRUE;
    }
    else {
      $error = TRUE;
    }
    @bzclose($fp_in);
    @fclose($fp_out);
  }
  return $success;
}

/**
 * Zip encode a file.
 */
function _backup_migrate_zip_encode($source, $dest, $filename) {
  $success = FALSE;
  if (class_exists('ZipArchive')) {
    $zip = new ZipArchive;
    $res = $zip->open($dest, constant("ZipArchive::CREATE"));
    if ($res === TRUE) {
      $zip->addFile($source, $filename);
      $success = $zip->close();
    }
  }
  return $success;
}

/**
 * Zip decode a file.
 */
function _backup_migrate_zip_decode($source, $dest, $filename) {
  $success = FALSE;
  $zip = new ZipArchive;
  if (class_exists('ZipArchive')) {
    if (($fp_out = fopen($dest, "w")) && ($zip->open($source)) && ($fp_in = $zip->getStream($filename))) {
      while (!feof($fp_in)) {
        fwrite($fp_out, fread($fp_in, 1024 * 512));
      }
      $success = TRUE;
    }
    else {
      $error = TRUE;
    }
    @fclose($fp_in);
    @fclose($fp_out);
  }
  return $success;
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

