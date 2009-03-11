<?php
// $Id$


/**
 * @file
 * A filter to run some basic utility functions. Basically any useful option not big enough to justify it's own class.
 */

/**
 * A filter to send a notification email on success or failure of backup.
 *
 * @ingroup backup_migrate_filters
 */
class backup_migrate_filter_utils extends backup_migrate_filter {
  var $saved_devel_query = NULL;

  /**
   * Get the default backup settings for this filter.
   */
  function backup_settings_default() {
    return array(
      'utils_disable_query_log' => TRUE,
    );
  }

  /**
   * Get the default restore settings for this filter.
   */
  function restore_settings_default() {
    return array(
      'utils_disable_query_log' => TRUE,
    );
  }

  /**
   * Get the form for the backup settings for this filter.
   */
  function backup_settings_form($settings) {
    $form = array();
    if (module_exists('devel') && variable_get('dev_query', 0)) {
      $form['advanced']['utils_disable_query_log'] = array(
        '#type' => 'checkbox',
        '#title' => t("Disable query log"),
        '#default_value' => @$settings['utils_disable_query_log'],
        '#description' => t('Disable the devel module\'s query logging during the backup operation. It will be enabled again after backup is complete. This is very highly recommended.'),
      );
    }
    return $form;
  }

  /**
   * Get the form for the restore settings for this filter.
   */
  function restore_settings_form($settings) {
    $form = array();
    if (module_exists('devel') && variable_get('dev_query', 0)) {
      $form['advanced']['utils_disable_query_log'] = array(
        '#type' => 'checkbox',
        '#title' => t("Disable query log"),
        '#default_value' => @$settings['utils_disable_query_log'],
        '#description' => t('Disable the devel module\'s query logging during the restore operation. It will be enabled again after restore is complete. This is very highly recommended.'),
      );
    }
    return $form;
  }

  function pre_backup($source, $file, $settings) {
    $this->disable_devel_query($settings);
  }

  function post_backup($source, $file, $settings) {
    $this->enable_devel_query($settings);
  }

  function pre_restore($file, $settings) {
    $this->disable_devel_query($settings);
  }

  function post_restore($file, $settings) {
    $this->enable_devel_query($settings);
  }

  /**
   * Disable devel query logging if it's active and the user has chosen to do so.
   */
  function disable_devel_query($settings) {
    $this->saved_devel_query = variable_get('dev_query', 0);
    if (module_exists('devel') && variable_get('dev_query', 0) && @$settings['filters']['utils_disable_query_log']) {
      variable_set('dev_query', 0);
    }
  }

  /**
   * Restore devel query to previous state.
   */
  function enable_devel_query($settings) {
    if (module_exists('devel')) {
      variable_set('dev_query', $this->saved_devel_query);
    }
  }

}