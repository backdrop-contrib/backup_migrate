<?php
// $Id$


/**
 * @file destinations.inc
 * All of the destination handling code needed for Backup and Migrate.
 */

/**
 * Get the available destination types.
 */
function backup_migrate_get_destination_types() {
  return module_invoke_all('backup_migrate_destination_types');
}

/**
 * Implementation of hook_backup_migrate_destination_types().
 *
 * Get the built in Backup and Migrate destination types.
 */
function backup_migrate_backup_migrate_destination_types() {
  return array(
    'file' => array(
      'type_name' => t('Server Directory'),
      'description' => t('Save the backup files to any directory on the server which the web-server can write to.'),
      'file' => drupal_get_path('module', 'backup_migrate') .'/includes/destinations.file.inc',
      'save_callback' => 'backup_migrate_destination_file_save',
      'load_callback' => 'backup_migrate_destination_file_load',
      'list_callback' => 'backup_migrate_destination_files_list',
      'delete_callback' => 'backup_migrate_destination_file_delete',
      'conf_callback' => 'backup_migrate_destination_file_conf',
      'ops' => array('scheduled backup', 'manual backup', 'restore', 'list files')
      ),
    'browser' => array(
      'type_name' => t('Browser Upload/Download'),
      'description' => t('Save the backup files to the browser (download) or upload via a form.'),
      'file' => drupal_get_path('module', 'backup_migrate') .'/includes/destinations.browser.inc',
      'save_callback' => 'backup_migrate_destination_browser_save',
      'load_callback' => 'backup_migrate_destination_browser_load',
      'ops' => array('manual backup', 'restore')
      ),
    'email' => array(
      'type_name' => t('Email'),
      'description' => t('Send the backup as an email attachment to the specified email address.'),
      'file' => drupal_get_path('module', 'backup_migrate') .'/includes/destinations.email.inc',
      'save_callback' => 'backup_migrate_destination_email_save',
      'conf_callback' => 'backup_migrate_destination_email_conf',
      'ops' => array('manual backup', 'scheduled backup')
      ),
  );
}

/**
 * Get all the available backup destination.
 *
 * @param $op
 *  The operation which will be performed on the destination. Hooks can use this
 *  to return only those destinations appropriate for the given op.
 *  Options include:
 *    'manual backup' - destinations available for manual backup
 *    'scheduled backup' - destinations available for schedules backup
 *    'list files' - destinations whose backup files can be listed
 *    'restore' - destinations whose files can be restored from
 *    'all' - all available destinations should be returned
 */
function backup_migrate_get_destinations($op = 'all') {
  static $destinations = NULL;

  // Get the list of destinations and cache them locally.
  if ($destinations === NULL) {
    $types = backup_migrate_get_destination_types();
    $all_destinations = module_invoke_all('backup_migrate_destinations');

    // Merge any type specific info such as callbacks and defaults.
    foreach ($all_destinations as $destination) {
      if ($destination['type'] && isset($types[$destination['type']])) {
        $destination = array_merge($types[$destination['type']], $destination);
      }
      $destinations[$destination['destination_id']] = $destination;
    }
  }

  if ($op == 'all') {
    return $destinations;
  }

  $out = array();
  foreach ($destinations as $key => $destination) {
    if (in_array($op, (array)$destination['ops'])) {
      $out[$key] = $destination;
    }
  }
  return $out;
}

/**
 * Implementation of hook_backup_migrate_destinations().
 *
 * Get the built in backup destinations and those in the db.
 */
function backup_migrate_backup_migrate_destinations() {
  require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/destinations.file.inc';
  $out = array();

  $out[] = array(
    'destination_id' => 'upload',
    'name' => t("Upload"),
    'type' => 'browser',
    'ops' => array('restore'),
  );

  // Manual backup only destinations
  $out[] = array(
    'destination_id' => 'download',
    'name' => t("Download"),
    'type' => 'browser',
    'ops' => array('manual backup'),
  );

  // Manual backup only destinations
  if ($location = _backup_migrate_check_destination_dir('manual')) {
    $out[] = array(
      'destination_id' => 'manual',
      'name' => $op == 'manual backup' ? t("Save to Files Directory") : t("Manual Backups Directory"),
      'type_name' => t('Manual File Directory'),
      'location' => $location,
      'save_callback' => 'backup_migrate_destination_file_save',
      'load_callback' => 'backup_migrate_destination_file_load',
      'list_callback' => 'backup_migrate_destination_files_list',
      'delete_callback' => 'backup_migrate_destination_file_delete',
      'ops' => array('manual backup', 'restore', 'list files'),
    );
  }

  // Schedule backup only destinations
  if ($location = _backup_migrate_check_destination_dir('scheduled')) {
    $out[] = array(
      'destination_id' => 'scheduled',
      'name' => $op == 'scheduled backup' ? t("Save to Files Directory") : t("Scheduled Backups Directory"),
      'type_name' => t('Scheduled File Directory'),
      'location' => $location,
      'save_callback' => 'backup_migrate_destination_file_save',
      'load_callback' => 'backup_migrate_destination_file_load',
      'list_callback' => 'backup_migrate_destination_files_list',
      'delete_callback' => 'backup_migrate_destination_file_delete',
      'ops' => array('scheduled backup', 'restore', 'list files'),
    );
  }

  // Get the saved destinations
  $result = db_query('SELECT * FROM {backup_migrate_destinations}');
  while ($destination = db_fetch_array($result)) {
    $destination['settings'] = unserialize($destination['settings']);
    $destination['db'] = TRUE;
    $out[] = $destination;
  }
  return $out;
}

/**
 * Get the destination info for the destination with the given ID, or NULL if none exists.
 */
function backup_migrate_get_destination($destination_id) {
  $destinations = backup_migrate_get_destinations('all');
  return @$destinations[$destination_id];
}

/**
 * Get a list of the backup files in the given destination.
 */
function backup_migrate_destination_get_files($destination) {
  if ($destination) {
    // Include the necessary file if specified by the download type.
    if (!empty($destination['file'])) {
      require_once './'. $destination['file'];
    }

    // Call the specified download callback.
    if (!empty($destination['list_callback'])) {
      return $destination['list_callback']($destination);
    }
  }
  return array();
}

/**
 * Load a file from a destination and return the file info.
 */
function backup_migrate_destination_get_file($destination_id, $file_id) {
  if ($destination = backup_migrate_get_destination($destination_id)) {
    // Include the necessary file if specified by the download type.
    if (!empty($destination['file'])) {
      require_once './'. $destination['file'];
    }

    // Call the specified load callback.
    if (!empty($destination['load_callback'])) {
      return $destination['load_callback']($destination, $file_id);
    }
  }
  return NULL;
}

/**
 * Send a file to the destination specified by the settings array.
 */
function backup_migrate_destination_save_file($file, &$settings) {
  if ($destination = backup_migrate_get_destination($settings['destination_id'])) {
    $settings['destination'] = $destination;

    // Include the necessary file if specified by the download type.
    if (!empty($destination['file'])) {
      require_once './'. $destination['file'];
    }

    // Call the specified download callback.
    if (!empty($destination['save_callback'])) {
      $settings['file_id'] = $settings['filename'];
      $file = $destination['save_callback']($destination, $file, $settings);
      return $file;
    }
  }
  return NULL;
}

/**
 * Delete a file in the given destination.
 */
function backup_migrate_destination_delete_file($destination_id, $file_id) {
  if ($destination = backup_migrate_get_destination($destination_id)) {
    // Include the necessary file if specified by the download type.
    if (!empty($destination['file'])) {
      require_once './'. $destination['file'];
    }

    // Call the specified delete callback.
    if (!empty($destination['delete_callback'])) {
      return $destination['delete_callback']($destination, $file_id);
    }
  }
}

/**
 * Save an existing destination, or create a new one with the given values.
 */
function backup_migrate_destination_save_destination(&$destination) {
  // Any extra settings get serialized into the settings variable.
  $settings = serialize($destination['settings']);
  if ($destination['destination_id']) {
    db_query("UPDATE {backup_migrate_destinations}
                 SET  name = '%s',
                      type = '%s',
                      location = '%s',
                      username = '%s',
                      password = '%s',
                      settings = '%s'
               WHERE destination_id = %d",
      $destination['name'],
      $destination['type'],
      $destination['location'],
      $destination['username'],
      $destination['password'],
      $settings,
      $destination['destination_id']
      );
    _backup_migrate_message('Backup destination updated: %dest', array('%dest' => $destination['name']));
  }
  else {
    $destination['destination_id'] = db_next_id('{backup_migrate_destinations}_destination_id');
    db_query("INSERT INTO {backup_migrate_destinations} (name, type , location, username, password, settings, destination_id) VALUES ('%s', '%s', '%s', '%s', '%s', '%s', %d)",
      $destination['name'],
      $destination['type'],
      $destination['location'],
      $destination['username'],
      $destination['password'],
      $settings,
      $destination['destination_id']
      );
    _backup_migrate_message('Backup destination created: %dest', array('%dest' => $destination['name']));
  }
}

/**
 * Delete a destination from the database.
 */
function backup_migrate_destination_delete_destination($destination_id) {
  $destination = backup_migrate_get_destination($destination_id);
  if ($destination && $destination['db']) {
    db_query("DELETE FROM {backup_migrate_destinations} WHERE destination_id = %d", $destination_id);
    _backup_migrate_message('Backup destination deleted: %dest', array('%dest' => $destination['name']));
  }
}

/* UI Menu Callbacks */

/**
 * List the available backup destinations destination in the UI.
 */
function backup_migrate_ui_destination_display_destinations() {
  $out = array();
  foreach (backup_migrate_get_destinations('all') as $destination_id => $destination) {
    $links = _backup_migrate_destination_get_links($destination_id);
    // If there's nothing that can be done with this destination, don't show it.
    if ($links) {
      $out[] = array(
        check_plain($destination['name']),
        $destination['type_name'],
        check_plain($destination['location']),
        implode(" | ", $links),
      );
    }
  }

  $headers = array(
    t('Name'),
    t('Type'),
    t('Location'),
    t('Operations'),
  );
  return theme("table", $headers, $out) . l(t("Create new destination..."), 'admin/content/backup_migrate/destination/add');
}

/**
 * List the backup files in the given destination.
 */
function backup_migrate_ui_destination_display_files($destination_id) {
  $out = $sort = array();
  if ($destination = backup_migrate_get_destination($destination_id)) {
    $headers = array(
      array('data' => 'Filename', 'field' => 'filename'),
      array('data' => 'Last Modified', 'field' => 'filemtime'),
      array('data' => 'Size', 'field' => 'filesize'),
      t('Operations'),
    );

    $sort_order = tablesort_get_order($headers);
    $sort_key   = $sort_order['sql'] ? $sort_order['sql'] : 'filename';
    $sort_dir   = tablesort_get_sort($headers) == 'asc' ? SORT_ASC : SORT_DESC;

    $files      = backup_migrate_destination_get_files($destination);

    // Don't display the download/delete/restore ops if they are not available for this destination.
    $can_read   = !empty($destination['load_callback']);
    $can_delete = !empty($destination['delete_callback']);

    $i          = 0;
    foreach ((array)$files as $info) {
      $sort[] = $info[$sort_key];
      $out[] = array(
        check_plain($info['filename']),
        format_date($info['filemtime'], 'small'),
        format_size($info['filesize']),
        implode(" | ", _backup_migrate_destination_get_file_links($destination_id, $info['file_id'])),
      );
    }

    array_multisort($sort, $sort_dir, $out);

    if ($out) {
      return theme("table", $headers, $out);
    }
    else {
      return t('There are no backup files to display.');
    }
  }
  drupal_goto("admin/content/backup_migrate/destination");
}

/**
 * Get a form to create a destination, or links for the available types.
 */
function backup_migrate_ui_destination_create($type = NULL) {
  $types = backup_migrate_get_destination_types();

  // If a valid type has been specified, present a form
  if (isset($types[$type])) {
    $destination = array('type' => $type, 'name' => "Untitled Destination") + $types[$type];
    $output = drupal_get_form('backup_migrate_ui_destination_configure_form', $destination);
  }
  else {
    $items = array();
    // If no (valid) node type has been provided, display a node type overview.
    foreach ($types as $key => $type) {
      if ($type['conf_callback']) {
        $type_url_str = str_replace('_', '-', $key);
        $out = '<dt>'. l($type['type_name'], "admin/content/backup_migrate/destination/add/$type_url_str", array('title' => t('Add a new @s destination.', array('@s' => $type['type_name'])))) .'</dt>';
        $out .= '<dd>'. filter_xss_admin($type['description']) .'</dd>';
        $items[] = $out;
      }
    }
    if (count($items)) {
      $output = t('Choose the type of destination you would like to create:') .'<dl>'. implode('', $items) .'</dl>';
    }
    else {
      $output = t('No destination types available.');
    }
  }
  return $output;
}

/**
 * Get a form to configure the destination.
 */
function backup_migrate_ui_destination_configure($destination_id) {
  if ($destination = backup_migrate_get_destination($destination_id)) {
    return drupal_get_form('backup_migrate_ui_destination_configure_form', $destination);
  }
  return NULL;
}

/**
 * Get a form to configure the destination.
 */
function backup_migrate_ui_destination_configure_form($destination) {
  if ($destination) {
    $form = array();
    $form['type'] = array(
      "#type" => "value",
      '#default_value' => $destination['type'],
    );
    $form['destination_id'] = array(
      "#type" => "value",
      "#default_value" => $destination['destination_id'],
    );
    $form['name'] = array(
      "#type" => "textfield",
      "#title" => t("Destination name"),
      "#default_value" => $destination['name'],
      "#required" => TRUE,
    );

    $form['submit'] = array(
      "#type" => 'submit',
      "#weight" => 99,
      "#value" => t('Save Backup Destination'),
    );
    // Include the necessary file if specified by the download type.
    if (!empty($destination['file'])) {
      require_once './'. $destination['file'];
    }

    // Call the specified load callback.
    if (!empty($destination['conf_callback'])) {
      $form = $destination['conf_callback']($destination, $form);
    }
    return $form;
  }
  return array();
}

/**
 * Submit the destination configuration form.
 */
function backup_migrate_ui_destination_configure_form_submit($form_id, $form_values) {
  // Any extra settings get serialized into the settings variable.
  $form_values['settings'] = serialize($form_values['settings']);
  backup_migrate_destination_save_destination($form_values);
  return "admin/content/backup_migrate/destination";
}

/**
 * Delete a destination.
 */
function backup_migrate_ui_destination_delete_destination($destination_id) {
  return drupal_get_form('backup_migrate_ui_destination_delete_destination_confirm', $destination_id);
}

/**
 * Ask confirmation for deletion of a destination.
 */
function backup_migrate_ui_destination_delete_destination_confirm($destination_id) {
  $form['destination_id'] = array('#type' => 'value', '#value' => $destination_id);
  $destination = backup_migrate_get_destination($destination_id);
  return confirm_form($form, t('Are you sure you want to delete the backup destination %dest?', array('%dest' => $destination['name'])), 'admin/content/backup_migrate/destinations', t('This will not delete the backup files stored in the destination.'), t('Delete'), t('Cancel'));
}

/**
 * Delete a destination after confirmation.
 */
function backup_migrate_ui_destination_delete_confirm_submit($form_id, $form_values) {
  $destination_id = $form_values['destination_id'];
  backup_migrate_destination_delete_destination($destination_id);
  return "admin/content/backup_migrate/destination";
}

/**
 * Download a file to the browser.
 */
function backup_migrate_ui_destination_download_file($destination_id, $file_id) {
  if ($info = backup_migrate_destination_get_file($destination_id, $file_id)) {
    require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/files.inc';
    backup_migrate_file_transfer($info);
  }
}

/**
 * Restore a backup file from a destination.
 */
function backup_migrate_ui_destination_restore_file($destination_id, $file_id) {
  if ($file = backup_migrate_destination_get_file($destination_id, $file_id)) {
    return drupal_get_form('backup_migrate_ui_destination_restore_file_confirm', $destination_id, $file_id);
  }
  drupal_goto(user_access('access backup files') ? "admin/content/backup_migrate/destination/files/". $destination_id : "admin/content/backup_migrate");
}

/**
 * Ask confirmation for file restore.
 */
function backup_migrate_ui_destination_restore_file_confirm($destination_id, $file_id) {
  $form['destination_id'] = array('#type' => 'value', '#value' => $destination_id);
  $form['file_id'] = array('#type' => 'value', '#value' => $file_id);
  return confirm_form($form, t('Are you sure you want to restore the database?'), "admin/content/backup_migrate/destination/files/". $destination_id, t('Are you sure you want to restore the database from the backup file %file_id? This will delete some or all of your data and cannot be undone. <strong>Always test your backups on a non-production server!</strong>', array('%file_id' => $file_id)), t('Restore'), t('Cancel'));
}

/**
 * Do the file restore.
 */
function backup_migrate_ui_destination_restore_file_confirm_submit($form_id, $form_values) {
  $destination_id = $form_values['destination_id'];
  $file_id = $form_values['file_id'];
  if ($destination_id && $file_id) {
    backup_migrate_perform_restore($destination_id, $file_id);
    watchdog('backup_migrate', t('Database restored from %file_id', array('%file_id' => $file_id)));
  }
  return user_access('access backup files') ? "admin/content/backup_migrate/destination/files/". $destination_id : "admin/content/backup_migrate";
}

/**
 * Menu callback to delete a file from a destination.
 */
function backup_migrate_ui_destination_delete_file($destination_id, $file_id) {
  if ($file = backup_migrate_destination_get_file($destination_id, $file_id)) {
    return drupal_get_form('backup_migrate_ui_destination_delete_file_confirm', $destination_id, $file_id);
  }
  drupal_goto("admin/content/backup_migrate");
}

/**
 * Ask confirmation for file deletion.
 */
function backup_migrate_ui_destination_delete_file_confirm($destination_id, $file_id) {
  $form['destination_id'] = array('#type' => 'value', '#value' => $destination_id);
  $form['file_id'] = array('#type' => 'value', '#value' => $file_id);
  return confirm_form($form, t('Are you sure you want to delete the backup file?'), 'admin/content/backup_migrate/destination/files/'. $destination_id, t('Are you sure you want to delete the backup file %file_id? <strong>This action cannot be undone.<strong>', array('%file_id' => $file_id)), t('Delete'), t('Cancel'));
}

/**
 * Delete confirmed, perform the delete.
 */
function backup_migrate_ui_destination_delete_file_confirm_submit($form_id, $form_values) {
  if (user_access('delete backup files')) {
    $destination_id = $form_values['destination_id'];
    $file_id = $form_values['file_id'];
    backup_migrate_destination_delete_file($destination_id, $file_id);
    $message = t('Database backup file deleted: %file_id', array('%file_id' => $file_id));
    watchdog('backup_migrate', $message);
    drupal_set_message($message);
  }
  return user_access('access backup files') ? "admin/content/backup_migrate/files" : "admin/content/backup_migrate";
}

/* Utilities */

/**
 * Get the destination options as an options array for a form item.
 */
function _backup_migrate_get_destination_form_item_options($op) {
  $out = array();
  foreach (backup_migrate_get_destinations($op) as $key => $destination) {
    $out[$key] = $destination['name'];
  }
  return $out;
}

/**
 * Get the action links for a destination.
 */
function _backup_migrate_destination_get_links($destination_id) {
  $out = array();
  if ($destination = backup_migrate_get_destination($destination_id)) {
    // Don't display the download/delete/restore ops if they are not available for this destination.
    $can_list = !empty($destination['list_callback']);
    $can_conf = !empty($destination['conf_callback']);

    if ($can_list && user_access("access backup files")) {
      $out[] = l(t("List files"), "admin/content/backup_migrate/destination/files/". $destination['destination_id']);
    }
    if ($can_conf) {
      $out[] = l(t("Configure..."), "admin/content/backup_migrate/destination/configure/". $destination['destination_id']);
    }
    if ($can_conf) {
      $out[] = l(t("Delete..."), "admin/content/backup_migrate/destination/delete/". $destination['destination_id']);
    }
  }
  return $out;
}

/**
 * Get the action links for a file on a given destination.
 */
function _backup_migrate_destination_get_file_links($destination_id, $file_id) {
  $out = array();
  if ($destination = backup_migrate_get_destination($destination_id)) {
    // Don't display the download/delete/restore ops if they are not available for this destination.
    $can_read = !empty($destination['load_callback']);
    $can_delete = !empty($destination['delete_callback']);

    if ($can_read && user_access("access backup files")) {
      $out[] = l(t("Download"), "admin/content/backup_migrate/destination/downloadfile/". $destination_id .'/'. $file_id);
    }
    if ($can_read && user_access("restore from backup")) {
      $out[] = l(t("Restore..."), "admin/content/backup_migrate/destination/restorefile/". $destination_id .'/'. $file_id);
    }
    if ($can_read && user_access("delete backup files")) {
      $out[] = l(t("Delete..."), "admin/content/backup_migrate/destination/deletefile/". $destination_id .'/'. $file_id);
    }
  }
  return $out;
}

