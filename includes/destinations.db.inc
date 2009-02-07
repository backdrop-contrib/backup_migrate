<?php
// $Id$


/**
 * @file
 * Functions to handle the direct to database destination.
 */

/**
 * A destination type for saving to a database server.
 *
 * @ingroup backup_migrate_destinations
 */

class backup_migrate_destination_db extends backup_migrate_destination_remote {
  var $supported_ops = array('scheduled backup', 'manual backup', 'restore', 'configure', 'source');

  function type_name() {
    return t("Database");
  }

  /**
   * Save the info by importing it into the database.
   */
  function save_file($file, $settings) {
    require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/files.inc';
    require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/db.inc';

    // Switch to the new db.
    _backup_migrate_db_switch_db($this->dest_url(FALSE));

    // Restore the file to the db.
    backup_migrate_perform_restore_file($file);

    // Switch back to the old db.
    _backup_migrate_db_switch_db();

    return $file;
  }

  /**
   * Destination configuration callback.
   */
  function settings_form($form) {
    $form = parent::settings_form($form);
    $form['scheme']['#title'] = t('Database type');
    $form['scheme']['#options'] = array($GLOBALS['db_type'] => $GLOBALS['db_type']);
    $form['scheme']['#description'] = t('The type of the database. Drupal only supports one database type at a time, so this must be the same as the current database type.');
    $form['path']['#title'] = t('Database name');
    $form['path']['#description'] = t('The name of the database. The database must exist, it will not be created for you.');
    $form['user']['#description'] = t('Enter the name of a user who has write access to the database.');
    return $form;
  }

  /**
   * Validate the configuration form. Make sure the db info is valid.
   */
  function settings_form_validate($values) {
    if (!preg_match('/[a-zA-Z0-9_\$]+/', $values['path'])) {
      form_set_error('path', t('The database name is not valid.'));
    }
  }
}

/**
 * A destination for databases defined in settings.php.
 *
 * @ingroup backup_migrate_destinations
 */
class backup_migrate_destination_db_defaults extends backup_migrate_destination_db {
  var $supported_ops = array('scheduled backup', 'manual backup', 'source');
  var $db_key = "";

  function __construct($key) {
    global $db_url;
    $this->db_key = $key;
    $urls = is_array($db_url) ? $db_url : array('default' => $db_url);
    $this->set_url($urls[$key]);
  }

  function op($op) {
    // Don't allow backups to the default db as it's dangerous, confusing and of limited use.
    if ($this->db_key == 'default' && ($op == 'manual backup' || $op == 'scheduled backup')) {
      return FALSE;
    }
    return parent::op($op);
  }

  function id() {
    return "db_url:$this->db_key";
  }

  function name() {
    return $this->db_key == 'default' ? t("Default Database") : ($this->db_key .": ". $this->display_location());
  }
}

