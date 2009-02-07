<?php
// $Id$


/**
 * @file
 * Functions to handle the browser upload/download backup destination.
 */

/**
 * A destination type for browser upload/download.
 *
 * @ingroup backup_migrate_destinations
 */
class backup_migrate_destination_browser_upload extends backup_migrate_destination {
  var $supported_ops = array('restore');
  function __construct() {
    $params = array();
    $params['name'] = "Upload";
    $params['destination_id'] = 'upload';
    parent::__construct($params);
  }

  /**
   * File load destination callback.
   */
  function load_file($file_id) {
    require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/files.inc';
    if ($file = file_save_upload('backup_migrate_restore_upload')) {
      $out = new backup_file($file->filepath);
      backup_migrate_temp_files_add($file->filepath);
      return $out;
    }
    return NULL;
  }
}

/**
 * The built in default files directories. This is more or less a legacy holdover from v1.
 * But they serve a purpose as a meaningful default.
 */
class backup_migrate_destination_browser_download extends backup_migrate_destination {
  var $supported_ops = array('manual backup');
  function __construct() {
    $params = array();
    $params['name'] = "Download";
    $params['destination_id'] = 'download';
    parent::__construct($params);
  }

  /**
   * File save destination callback.
   */
  function save_file($file, $settings) {
    require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/files.inc';
    $file->transfer();
  }
}

