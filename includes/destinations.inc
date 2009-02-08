<?php
// $Id$


/**
 * @file
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
      'description' => t('Save the backup files to any directory on the server which the web-server can write to.'),
      'file' => drupal_get_path('module', 'backup_migrate') .'/includes/destinations.file.inc',
      'class' => 'backup_migrate_destination_files',
      'type_name' => t('Server Directory'),
      'can_create' => TRUE,
    ),
    'file_manual' => array(
      'file' => drupal_get_path('module', 'backup_migrate') .'/includes/destinations.file.inc',
      'class' => 'backup_migrate_destination_files_manual',
    ),
    'file_scheduled' => array(
      'file' => drupal_get_path('module', 'backup_migrate') .'/includes/destinations.file.inc',
      'class' => 'backup_migrate_destination_files_scheduled',
    ),
    'browser_download' => array(
      'file' => drupal_get_path('module', 'backup_migrate') .'/includes/destinations.browser.inc',
      'class' => 'backup_migrate_destination_browser_download',
    ),
    'browser_upload' => array(
      'file' => drupal_get_path('module', 'backup_migrate') .'/includes/destinations.browser.inc',
      'class' => 'backup_migrate_destination_browser_upload',
    ),
    'db' => array(
      'type_name' => t('Database'),
      'description' => t('Import the dump directly into another MySQL database.'),
      'file' => drupal_get_path('module', 'backup_migrate') .'/includes/destinations.db.inc',
      'class' => 'backup_migrate_destination_db',
      'can_create' => TRUE,
    ),
    'db_default' => array(
      'file' => drupal_get_path('module', 'backup_migrate') .'/includes/destinations.db.inc',
      'class' => 'backup_migrate_destination_db_defaults',
    ),
    'ftp' => array(
      'description' => t('Save the backup files to any a directory on an FTP server.'),
      'file' => drupal_get_path('module', 'backup_migrate') .'/includes/destinations.ftp.inc',
      'class' => 'backup_migrate_destination_ftp',
      'type_name' => t('FTP Directory'),
      'can_create' => TRUE,
    ),
    's3' => array(
      'description' => t('Save the backup files to a bucket on your !link.', array('!link' => l(t('Amazon S3 account'), 'http://aws.amazon.com/s3/'))),
      'file' => drupal_get_path('module', 'backup_migrate') .'/includes/destinations.s3.inc',
      'class' => 'backup_migrate_destination_s3',
      'type_name' => t('Amazon S3 Bucket'),
      'can_create' => TRUE,
    ),
    'email' => array(
      'type_name' => t('Email'),
      'description' => t('Send the backup as an email attachment to the specified email address.'),
      'file' => drupal_get_path('module', 'backup_migrate') .'/includes/destinations.email.inc',
      'class' => 'backup_migrate_destination_email',
      'can_create' => TRUE,
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
    // This code looks a lot like module_invoke_all, but array_merge_recursive
    // reindexes numeric array items so I've reimplemented it with array_merge.
    $destinations = array();
    foreach (module_implements('backup_migrate_destinations') as $module) {
      $function = $module .'_backup_migrate_destinations';
      $result = (array)call_user_func($function);
      foreach ($result as $key => $val) {
        $destinations[$key] = $val;
      }
    }
  }

  if ($op == 'all') {
    return $destinations;
  }

  $out = array();
  foreach ($destinations as $key => $destination) {
    if ($destination->op($op)) {
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
  $out              = array();

  $out['manual']    = backup_migrate_create_destination('file_manual');
  $out['scheduled'] = backup_migrate_create_destination('file_scheduled');
  $out['upload']    = backup_migrate_create_destination('browser_upload');
  $out['download']  = backup_migrate_create_destination('browser_download');

  // Expose the configured databases as sources.
  global $db_url;
  $urls = is_array($db_url) ? $db_url : array('default' => $db_url);
  foreach ((array)$urls as $key => $url) {
    if ($destination = backup_migrate_create_destination('db_default', $key)) {
      $out[$destination->id()] = $destination;
    }
  }

  // Get the saved destinations.
  $result = db_query('SELECT * FROM {backup_migrate_destinations}');
  while ($destination = db_fetch_array($result)) {
    if ($destination = backup_migrate_create_destination($destination['type'], $destination)) {
      $out[$destination->id()] = $destination;
    }
  }

  return $out;
}

/**
 * Create a destination object of the given type with the given params.
 */
function backup_migrate_create_destination($destination_type, $params = array()) {
  $out = NULL;
  $types = backup_migrate_get_destination_types();
  if ($type = @$types[$destination_type]) {
    // Include the necessary file if specified by the type.
    if (!empty($type['file'])) {
      require_once './'. $type['file'];
    }
    $params['type'] = $destination_type;
    $out = new $type['class']($params);
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
 * Load a file from a destination and return the file info.
 */
function backup_migrate_destination_get_file($destination_id, $file_id) {
  if ($destination = backup_migrate_get_destination($destination_id)) {
    return $destination->load_file($file_id);
  }
  return NULL;
}

/**
 * Send a file to the destination specified by the settings array.
 */
function backup_migrate_destination_save_file($file, &$settings) {
  if ($destination = backup_migrate_get_destination($settings['destination_id'])) {
    $file = $destination->save_file($file, $settings);
    return $file;
  }
  return NULL;
}

/**
 * Delete a file in the given destination.
 */
function backup_migrate_destination_delete_file($destination_id, $file_id) {
  if ($destination = backup_migrate_get_destination($destination_id)) {
    return $destination->delete_file($file_id);
  }
}

/**
 * Save an existing destination, or create a new one with the given values.
 */
function backup_migrate_destination_save_destination($destination) {
  $info = $destination->to_array();
  drupal_write_record('backup_migrate_destinations', $info, $info['destination_id'] ? array('destination_id') : array());
  _backup_migrate_message('Backup destination saved: %dest', array('%dest' => $destination->name()));
}

/**
 * Delete a destination from the database.
 */
function backup_migrate_destination_delete_destination($destination_id) {
  if ($destination = backup_migrate_get_destination($destination_id)) {
    db_query("DELETE FROM {backup_migrate_destinations} WHERE destination_id = %d", $destination->id());
    _backup_migrate_message('Backup destination deleted: %dest', array('%dest' => $destination->name()));
  }
}

/**
 * Get the action links for a file on a given destination.
 */
function _backup_migrate_destination_get_file_links($destination_id, $file_id) {
  $out = array();
  if ($destination = backup_migrate_get_destination($destination_id)) {
    $out = $destination->get_file_links($file_id);
  }
  return $out;
}

/* UI Menu Callbacks */

/**
 * List the available backup destinations destination in the UI.
 */
function backup_migrate_ui_destination_display_destinations() {
  $out = array();
  foreach (backup_migrate_get_destinations('all') as $destination_id => $destination) {
    // If there's nothing that can be done with this destination, don't show it.
    if ($links = $destination->get_destination_links()) {
      $out[] = array(
        check_plain($destination->name()),
        $destination->type_name(),
        check_plain($destination->display_location()),
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
  return theme("table", $headers, $out) . l(t("Create new destination"), 'admin/content/backup_migrate/destination/add');
}

/**
 * List the backup files in the given destination.
 */
function backup_migrate_ui_destination_display_files($destination_id) {
  $out = $sort = array();
  if ($destination = backup_migrate_get_destination($destination_id)) {
    drupal_set_title(t('%title Files', array('%title' => $destination->name())));
    $headers = array(
      array('data' => 'Filename', 'field' => 'filename'),
      array('data' => 'Date', 'field' => 'filetime'),
      array('data' => 'Size', 'field' => 'filesize'),
      t('Operations'),
    );

    $sort_order = tablesort_get_order($headers);
    $sort_key   = $sort_order['sql'] ? $sort_order['sql'] : 'filename';
    $sort_dir   = tablesort_get_sort($headers) == 'asc' ? SORT_ASC : SORT_DESC;

    $files      = $destination->list_files();

    $i          = 0;
    foreach ((array)$files as $file) {
      $info = $file->info();
      // Show only files that can be restored from.
      if ($file->is_recognized_type()) {
        $sort[] = $info[$sort_key];
        $out[] = array(
          check_plain($info['filename']),
          format_date($info['filetime'], 'small'),
          format_size($info['filesize']),
          implode(" | ", $destination->get_file_links($file->file_id())),
        );
      }
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
    $destination = backup_migrate_create_destination($type);
    $output = drupal_get_form('backup_migrate_ui_destination_configure_form', $destination);
  }
  else {
    $items = array();
    // If no (valid) node type has been provided, display a node type overview.
    foreach ($types as $key => $type) {
      if (@$type['can_create']) {
        $type_url_str = str_replace('_', '-', $key);
        $out = '<dt>'. l($type['type_name'], "admin/content/backup_migrate/destination/add/$type_url_str", array('attributes' => array('title' => t('Add a new @s destination.', array('@s' => $type['type_name']))))) .'</dt>';
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
function backup_migrate_ui_destination_configure_form(&$form_state, $destination) {
  if ($destination) {
    $form = array();
    $form['destination'] = array(
      "#type" => "value",
      '#default_value' => $destination,
    );
    $form['destination_id'] = array(
      "#type" => "value",
      '#default_value' => $destination->id(),
    );
    $form['name'] = array(
      "#type" => "textfield",
      "#title" => t("Destination name"),
      "#default_value" => $destination->name(),
      "#required" => TRUE,
    );
    $form['actions'] = array('#prefix' => '<div class="container-inline">', '#suffix' => '</div>', '#weight' => 99);
    $form['actions']['submit'] = array('#type' => 'submit', '#value' => t('Save Backup Destination'));
    $form['actions']['cancel'] = array('#value' => l(t('Cancel'), 'admin/content/backup_migrate/destination'));

    // Call the settings form callback.
    $form = $destination->settings_form($form);
    return $form;
  }
  return array();
}

/**
 * Submit the destination configuration form.
 */
function backup_migrate_ui_destination_configure_form_validate($form, &$form_state) {
  $values = $form_state['values'];

  // Post process the settings.
  $destination = $values['destination'];
  $values = $destination->settings_form_validate($values);
}

/**
 * Submit the destination configuration form.
 */
function backup_migrate_ui_destination_configure_form_submit($form, &$form_state) {
  $values = $form_state['values'];

  // Post processthe settings.
  $destination = $values['destination'];
  $values['type'] = $destination->type();
  $values = $destination->settings_form_submit($values);
  $destination->from_array($values);

  backup_migrate_destination_save_destination($destination);

  $form_state['redirect'] = "admin/content/backup_migrate/destination";
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
function backup_migrate_ui_destination_delete_destination_confirm(&$form_state, $destination_id) {
  $form['destination_id'] = array('#type' => 'value', '#value' => $destination_id);
  $destination = backup_migrate_get_destination($destination_id);
  return confirm_form($form, t('Are you sure you want to delete the backup destination %dest?', array('%dest' => $destination->name())), 'admin/content/backup_migrate/destination', t('This will not delete the backup files stored in the destination.'), t('Delete'), t('Cancel'));
}

/**
 * Delete a destination after confirmation.
 */
function backup_migrate_ui_destination_delete_destination_confirm_submit($form, &$form_state) {
  if ($form_state['values']['confirm']) {
    $destination_id = $form_state['values']['destination_id'];
    backup_migrate_destination_delete_destination($destination_id);
  }
  $form_state['redirect'] = "admin/content/backup_migrate/destination";
}

/**
 * Download a file to the browser.
 */
function backup_migrate_ui_destination_download_file($destination_id, $file_id) {
  if ($file = backup_migrate_destination_get_file($destination_id, $file_id)) {
    require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/files.inc';
    $file->transfer();
  }
}

/**
 * Restore a backup file from a destination.
 */
function backup_migrate_ui_destination_restore_file($destination_id, $file_id) {
  if ($file = backup_migrate_destination_get_file($destination_id, $file_id)) {
    return drupal_get_form('backup_migrate_ui_destination_restore_file_confirm', $destination_id, $file_id);
  }
  drupal_goto(user_access('access backup files') ? "admin/content/backup_migrate/destination/list/files/". $destination_id : "admin/content/backup_migrate");
}

/**
 * Ask confirmation for file restore.
 */
function backup_migrate_ui_destination_restore_file_confirm(&$form_state, $destination_id, $file_id) {
  $form['destination_id'] = array('#type' => 'value', '#value' => $destination_id);
  $form['file_id'] = array('#type' => 'value', '#value' => $file_id);
  return confirm_form($form, t('Are you sure you want to restore the database?'), "admin/content/backup_migrate/destination/list/files/". $destination_id, t('Are you sure you want to restore the database from the backup file %file_id? This will delete some or all of your data and cannot be undone. <strong>Always test your backups on a non-production server!</strong>', array('%file_id' => $file_id)), t('Restore'), t('Cancel'));
}

/**
 * Do the file restore.
 */
function backup_migrate_ui_destination_restore_file_confirm_submit($form, &$form_state) {
  $destination_id = $form_state['values']['destination_id'];
  $file_id = $form_state['values']['file_id'];
  if ($destination_id && $file_id) {
    backup_migrate_perform_restore($destination_id, $file_id);
  }
  $redir = user_access('access backup files') ? "admin/content/backup_migrate/destination/list/files/". $destination_id : "admin/content/backup_migrate";
  $form_state['redirect'] = $redir;
}

/**
 * Menu callback to delete a file from a destination.
 */
function backup_migrate_ui_destination_delete_file($destination_id, $file_id) {
  return drupal_get_form('backup_migrate_ui_destination_delete_file_confirm', $destination_id, $file_id);
  drupal_goto("admin/content/backup_migrate");
}

/**
 * Ask confirmation for file deletion.
 */
function backup_migrate_ui_destination_delete_file_confirm(&$form_state, $destination_id, $file_id) {
  $form['destination_id'] = array('#type' => 'value', '#value' => $destination_id);
  $form['file_id'] = array('#type' => 'value', '#value' => $file_id);
  return confirm_form($form, t('Are you sure you want to delete the backup file?'), 'admin/content/backup_migrate/destination/list/files/'. $destination_id, t('Are you sure you want to delete the backup file %file_id? <strong>This action cannot be undone.</strong>', array('%file_id' => $file_id)), t('Delete'), t('Cancel'));
}

/**
 * Delete confirmed, perform the delete.
 */
function backup_migrate_ui_destination_delete_file_confirm_submit($form, &$form_state) {
  if (user_access('delete backup files')) {
    $destination_id = $form_state['values']['destination_id'];
    $file_id = $form_state['values']['file_id'];
    backup_migrate_destination_delete_file($destination_id, $file_id);
    _backup_migrate_message('Database backup file deleted: %file_id', array('%file_id' => $file_id));
  }
  $form_state['redirect'] = user_access('access backup files') ? "admin/content/backup_migrate/destination/list/files/". $destination_id : "admin/content/backup_migrate";
}

/* Utilities */

/**
 * Get the destination options as an options array for a form item.
 */
function _backup_migrate_get_destination_form_item_options($op) {
  $out = array();
  foreach (backup_migrate_get_destinations($op) as $key => $destination) {
    $out[$key] = $destination->name();
  }
  return $out;
}

/**
 * A base class for creating destinations.
 */
class backup_migrate_destination {
  var $dest_name = "";
  var $dest_type = "";
  var $dest_location = "";
  var $dest_user = "";
  var $dest_password = "";
  var $dest_settings = array();
  var $dest_id = NULL;
  var $dest_url = array();
  var $supported_ops = array();

  /**
   * Constructor, set the basic info pulled from the db.
   */
  function __construct($params = array()) {
    $this->from_array($params);
  }

  function ops() {
    return $this->supported_ops;
  }

  /**
   * Does this destination support the given operation.
   */
  function op($op) {
    $ops = (array)$this->ops();
    return in_array($op, $ops);
  }

  /**
   * Some basic getters and setters. Not really useful until they are overriden.
   */
  function id() {
    return $this->dest_id;
  }

  function set_id($id) {
    $this->dest_id = $id;
  }

  function name() {
    return $this->dest_name;
  }

  function set_name($name) {
    $this->dest_name = $name;
  }

  function type() {
    return $this->dest_type;
  }

  function set_type($type) {
    $this->dest_type = $type;
  }

  function location() {
    return $this->dest_location;
  }

  function display_location() {
    return $this->location();
  }

  function set_location($location) {
    $this->dest_location = $location;
  }

  function username() {
    return $this->dest_user;
  }

  function password() {
    return $this->dest_password;
  }

  function set_user_password($user, $password) {
    $this->dest_password = $password;
  }

  function settings() {
    return $this->dest_settings;
  }

  function set_settings($settings) {
    $this->dest_settings = $settings;
  }

  /**
   * Get the type name of this destination for display to the user.
   */
  function type_name() {
    if ($type = $this->type()) {
      $types = backup_migrate_get_destination_types();
      return isset($types[$type]['type_name']) ? $types[$type]['type_name'] : $type;
    }
  }

  /**
   * Save the given file to the destination.
   */
  function save_file($file, $settings) {
    // This must be overriden.
    return $file;
  }

  /**
   * Load the file with the given destination specific id and return as a backup_file object.
   */
  function load_file($file_id) {
    // This must be overriden.
    return NULL;
  }

  /**
   * List all the available files in the given destination with their destination specific id.
   */
  function list_files() {
    return array();
  }

  /**
   * Delete the file with the given destination specific id.
   */
  function delete_file($file_id) {
    // This must be overriden.
  }

  /**
   * Get the action links for a destination.
   */
  function get_destination_links() {
    $out = array();
    // Don't display the download/delete/restore ops if they are not available for this destination.
    $can_list       = $this->op('list files');
    $can_conf       = $this->op('configure');

    $destination_id = $this->id();
    if ($can_list && user_access("access backup files")) {
      $out[] = l(t("list files"), "admin/content/backup_migrate/destination/list/files/". $destination_id);
    }
    if ($can_conf) {
      $out[] = l(t("configure"), "admin/content/backup_migrate/destination/list/configure/". $destination_id);
    }
    if (is_numeric($this->id())) {
      $out[] = l(t("delete"), "admin/content/backup_migrate/destination/list/delete/". $destination_id);
    }
    return $out;
  }

  /**
   * Get the action links for a file on a given destination.
   */
  function get_file_links($file_id) {
    $out = array();

    // Don't display the download/delete/restore ops if they are not available for this destination.
    $can_read       = $this->can_read_file($file_id);
    $can_delete     = $this->can_delete_file($file_id);

    $destination_id = $this->id();
    if ($can_read && user_access("access backup files")) {
      $out[] = l(t("download"), "admin/content/backup_migrate/destination/downloadfile/". $destination_id .'/'. $file_id);
    }
    if ($can_read && user_access("restore from backup")) {
      $out[] = l(t("restore"), "admin/content/backup_migrate/destination/list/restorefile/". $destination_id .'/'. $file_id);
    }
    if ($can_delete && user_access("delete backup files")) {
      $out[] = l(t("delete"), "admin/content/backup_migrate/destination/list/deletefile/". $destination_id .'/'. $file_id);
    }
    return $out;
  }

  /**
   * Determine if we can read the given file.
   */
  function can_read_file($file_id) {
    return $this->op('restore');
  }

  /**
   * Determine if we can read the given file.
   */
  function can_delete_file($file_id) {
    return $this->op('delete');
  }

  /**
   * Get the form for the settings for this destination type.
   */
  function settings_default() {
    return array();
  }

  /**
   * Get the form for the settings for this destination.
   */
  function settings_form($form) {
    return $form;
  }

  /**
   * Validate the form for the settings for this destination.
   */
  function settings_form_validate($form_values) {
  }

  /**
   * Submit the settings form. Any values returned will be saved.
   */
  function settings_form_submit($form_values) {
    return $form_values;
  }

  /**
   * Load an existing destination from an array.
   */
  function from_array($params) {
    $this->set_type(isset($params['type']) ? $params['type'] : "");
    $this->set_id(isset($params['destination_id']) ? $params['destination_id'] : "");
    $this->set_name(isset($params['name']) ? $params['name'] : "Untitled Destination");
    $this->set_location(isset($params['location']) ? $params['location'] : "");
    $this->set_settings(isset($params['settings']) ? $params['settings'] : array());
  }

  /**
   * Return as an array of values.
   */
  function to_array() {
    // Any extra settings get serialized into the settings variable.
    $info = array(
      'type' => $this->type(),
      'destination_id' => $this->id(),
      'name' => $this->name(),
      'location' => $this->location(),
      'settings' => serialize($this->settings()),
    );
    return $info;
  }
}

/**
 * A base class for creating destinations.
 */
class backup_migrate_destination_remote extends backup_migrate_destination {
  /**
   * The location is a URI so parse it and store the parts.
   */
  function location() {
    return $this->url(FALSE);
  }

  /**
   * The location to display is the url without the password.
   */
  function display_location() {
    return $this->url(TRUE);
  }

  /**
   * Return the location with the password.
   */
  function set_location($location) {
    $this->set_url($location);
  }

  /**
   * Get a url from the parts.
   */
  function url($hide_password = TRUE) {
    return $this->glue_url($this->dest_url, $hide_password);
  }

  /**
   * Glue a URLs component parts back into a URL.
   */
  function glue_url($parts, $hide_password = TRUE) {
    // Obscure the password if we need to.
    $parts['pass'] = $hide_password ? "" : $parts['pass'];

    // Assemble the URL.
    $out = "";
    $out .= $parts['scheme'] .'://';
    $out .= $parts['user'] ? urlencode($parts['user']) : '';
    $out .= ($parts['user'] && $parts['pass']) ? ":". urlencode($parts['pass']) : '';
    $out .= ($parts['user'] || $parts['pass']) ? "@" : "";
    $out .= $parts['host'];
    $out .= "/". $parts['path'];
    return $out;
  }

  /**
   * Break a URL into it's component parts.
   */
  function set_url($url) {
    $parts          = (array)parse_url($url);
    $parts['user'] = urldecode($parts['user']);
    $parts['pass'] = urldecode($parts['pass']);
    $parts['path'] = urldecode($parts['path']);
    $parts['path']  = ltrim($parts['path'], "/");
    $this->dest_url = $parts;
  }

  /**
   * Destination configuration callback.
   */
  function settings_form($form) {
    $form = parent::settings_form($form);
    $form['scheme'] = array(
      "#type" => "select",
      "#title" => t("Scheme"),
      "#default_value" => @$this->dest_url['scheme'] ? $this->dest_url['scheme'] : 'mysql',
      "#required" => TRUE,
      "#options" => array($GLOBALS['db_type'] => $GLOBALS['db_type']),
    );
    $form['host'] = array(
      "#type" => "textfield",
      "#title" => t("Host"),
      "#default_value" => @$this->dest_url['host'] ? $this->dest_url['host'] : 'localhost',
      "#required" => TRUE,
    );
    $form['path'] = array(
      "#type" => "textfield",
      "#title" => t("Path"),
      "#default_value" => @$this->dest_url['path'],
      "#required" => TRUE,
    );
    $form['user'] = array(
      "#type" => "textfield",
      "#title" => t("Username"),
      "#default_value" => @$this->dest_url['user'],
      "#required" => TRUE,
    );
    $form['pass'] = array(
      "#type" => "password",
      "#title" => t("Password"),
      "#default_value" => @$this->dest_url['pass'],
      '#description' => '',
    );
    if (@$this->dest_url['pass']) {
      $form['old_password'] = array(
        "#type" => "value",
        "#value" => @$this->dest_url['pass'],
      );
      $form['pass']["#description"] .= t(' You do not need to enter a password unless you wish to change the currently saved password.');
    }
    return $form;
  }

  /**
   * Validate the configuration form. Make sure the email address is valid.
   */
  function settings_form_submit($values) {
    $values['pass'] = $values['pass'] ? $values['pass'] : $values['old_password'];
    $values['location'] = $this->glue_url($values, FALSE);
    return $values;
  }
}

