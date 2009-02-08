<?php
// $Id$


/**
 * @file
 * All of the filter handling code needed for Backup and Migrate.
 */

/**
 * Get the available destination types.
 */
function backup_migrate_get_filters($op = NULL) {
  static $filters = NULL;
  if ($filters === NULL) {
    $filters = array();
    $definitions = module_invoke_all('backup_migrate_filters');
    foreach ($definitions as $definition) {
      // Include the necessary file if specified by the filter.
      if (!empty($definition['file'])) {
        require_once './'. $definition['file'];
      }
      $filters[] = new $definition['class'];
    }
  }
  // Sort the filters based on the weight for the given operation.
  foreach ($filters as $filter) {
    $sort[] = $filter->weight($op);
  }
  array_multisort($filters, $sort);
  return $filters;
}

/**
 * Implementation of hook_backup_migrate_filters().
 *
 * Get the built in Backup and Migrate filters.
 */
function backup_migrate_backup_migrate_filters() {
  return array(
    'compression' => array(
      'file' => drupal_get_path('module', 'backup_migrate') .'/includes/filters.compression.inc',
      'class' => 'backup_migrate_filter_compression',
    ),
    'statusnotify' => array(
      'file' => drupal_get_path('module', 'backup_migrate') .'/includes/filters.statusnotify.inc',
      'class' => 'backup_migrate_filter_statusnotify',
    ),
  );
}

/**
 * Invoke the given method on all of the available filters.
 */
function backup_migrate_filters_invoke_all() {
  $args    = func_get_args();
  $op      = array_shift($args);
  $out     = array();
  $filters = backup_migrate_get_filters();
  foreach ($filters as $filter) {
    if (method_exists($filter, $op)) {
      $out += (array)call_user_func_array(array($filter, $op), $args);
    }
  }
  return $out;
}

/**
 * Filter a backup file before sending it to the destination.
 */
function backup_migrate_filters_backup($file, $settings) {
  $filters = backup_migrate_get_filters('backup_file');
  foreach ($filters as $filter) {
    $file = $filter->backup_file($file, $settings);
  }
  return $file;
}

/**
 * Filter a backup file before sending it to the destination.
 */
function backup_migrate_filters_restore($file) {
  $filters = backup_migrate_get_filters();
  foreach ($filters as $filter) {
    $file = $filter->restore_file($file, $settings);
  }
  return $file;
}

/**
 * Get the backup settings for all of the filters.
 */
function backup_migrate_filters_settings_form($settings, $op) {
  $out = backup_migrate_filters_invoke_all($op .'_settings_form', $settings);
  $out = backup_migrate_filters_settings_form_set_parents($out);
  return $out;
}

/**
 * Add a form parent to the filter settings so that the filter values are saved in the right table.
 */
function backup_migrate_filters_settings_form_set_parents($form) {
  foreach ($form as $key => $item) {
    if (isset($item['#type']) && !isset($item['#parents'])) {
      $form[$key]['#parents'] = array('filters', $key);
    }
    else {
      $form[$key] = backup_migrate_filters_settings_form_set_parents($item);
    }
  }
  return $form;
}

/**
 * Get the default settings for the filters.
 */
function backup_migrate_filters_settings_default($op) {
  return backup_migrate_filters_invoke_all($op .'_settings_default');
}

/**
 * Get the file types for all of the filters.
 */
function backup_migrate_filters_file_types() {
  return backup_migrate_filters_invoke_all('file_types');
}

/**
 * A base class for basing filters on.
 */
class backup_migrate_filter {
  var $weight = 0;
  var $op_weights = array();

  /**
   * Get the weight of the filter for the given op.
   */
  function weight($op = NULL) {
    if ($op && isset($this->op_weights[$op])) {
      return $this->op_weights[$op];
    }
    return $this->weight;
  }

  /**
   * Get the form for the settings for this filter.
   */
  function backup_settings_default() {
    return array();
  }

  /**
   * Get the form for the settings for this filter.
   */
  function backup_settings_form($settings) {
    return array();
  }

  /**
   * Get the form for the settings for this filter.
   */
  function backup_settings_form_validate($form_values) {
  }

  /**
   * Submit the settings form. Any values returned will be saved.
   */
  function backup_settings_form_submit($form_values) {
    return $form_values;
  }

  /**
   * Get the form for the settings for this filter.
   */
  function restore_settings_default() {
    return array();
  }

  /**
   * Get the form for the settings for this filter.
   */
  function restore_settings_form($settings) {
    return array();
  }

  /**
   * Get the form for the settings for this filter.
   */
  function restore_settings_form_validate($form_values) {
  }

  /**
   * Submit the settings form. Any values returned will be saved.
   */
  function restore_settings_form_submit($form_values) {
    return $form_values;
  }

  /**
   * Get a list of file types handled by this filter.
   */
  function file_types() {
    return array();
  }

  /**
   * This function is called on a backup file after the backup has been completed.
   */
  function backup_file($file, $settings) {
    return $file;
  }

  /**
   * This function is called on a backup file before importing it.
   */
  function restore_file($file, $settings) {
    return $file;
  }
}

