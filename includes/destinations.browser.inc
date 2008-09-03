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
  $info = backup_migrate_file_info($file);
  $info['filename'] = $settings['filename'];
  backup_migrate_file_transfer($info);
}

/**
 * Browser upload destination callback.
 */
function backup_migrate_destination_browser_load($destination, $file_id) {
  require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/files.inc';
  if ($file = file_save_upload('backup_migrate_restore_upload')) {
    $filepath = backup_migrate_temp_file($file->filename);
    rename($file->filepath, $filepath);
    $out = backup_migrate_file_info($filepath);
    $out['filename'] = $file->filename;
    return $out;
  }
  return NULL;
}

