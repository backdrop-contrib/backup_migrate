<?php
// $Id$


/**
 * @file
 * Functions to handle the browser upload/download backup destination.
 */

/**
 * Browser download destination callback.
 */
function backup_migrate_destination_browser_save($destination, $file, $settings) {
  require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/files.inc';
  $file->transfer();
}

/**
 * Browser upload destination callback.
 */
function backup_migrate_destination_browser_load($destination, $file_id) {
  require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/files.inc';
  if ($file = file_save_upload('backup_migrate_restore_upload')) {
    $out = new backup_file($file->filepath);
    backup_migrate_temp_files_add($file->filepath);
    return $out;
  }
  return NULL;
}

