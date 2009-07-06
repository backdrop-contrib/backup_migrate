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
    backup_migrate_include('files', 'db');

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
  function edit_form() {
    $form = parent::edit_form();
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
  function edit_form_validate($form, &$form_state) {
    if (!preg_match('/[a-zA-Z0-9_\$]+/', $form_state['values']['path'])) {
      form_set_error('path', t('The database name is not valid.'));
    }
    parent::edit_form_validate($form, $form_state);
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

  function __construct($params) {
    global $db_url;
    $this->db_key = $params['db_key'];
    $urls = is_array($db_url) ? $db_url : array('default' => $db_url);
    $this->set_url($urls[$this->db_key]);
  }

  function op($op) {
    // Don't allow backups to the default db as it's dangerous, confusing and of limited use.
    if ($this->db_key == 'default' && ($op == 'manual backup' || $op == 'scheduled backup')) {
      return FALSE;
    }
    return parent::op($op);
  }

  function get_id() {
    return "db_url:$this->db_key";
  }

  function get_name() {
    return $this->db_key == 'default' ? t("Default Database") : ($this->db_key .": ". $this->get_display_location());
  }

  /**
   * Get a row of data to be used in a list of items of this type.
   */  
  function get_list_row() {
    // Return none as this type should not be displayed.
    return array();
  }
}

