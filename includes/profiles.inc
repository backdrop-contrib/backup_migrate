<?php
// $Id$


/**
 * @file
 * All of the settings profiles handling code for Backup and Migrate.
 */

/**
 * Get all the available backup profiles.
 */
function backup_migrate_get_profiles() {
  require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/filters.inc';
  static $profiles = NULL;

  // Get the list of profiles and cache them locally.
  if ($profiles === NULL) {
    $profiles = array();
    $all_profiles = module_invoke_all('backup_migrate_profiles');
    // Reindex since module_invoke_all stomps on numerical indices (thanks to array_merge).
    foreach ($all_profiles as $profile) {
      // Set the default values for filter setting which don't exist in the profile.
      $profile['filters'] = $profile['filters'] + backup_migrate_filters_settings_default('backup');
      $profiles[$profile['profile_id']] = $profile;
    }
  }
  return $profiles;
}

/**
 * Get the profile info for the profile with the given ID, or NULL if none exists.
 */
function backup_migrate_get_profile($profile_id) {
  $profiles = backup_migrate_get_profiles();
  return @$profiles[$profile_id];
}

/**
 * Implementation of hook_backup_migrate_profiles().
 *
 * Get the backup profiles stored in the db.
 */
function backup_migrate_backup_migrate_profiles() {
  $out = array();

  // Get the default profiles from settings.php
  $out += (array)variable_get('backup_migrate_profiles_defaults', array());

  // Get the saved profiless
  $result = db_query('SELECT * FROM {backup_migrate_profiles}');
  while ($profile = db_fetch_array($result)) {
    $profile['db'] = TRUE;
    $profile['exclude_tables'] = unserialize($profile['exclude_tables']);
    $profile['nodata_tables'] = unserialize($profile['nodata_tables']);
    $profile['filters'] = unserialize($profile['filters']);
    $out[$profile['profile_id']] = $profile;
  }
  return $out;
}

/**
 * Update an existing profile or create a new one.
 */
function backup_migrate_profile_save_profile($profile) {
  $profile['nodata_tables'] = serialize(array_filter((array)$profile['nodata_tables']));
  $profile['exclude_tables'] = serialize(array_filter((array)$profile['exclude_tables']));
  $profile['filters'] = serialize((array)$profile['filters']);

  drupal_write_record('backup_migrate_profiles', $profile, @$profile['profile_id'] ? array('profile_id') : array());
  _backup_migrate_message('Profile setting saved: %profile', array('%profile' => $profile['name']));
  return $profile;
}

/**
 * Delete a saved profile from the database.
 */
function backup_migrate_profile_delete_profile($profile_id) {
  $profile = backup_migrate_get_profile($profile_id);
  if ($profile && $profile['db']) {
    db_query("DELETE FROM {backup_migrate_profiles} WHERE profile_id = %d", $profile_id);
    _backup_migrate_message('Profile deleted: %profile', array('%profile' => $profile['name']));
  }
}

/**
 * List the the available profiles in the UI.
 */
function backup_migrate_ui_profile_display_profiles() {
  require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/destinations.inc';
  $out = array();
  foreach (backup_migrate_get_profiles() as $profile) {
    $source = backup_migrate_get_destination($profile['source_id']);
    $row = array(
      check_plain($profile['name']),
      $source ? $source->name() : t("Missing"),
      check_plain($profile['filename']),
      $profile['exclude_tables'] ? count($profile['exclude_tables']) : t("No tables excluded"),
      $profile['nodata_tables'] ? count($profile['nodata_tables']) : t("No data omitted"),
      implode(" | ", _backup_migrate_profile_get_links($profile['profile_id'])),
    );

    if (!@$profile['enabled']) {
      foreach ($row as $key => $field) {
        $row[$key] = array('data' => $field, 'class' => 'profile-list-disabled');
      }
    }
    $out[] = $row;
  }

  $headers = array(
    t('Name'),
    t('Source'),
    t('Filename'),
    t('Excluded Tables'),
    t('No Data Tables'),
    t('Operations'),
  );
  drupal_add_css(drupal_get_path('module', 'backup_migrate') .'/backup_migrate.css');
  if ($out) {
    $out = theme("table", $headers, $out);
  }
  else {
    $out = t('There are no profiles to display.');
  }
  if (user_access('administer backup and migrate')) {
    $out .= ' '. l(t("Create new profile"), 'admin/content/backup_migrate/profile/add');
  }
  return $out;
}

/**
 * Get a form to create a new profile.
 */
function backup_migrate_ui_profile_create() {
  $profile = _backup_migrate_profile_default_profile() + array('name' => t("Untitled Profile"), 'source' => 'db_url:default');
  $output = drupal_get_form('backup_migrate_ui_profile_configure_form', $profile);
  return $output;
}

/**
 * Get a form to configure the profile.
 */
function backup_migrate_ui_profile_configure($profile_id = NULL) {
  if ($profile = backup_migrate_get_profile($profile_id)) {
    return drupal_get_form('backup_migrate_ui_profile_configure_form', $profile);
  }
  drupal_goto('admin/content/backup_migrate/profile');
}

/**
 * Get a form to configure the profile.
 */
function backup_migrate_ui_profile_configure_form(&$form_state, $profile) {
  if ($profile) {
    $form = array();
    $form['name'] = array(
      "#type" => "textfield",
      "#title" => t("Profile Name"),
      "#default_value" => $profile['name'],
    );

    $form += _backup_migrate_ui_backup_settings_form($profile);
    $form['profile_id'] = array(
      '#type' => 'value',
      '#value' => @$profile['profile_id'],
    );
    $form['actions'] = array('#prefix' => '<div class="container-inline">', '#suffix' => '</div>', '#weight' => 99);
    $form['actions']['submit'] = array('#type' => 'submit', '#value' => t('Save Profile'));
    $form['actions']['cancel'] = array('#value' => l(t('Cancel'), 'admin/content/backup_migrate/profile'));
    return $form;
  }
  return array();
}

/**
 * Submit the profile configuration form.
 */
function backup_migrate_ui_profile_configure_form_submit($form, &$form_state) {
  backup_migrate_profile_save_profile($form_state['values']);
  $form_state['redirect'] = "admin/content/backup_migrate/profile";
}

/**
 * Delete a profile.
 */
function backup_migrate_ui_profile_delete($profile_id = NULL) {
  if ($profile = backup_migrate_get_profile($profile_id)) {
    return drupal_get_form('backup_migrate_ui_profile_delete_confirm', $profile_id);
  }
  drupal_goto('admin/content/backup_migrate/profile');
}

/**
 * Ask confirmation for deletion of a profile.
 */
function backup_migrate_ui_profile_delete_confirm(&$form_state, $profile_id) {
  $form['profile_id'] = array('#type' => 'value', '#value' => $profile_id);
  $profile = backup_migrate_get_profile($profile_id);
  return confirm_form($form, t('Are you sure you want to delete the profile %profile?', array('%profile' => $profile['name'])), 'admin/content/backup_migrate/profile', t('This will cannot be undone.'), t('Delete'), t('Cancel'));
}

/**
 * Delete a destination after confirmation.
 */
function backup_migrate_ui_profile_delete_confirm_submit($form, &$form_state) {
  $profile_id = $form_state['values']['profile_id'];
  backup_migrate_profile_delete_profile($profile_id);
  $form_state['redirect'] = "admin/content/backup_migrate/profile";
}

/**
 * Export a profile to a renedered PHP variable.
 */
function backup_migrate_ui_profile_export($profile_id = NULL) {
  if (function_exists('ctools_include') && $profile = backup_migrate_get_profile($profile_id)) {
    ctools_include('export');
    unset($profile['db']);
    if (is_numeric($destination['profile_id'])) {
      $profile['profile_id'] = uniqid('default_');
    }
    return drupal_get_form('backup_migrate_ui_export_form', $profile);
  }
  drupal_goto('admin/content/backup_migrate/profile');
}

/* Utilities */

/**
 * Get the available profiles as an options array for a form item.
 */
function _backup_migrate_get_profile_form_item_options() {
  $out = array();
  foreach (backup_migrate_get_profiles() as $key => $profile) {
    $out[$key] = $profile['name'];
  }
  return $out;
}

/**
 * Get a form to configure the profile.
 */
function _backup_migrate_ui_backup_settings_form($profile) {
  drupal_add_js(array('backup_migrate' => array('checkboxLinkText' => t('View as checkboxes'))), 'setting');
  drupal_add_js(drupal_get_path('module', 'backup_migrate') .'/backup_migrate.js');
  drupal_add_css(drupal_get_path('module', 'backup_migrate') .'/backup_migrate.css');

  require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/files.inc';
  require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/db.inc';
  require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/destinations.inc';
  require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/filters.inc';

  $form    = array();
  $tables  = _backup_migrate_get_table_names();

  $sources = _backup_migrate_get_destination_form_item_options('source');
  if (count($sources) > 1) {
    $form['source'] = array(
      "#type" => "fieldset",
      "#title" => t("Backup Source"),
      "#collapsible" => TRUE,
      "#collapsed" => FALSE,
      "#tree" => FALSE,
      "#description" => t("Choose the database to backup. Any database destinations you have created and any databases specified in your settings.php can be backed up."),
    );
    $form['source']['source_id'] = array(
      "#type" => "select",
      "#title" => t("Source"),
      "#options" => _backup_migrate_get_destination_form_item_options('source'),
      "#default_value" => $profile['source_id'],
    );
  }
  else {
    $form['source_id'] = array(
      "#type" => "value",
      "#value" => $profile['source_id'],
    );
  }

  $form['file'] = array(
    "#type" => "fieldset",
    "#title" => t("Backup File"),
    "#collapsible" => TRUE,
    "#collapsed" => FALSE,
    "#tree" => FALSE,
  );
  $form['file']['filename'] = array(
    "#type" => "textfield",
    "#title" => t("Backup file name"),
    "#default_value" => $profile['filename'],
  );
  if (module_exists('token')) {
    $form['file']['token_help'] = array(
      '#title' => t('Replacement patterns'),
      '#type' => 'fieldset',
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
      '#description' => t('Prefer raw-text replacements for text to avoid problems with HTML entities!'),
    );
    $form['file']['token_help']['help'] = array(
      '#value' => theme('token_help', ''),
    );
    $form['file']['filename']['#description'] = t('You can use tokens to in the file name.', array('!tokenurl' => 'http://drupal.org/project/token'));
  }
  else {
    $form['file']['filename']['#description'] = t('If you install the <a href="!tokenurl">Token Module</a> you can use tokens to in the file name.', array('!tokenurl' => 'http://drupal.org/project/token'));
  }
  $form['file']['append_timestamp'] = array(
    "#type" => "checkbox",
    "#title" => t("Append a timestamp."),
    "#default_value" => $profile['append_timestamp'],
  );
  $form['file']['timestamp_format'] = array(
    "#type" => "textfield",
    "#title" => t("Timestamp format"),
    "#default_value" => $profile['timestamp_format'],
    "#description" => t('Should be a PHP <a href="!url">date()</a> format string.', array('!url' => 'http://www.php.net/date')),
  );
  $form['tables'] = array(
    "#type" => "fieldset",
    "#title" => t("Database Tables"),
    "#collapsible" => TRUE,
    "#collapsed" => TRUE,
    "#tree" => FALSE,
    "#description" => t("You may omit specific tables, or specific table data from the backup file. Only omit data that you know you will not need such as cache data, or tables from other applications. Excluding tables can break your Drupal install, so <strong>do not change these settings unless you know what you're doing</strong>."),
  );
  $form['tables']['exclude_tables'] = array(
    "#type" => "select",
    "#multiple" => TRUE,
    "#title" => t("Exclude the following tables altogether"),
    "#options" => $tables,
    "#default_value" => $profile['exclude_tables'],
    "#description" => t("The selected tables will not be added to the backup file."),
  );
  $form['tables']['nodata_tables'] = array(
    "#type" => "select",
    "#multiple" => TRUE,
    "#title" => t("Exclude the data from the following tables"),
    "#options" => $tables,
    "#default_value" => $profile['nodata_tables'],
    "#description" => t("The selected tables will have their structure backed up but not their contents. This is useful for excluding cache data to reduce file size."),
  );
  $form['advanced'] = array();
  $form = array_merge_recursive($form, backup_migrate_filters_settings_form($profile['filters'], 'backup'));

  // Add the advanced fieldset if there are any fields in it.
  if ($form['advanced']) {
    $form['advanced']['#type'] = 'fieldset';
    $form['advanced']['#title'] = t('Advanced Options');
    $form['advanced']['#collapsed'] = true;
    $form['advanced']['#collapsible'] = true;
  }

  return $form;
}

/**
 * Get the action links for a profile.
 */
function _backup_migrate_profile_get_links($profile_id) {
  $out = array();
  if ($profile = backup_migrate_get_profile($profile_id)) {
    if (!empty($profile['db'])) {
      $out[] = l(t("configure"), "admin/content/backup_migrate/profile/list/configure/". $profile_id);
      $out[] = l(t("delete"), "admin/content/backup_migrate/profile/list/delete/". $profile_id);
    }
    if (module_exists('ctools')) {
      $out[] = l(t("export"), "admin/content/backup_migrate/profile/list/export/". $profile_id);
    }
  }
  return $out;
}

/**
 * Get the default profile.
 */
function _backup_migrate_profile_default_profile() {
  require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/db.inc';
  require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/files.inc';
  require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/filters.inc';
  return array(
    'source_id' => 'db_url:default',
    'exclude_tables' => _backup_migrate_default_exclude_tables(),
    'nodata_tables' => _backup_migrate_default_structure_only_tables(),
    'filename' => _backup_migrate_default_filename(),
    'append_timestamp' => 1,
    'timestamp_format' => 'Y-m-d\TH-i-s',
    'compression' => "none",
    'filters' => backup_migrate_filters_settings_default('backup'),
  );
}

/**
 * Get the default profile saved by the user (or the module default if none exists).
 */
function _backup_migrate_profile_saved_default_profile($profile_id = NULL) {
  $profile_id = $profile_id ? $profile_id : variable_get("backup_migrate_profile_id", NULL);
  $profile = NULL;
  if (is_numeric($profile_id)) {
    $profile = backup_migrate_get_profile($profile_id);
  }
  if (!$profile) {
    $profile = _backup_migrate_profile_default_profile();
  }
  return $profile;
}

