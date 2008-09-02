<?php
// $Id$


/**
 * @file
 * All of the schedule handling code needed for Backup and Migrate.
 */

/**
 * Run the preconfigured schedules. Called on cron.
 */
function backup_migrate_schedules_run() {
  require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/profiles.inc';
  foreach (backup_migrate_get_schedules() as $schedule) {
    $now = time();
    if ($schedule['enabled'] && $schedule['last_run'] < ($now - ($schedule['period']))) {
      if ($settings = backup_migrate_get_profile($schedule['profile_id'])) {
        $settings['destination_id'] = $schedule['destination_id'];
        backup_migrate_perform_backup($settings);
        _backup_migrate_schedule_set_last_run($schedule['schedule_id'], $now);
        _backup_migrate_schedule_remove_expired_backups($schedule['destination_id'], $schedule['keep']);
      }
      else {
        _backup_migrate_message("Schedule '%schedule' could not be run because requires a profile which is missing.", array('%schedule' => $schedule['name']), 'error');
      }
    }
  }
}

/**
 * Get all the available backup schedules.
 */
function backup_migrate_get_schedules() {
  static $schedules = NULL;

  // Get the list of schedules and cache them locally.
  if ($schedules === NULL) {
    $schedules = array();
    $all_schedules = module_invoke_all('backup_migrate_schedules');
    // Reindex since module_invoke_all stomps on numerical indices (thanks to array_merge).
    foreach ($all_schedules as $schedule) {
      $schedules[$schedule['schedule_id']] = $schedule;
    }
  }
  return $schedules;
}

/**
 * Get the schedule info for the schedule with the given ID, or NULL if none exists.
 */
function backup_migrate_get_schedule($schedule_id) {
  $schedules = backup_migrate_get_schedules();
  return @$schedules[$schedule_id];
}

/**
 * Implementation of hook_backup_migrate_schedules().
 *
 * Get the backup schedules stored in the db.
 */
function backup_migrate_backup_migrate_schedules() {
  // Get the saved scheduless
  $result = db_query('SELECT * FROM {backup_migrate_schedules}');
  while ($schedule = db_fetch_array($result)) {
    $schedule['db'] = TRUE;
    $out[$schedule['schedule_id']] = $schedule;
  }
  return $out;
}

/**
 * Update an existing schedule or create a new one.
 */
function backup_migrate_schedule_save_schedule(&$schedule) {
  // Calculate the period in seconds
  $periods            = _backup_migrate_frequency_periods();
  $period             = $periods[$schedule['period']['type']];
  $schedule['period'] = $schedule['period']['number'] * $period['seconds'];

  drupal_write_record('backup_migrate_schedules', $schedule, $schedule['schedule_id'] ? array('schedule_id') : array());
  _backup_migrate_message('Schedule saved: %schedule', array('%schedule' => $schedule['name']));
}

/**
 * Delete a saved schedule from the database.
 */
function backup_migrate_schedule_delete_schedule($schedule_id) {
  $schedule = backup_migrate_get_schedule($schedule_id);
  if ($schedule && $schedule['db']) {
    db_query("DELETE FROM {backup_migrate_schedules} WHERE schedule_id = %d", $schedule_id);
    _backup_migrate_message('Schedule deleted: %schedule', array('%schedule' => $schedule['name']));
  }
}

/* UI Menu Callbacks */

/**
 * List the the available schedules in the UI.
 */
function backup_migrate_ui_schedule_display_schedules() {
  require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/destinations.inc';
  require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/profiles.inc';
  $out = array();
  foreach (backup_migrate_get_schedules() as $schedule) {
    $destination = backup_migrate_get_destination($schedule['destination_id']);
    $profile     = backup_migrate_get_profile($schedule['profile_id']);
    $row         = array(
      check_plain($schedule['name']),
      $destination ? l($destination['name'], 'admin/content/backup_migrate/destination/files/'. $destination['destination_id']) : t("Missing"),
      $profile ? $profile['name'] : t("Missing"),
      _backup_migrate_schedule_format_frequency($schedule['period']),
      $schedule['keep'] ? $schedule['keep'] : t('All'),
      $schedule['enabled'] ? t('Enabled') : t('Disabled'),
      $schedule['last_run'] ? format_date($schedule['last_run'], 'small') : t('Never'),
      implode(" | ", _backup_migrate_schedule_get_links($schedule['schedule_id'])),
    );

    if (!$schedule['enabled']) {
      foreach ($row as $key => $field) {
        $row[$key] = array('data' => $field, 'class' => 'schedule-list-disabled');
      }
    }
    $out[] = $row;
  }

  $headers = array(
    t('Name'),
    t('Destination'),
    t('Profile'),
    t('Frequency'),
    t('Keep'),
    t('Enabled'),
    t('Last run'),
    t('Operations'),
  );
  drupal_add_css(drupal_get_path('module', 'backup_migrate') .'/backup_migrate.css');
  if ($out) {
    $out = theme("table", $headers, $out);
  }
  else {
    $out = t('There are no schedules to display.');
  }
  return $out .' '. l(t("Create new schedule..."), 'admin/content/backup_migrate/schedule/add');
}

/**
 * Get a form to create a new schedule.
 */
function backup_migrate_ui_schedule_create() {
  $schedule = array('name' => "Untitled Schedule", 'enabled' => 1, 'keep' => 0, 'period' => 60 * 60 * 24);
  $output = drupal_get_form('backup_migrate_ui_schedule_configure_form', $schedule);
  return $output;
}

/**
 * Get a form to configure the schedule.
 */
function backup_migrate_ui_schedule_configure($schedule_id) {
  if ($schedule = backup_migrate_get_schedule($schedule_id)) {
    return drupal_get_form('backup_migrate_ui_schedule_configure_form', $schedule);
  }
  return NULL;
}

/**
 * Get a form to configure the schedule.
 */
function backup_migrate_ui_schedule_configure_form(&$form_state, $schedule) {
  if ($schedule) {
    require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/destinations.inc';
    require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/profiles.inc';
    $form = array();
    $form['schedule_id'] = array(
      "#type" => "value",
      "#default_value" => $schedule['schedule_id'],
    );
    $form['enabled'] = array(
      "#type" => "checkbox",
      "#title" => t("Enabled"),
      "#field_suffix" => t("Hour(s)"),
      "#default_value" => $schedule['enabled'],
    );
    $form['name'] = array(
      "#type" => "textfield",
      "#title" => t("Schedule Name"),
      "#default_value" => $schedule['name'],
    );
    $form['profile_id'] = array(
      "#type" => "select",
      "#title" => t("Settings Profile"),
      "#options" => _backup_migrate_get_profile_form_item_options(),
      "#default_value" => $schedule['profile_id'],
    );
    $form['profile_id']['#description'] .= ' '. l(t("Create new profile..."), "admin/content/backup_migrate/profile/add");
    if (!$form['profile_id']['#options']) {
      $form['profile_id']['#options'] = array('0' => '-- None Available --');
    }

    $period_options = array();
    foreach (_backup_migrate_frequency_periods() as $type => $period) {
      $period_options[$type] = $period['title'];
    }
    $default_period     = _backup_migrate_schedule_get_frequency_period($schedule['period']);
    $default_period_num = $schedule['period'] / $default_period['seconds'];

    $form['period']     = array(
      "#type" => "item",
      "#title" => t("Backup every"),
      "#prefix" => '<div class="container-inline">',
      "#suffix" => '</div>',
      "#tree" => TRUE,
    );
    $form['period']['number'] = array(
      "#type" => "textfield",
      "#size" => 6,
      "#default_value" => $default_period_num,
    );
    $form['period']['type'] = array(
      "#type" => "select",
      "#options" => $period_options,
      "#default_value" => $default_period['type'],
    );

    $form['keep'] = array(
      "#type" => "textfield",
      "#size" => 6,
      "#title" => t("Number of Backup files to keep"),
      "#description" => t("The number of backup files to keep before deleting old ones. Use 0 to never delete backups"),
      "#default_value" => $schedule['keep'],
    );
    $destination_options = _backup_migrate_get_destination_form_item_options('scheduled backup');
    $form['destination_id'] = array(
      "#type" => "select",
      "#title" => t("Destination"),
      "#description" => t("Choose where the backup file will be saved. Backup files contain sensitive data, so be careful where you save them."),
      "#options" => $destination_options,
      "#default_value" => $schedule['destination_id'],
    );
    $form['destination_id']['#description'] .= ' '. l(t("Create new destination..."), "admin/content/backup_migrate/destination/add");

    $form['submit'] = array(
      '#type' => 'submit',
      '#weight' => 99,
      '#value' => t('Save Schedule'),
    );
    return $form;
  }
  return array();
}

/**
 * Validate the schedule configuration form.
 */
function backup_migrate_ui_schedule_configure_form_validate($form, &$form_state) {
  if (!is_numeric($form_state['values']['period']['number']) || $form_state['values']['period']['number'] <= 0) {
    form_set_error('[period][number]', t('Backup period must be a number greater than 0.'));
  }
  if (!is_numeric($form_state['values']['keep']) || $form_state['values']['keep'] < 0) {
    form_set_error('[keep]', t('Number to keep must be an integer greater than or equal to 0.'));
  }
}

/**
 * Submit the schedule configuration form.
 */
function backup_migrate_ui_schedule_configure_form_submit($form, &$form_state) {
  backup_migrate_schedule_save_schedule($form_state['values']);
  $form_state['redirect'] = "admin/content/backup_migrate/schedule";
}

/**
 * Delete a schedule.
 */
function backup_migrate_ui_schedule_delete($schedule_id) {
  return drupal_get_form('backup_migrate_ui_schedule_delete_confirm', $schedule_id);
}

/**
 * Ask confirmation for deletion of a schedule.
 */
function backup_migrate_ui_schedule_delete_confirm(&$form_state, $schedule_id) {
  $form['schedule_id'] = array('#type' => 'value', '#value' => $schedule_id);
  $schedule = backup_migrate_get_schedule($schedule_id);
  return confirm_form($form, t('Are you sure you want to delete the schedule %schedule?', array('%schedule' => $schedule['name'])), 'admin/content/backup_migrate/schedule', t('This will cannot be undone.'), t('Delete'), t('Cancel'));
}

/**
 * Delete a destination after confirmation.
 */
function backup_migrate_ui_schedule_delete_confirm_submit($form, &$form_state) {
  $schedule_id = $form_state['values']['schedule_id'];
  backup_migrate_schedule_delete_schedule($schedule_id);
  $form_state['redirect'] = "admin/content/backup_migrate/schedule";
}


/* Utilities */

/**
 * Get the action links for a schedule.
 */
function _backup_migrate_schedule_get_links($schedule_id) {
  $out = array();
  if ($schedule = backup_migrate_get_schedule($schedule_id)) {
    if ($schedule['db']) {
      $out[] = l(t("Configure..."), "admin/content/backup_migrate/schedule/configure/". $schedule_id);
      $out[] = l(t("Delete..."), "admin/content/backup_migrate/schedule/delete/". $schedule_id);
    }
  }
  return $out;
}

/**
 * Set the last run time of a schedule to the given timestamp, or now if none specified.
 */
function _backup_migrate_schedule_set_last_run($schedule_id, $timestamp = NULL) {
  if ($timestamp === NULL) {
    $timestamp = time();
  }
  if ($schedule_id) {
    db_query("UPDATE {backup_migrate_schedules}
                 SET  last_run = %d
               WHERE schedule_id = '%s'",
      $timestamp,
      $schedule_id
      );
  }
}

/**
 * Remove older backups keeping only the number specified by the aministrator.
 */
function _backup_migrate_schedule_remove_expired_backups($destination_id, $num_to_keep) {
  require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/destinations.inc';
  // If num to keep is not 0 (0 is infinity).
  if ($num_to_keep && $destination = backup_migrate_get_destination($destination_id)) {
    if ($destination_files = backup_migrate_destination_get_files($destination)) {
      // Sort the files by modified time.
      foreach ($destination_files as $file) {
        $files[str_pad($file['filemtime'], 10, "0", STR_PAD_LEFT) ."-". $i++] = $file['filepath'];
      }

      // If we are beyond our limit, remove as many as we need.
      $num_files = count($files);
      if ($num_files > $num_to_keep) {
        $num_to_delete = $num_files - $num_to_keep;
        // Sort by date.
        ksort($files);
        // Delete from the start of the list (earliest).
        for ($i = 0; $i < $num_to_delete; $i++) {
          $filepath = array_shift($files);
          file_delete($filepath);
        }
      }
    }
  }
}

/**
 * Format a frequency in human-readable form.
 */
function _backup_migrate_schedule_format_frequency($frequency) {
  $period = _backup_migrate_schedule_get_frequency_period($frequency);
  $out = format_plural(($frequency / $period['seconds']), $period['singular'], $period['plural']);
  return $out;
}

/**
 * Get the period of the frequency (ie: seconds, minutes etc.)
 */
function _backup_migrate_schedule_get_frequency_period($frequency) {
  $out = "";
  foreach (array_reverse(_backup_migrate_frequency_periods()) as $period) {
    if ($period['seconds'] && ($frequency % $period['seconds']) === 0) {
      return $period;
    }
  }
}

/**
 * Get a list of available backup periods. Only returns time periods which have a
 *  (reasonably) consistent number of seconds.
 */
function _backup_migrate_frequency_periods() {
  return array(
    'seconds' => array('type' => 'seconds', 'seconds' => 1, 'title' => 'Seconds', 'singular' => t('Once a second'), 'plural' => t('Every @count seconds')),
    'minutes' => array('type' => 'minutes', 'seconds' => 60, 'title' => 'Minutes', 'singular' => t('Once a minute'), 'plural' => t('Every @count minutes')),
    'hours' => array('type' => 'hours', 'seconds' => 3600, 'title' => 'Hours', 'singular' => t('Once an hour'), 'plural' => t('Every @count hours')),
    'days' => array('type' => 'days', 'seconds' => 86400, 'title' => 'Days', 'singular' => t('Once a day'), 'plural' => t('Every @count days')),
    'weeks' => array('type' => 'weeks', 'seconds' => 604800, 'title' => 'Weeks', 'singular' => t('Once a week'), 'plural' => t('Every @count weeks')),
  );
}

