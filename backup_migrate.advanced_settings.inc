<?php

/**
 * @file
 * Functionality required for the advanced settings form.
 */

/**
 * The advanced settings form.
 */
function backup_migrate_ui_advanced_settings_form($form, &$form_state) {
  $config = config('backup_migrate.settings');
  $form['#config'] = 'backup_migrate.settings';

  $form['backup_migrate_advanced_settings_intro'] = array(
    '#markup' => t('These settings should be handled with care, it is recommended to leave them at their defaults unless there is a specific need to modify them.'),
    '#prefix' => '<p>',
    '#suffix' => '</p>',
  );
  $form['backup_migrate_memory_limit'] = array(
    '#type' => 'textfield',
    '#title' => t('Memory Limit'),
    '#default_value' => $config->get('backup_migrate_memory_limit'),
    '#description' => t('If backups fail due to an out-of-memory, try adjusting the memory limit using a php.ini style value, for example 512M for 512 megabytes or 1G for 1 gigabyte. Default: <kbd>256M</kbd>.'),
    '#required' => FALSE,
    '#element_validate' => array('backup_migrate_memory_limit_validate'),
  );
  $form['backup_migrate_backup_max_time'] = array(
    '#type' => 'number',
    '#min' => 0,
    '#title' => t('Time Limit'),
    '#default_value' => $config->get('backup_migrate_backup_max_time'),
    '#description' => t('If backups fail due to a PHP timeout error, especially an error saying "MySQL server has gone away", increase this setting. Before doing this, check to see what PHP\'s "max_execution_time" is set to, then set this field to a higher number, e.g. if max_execution_time is 180 (seconds) try setting Time Limit to 240 seconds (4 minutes), or 300 seconds (5 minutes). Default: <kbd>1200</kbd>.'),
    '#required' => FALSE,
  );
  $form['backup_migrate_verbose'] = array(
    '#type' => 'checkbox',
    '#title' => t('Verbose Output'),
    '#default_value' => $config->get('backup_migrate_verbose'),
    '#description' => t('Enable this to make the module log additional messages to watchdog as the module performs certain actions. Default: off.'),
    '#required' => FALSE,
  );
  $form['backup_migrate_disable_cron'] = array(
    '#type' => 'checkbox',
    '#title' => t('Disable Cron'),
    '#default_value' => $config->get('backup_migrate_disable_cron'),
    '#description' => t('It can be frustrating working from a production database backup on non-prod servers as scheduled backups will automatically run via cron the same as they run on production. The custom cron tasks may be disabled using this checkbox. Note: this doesn\'t prevent people from manually running backups via the UI or from the Drush commands, so it is safe to enable on all site instances and then disable on production environments. Default: off.'),
    '#required' => FALSE,
  );
  $form['backup_migrate_data_rows_per_query'] = array(
    '#type' => 'number',
    '#min' => 0,
    '#title' => t('Rows Per Query'),
    '#default_value' => $config->get('backup_migrate_data_rows_per_query'),
    '#description' => t('Controls how many records are loaded from the database at once. Defaults to "<kbd>50000</kbd>", i.e. 50000 rows. Note that setting this to a high number can cause problems when exporting large data sets, e.g. cache tables can have huge volumes of data per record.'),
    '#required' => FALSE,
  );
  $form['backup_migrate_data_rows_per_line'] = array(
    '#type' => 'number',
    '#min' => 0,
    '#title' => t('Rows Per Line'),
    '#default_value' => $config->get('backup_migrate_data_rows_per_line'),
    '#description' => t('Controls how many records are included in a single INSERT statement. Defaults to "!default", i.e. <kbd>30</kbd> records.'),
    '#required' => FALSE,
  );
  $form['backup_migrate_data_bytes_per_line'] = array(
    '#type' => 'number',
    '#min' => 0,
    '#title' => t('Data Bytes Per Line'),
    '#default_value' => $config->get('backup_migrate_data_bytes_per_line'),
    '#description' => t('Controls how much data will be inserted at once using a single INSERT statement. This works with the "backup_migrate_data_rows_per_line" variable to ensure that each INSERT statement doesn\'t end up being too large. Defaults to "<kbd>2000</kbd>", i.e. 2000 bytes.'),
    '#required' => FALSE,
  );
  $form['backup_migrate_max_email_size'] = array(
    '#type' => 'textfield',
    '#title' => t('Maximum E-mail Size'),
    '#default_value' => $config->get('backup_migrate_max_email_size'),
    '#description' => t('In bytes. Limits the size of e-mails with attached back-ups that can be sent. Default: <kbd>20971520</kdb>.'),
    '#required' => FALSE,
    '#element_validate' => array('backup_migrate_unsigned_integer_validate'),
  );
  $form['backup_migrate_cleanup_temp_files'] = array(
    '#type' => 'checkbox',
    '#title' => t('Clean up Temporary Files'),
    '#default_value' => $config->get('backup_migrate_cleanup_temp_files'),
    '#description' => t('Should temporary files be deleted during the current back-up run? Default: on.'),
    '#required' => FALSE,
  );
  $form['backup_migrate_cleanup_time'] = array(
    '#type' => 'textfield',
    '#title' => t('Clean-up Time'),
    '#default_value' => $config->get('backup_migrate_cleanup_time'),
    '#description' => t('Delete temporary files belonging to this modules that are older than the expire time. In seconds. 21600 seconds is 6 hours. Default: <kbd>21600</kbd>.'),
    '#required' => FALSE,
    '#element_validate' => array('element_validate_integer'),
  );
  $form['backup_migrate_schedule_buffer'] = array(
    '#type' => 'textfield',
    '#title' => t('Schedule buffer'),
    '#default_value' => $config->get('backup_migrate_schedule_buffer'),
    '#description' => t('Add a small negative buffer (for example 1% of the entire period) to the time to account for slight difference in cron run length. Format: percentage of the period as a decimal fraction between 0 and 1. Default: <kbd>0.01</kbd>.'),
    '#required' => FALSE,
    '#element_validate' => array('backup_migrate_fraction_validate'),
  );

  return system_settings_form($form);
}

/**
 * Validate fields that require a PHP.ini-like memory_limit value.
 */
function backup_migrate_memory_limit_validate($element, &$form_state, $form) {
  if (!_backup_migrate_is_valid_memory_limit($element['#value'])) {
    form_error($element, t('%name must be a valid memory limit setting.', array('%name' => $element['#title'])));
  }
}

/**
 * Validate fields that require zero or a positive integer.
 */
function backup_migrate_unsigned_integer_validate($element, &$form_state, $form) {
  $value = $element['#value'];
  $test = preg_match('/^[0-9]+$/', $value);
  if ($test !== 1) {
    form_error($element, t('%name must be zero or a positive integer.', array('%name' => $element['#title'])));
  }
}

/**
 * Validate fields that require a decimal between 0 and 1.
 *
 * @todo Better name.
 */
function backup_migrate_fraction_validate($element, &$form_state, $form) {
  $value = $element['#value'];
  $test = preg_match('/^(0|1|0?\.[0-9]+)$/', $value);
  if ($test !== 1) {
    form_error($element, t('%name must be a decimal between 0 and 1.', array('%name' => $element['#title'])));
  }
}
