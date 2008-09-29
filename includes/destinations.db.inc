<?php
// $Id$


/**
 * @file
 * Functions to handle the direct to database destination.
 */

/**
 * Databse save download destination callback.
 */
function backup_migrate_destination_db_save($destination, $file, $settings) {
  require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/files.inc';
  require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/db.inc';

  // Switch to the new db.
  _backup_migrate_db_switch_db(_backup_migrate_destination_glue_url($destination, FALSE));

  // Restore the file to the db.
  backup_migrate_perform_restore_file(backup_migrate_file_info($file));

  // Switch back to the old db.
  _backup_migrate_db_switch_db();

  return $file;
}

/**
 * Destination configuration callback.
 */
function backup_migrate_destination_db_conf($destination, $form) {
  $form['scheme'] = array(
    "#type" => "select",
    "#title" => t("Database Type"),
    "#default_value" => $destination['scheme'] ? $destination['scheme'] : 'mysql',
    "#required" => TRUE,
    '#options' => array($GLOBALS['db_type'] => $GLOBALS['db_type']),
    "#description" => t('The type of the database. Drupal only supports one database type at a time, so this must be the same as the current database type.'),
  );
  $form['host'] = array(
    "#type" => "textfield",
    "#title" => t("Database Host"),
    "#default_value" => $destination['host'] ? $destination['host'] : 'localhost',
    "#required" => TRUE,
    "#description" => t('The host of the database.'),
  );
  $form['path'] = array(
    "#type" => "textfield",
    "#title" => t("Database Name"),
    "#default_value" => $destination['path'],
    "#required" => TRUE,
    "#description" => t('The name of the database. The database must exist, it will not be created for you.'),
  );
  $form['user'] = array(
    "#type" => "textfield",
    "#title" => t("Database User"),
    "#default_value" => $destination['user'],
    "#required" => TRUE,
    "#description" => t('Enter the name of a user who has write access to the database.'),
  );
  $form['password'] = array(
    "#type" => "password",
    "#title" => t("Database Password"),
    "#default_value" => $destination['password'],
    "#description" => t('Enter the password for the user.'),
  );
  if ($destination['password']) {
    $form['old_password'] = array(
      "#type" => "value",
      "#value" => $destination['password'],
    );
    $form['password']["#description"] .= t(' You do not need to enter a password unless you wish to change the currently saved password.');
  }
  $form['#validate'] = array('backup_migrate_destination_db_conf_validate');
  $form['#submit'] = array('backup_migrate_destination_db_conf_submit', 'backup_migrate_ui_destination_configure_form_submit');
  return $form;
}

/**
 * Validate the configuration form. Make sure the db info is valid.
 */
function backup_migrate_destination_db_conf_validate($form, &$form_state) {
  if (!preg_match('/[a-zA-Z0-9_\$]+/', $form_state['values']['path'])) {
    form_set_error('path', t('The database name is not valid.'));
  }
}

/**
 * Validate the configuration form. Make sure the email address is valid.
 */
function backup_migrate_destination_db_conf_submit($form, &$form_state) {
  $form_state['values']['password'] = $form_state['values']['password'] ? $form_state['values']['password'] : $form_state['values']['old_password'];
  $form_state['values']['location'] = _backup_migrate_destination_glue_url($form_state['values']);
}
