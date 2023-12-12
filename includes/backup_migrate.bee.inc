<?php


/**
 * @file
 * Bee commands for backup and migrate.
 */

/**
 * Implementation of hook_bee_command().
 */
function backup_migrate_bee_command() {
  $items['bam-backup'] = array(
    'description' => bt("Backup a specified source associated with a Backdrop CMS website."),
    'callback' => 'backup_migrate_backup_bee_callback',
    'group' => 'backup_migrate',
    'arguments' => array(
      'source'        => bt("Optional. The machine name of the source to back up. Use 'bee bam-sources' to get a list of sources available. Defaults to 'db'"),
      'destination'   => bt("Optional. The machine name of the destination to save the backup file to. Use 'bee bam-destinations' to get a list of destinations available. Defaults to 'manual'"),
      'profile'       => bt("Optional. The machine name of the settings profile to use. Use 'bee bam-profiles' to get a list of available settings profiles available. Defaults to 'default'"),
    ),
    'optional_arguments' => array(
      'source',
      'destination',
      'profile',
    ),
    'aliases' => array('bb'),
    'bootstrap' => BEE_BOOTSTRAP_FULL,
    'examples' => array(
      'bee bam-backup' =>                         bt("Back up the default source 'db' using the default destination and settings profile."),
      'bee bam-backup files' =>                   bt("Back up the default source 'files' using the default destination and settings profile."),
      'bee bam-backup archive scheduled' =>       bt("Back up the default source 'archive' to the default destination 'scheduled' using the default settings profile."),
      'bee bam-backup db scheduled mysettings' => bt("Back up the default source 'db' to the default destination 'scheduled' using the settings profile 'mysettings'."),
    ),
  );

  $items['bam-destinations'] = array(
    'description' => bt("Get a list of available destinations."),
    'callback' => 'backup_migrate_list_destinations_bee_callback',
    'group' => 'backup_migrate',
    'bootstrap' => BEE_BOOTSTRAP_FULL,
  );

  $items['bam-profiles'] = array(
    'description' => bt("Get a list of available settings profiles."),
    'callback' => 'backup_migrate_list_settings_profiles_bee_callback',
    'group' => 'backup_migrate',
    'bootstrap' => BEE_BOOTSTRAP_FULL,
  );

  $items['bam-restore'] = array(
    'description' => bt("Restore a saved backup to a specified source."),
    'callback' => 'backup_migrate_restore_bee_callback',
    'group' => 'backup_migrate',
    'arguments' => array(
      'source'        => bt("Required. The machine name of the source to restore to."),
      'destination'   => bt("Required. The machine name of the destination where the backup is located."),
      'backup'        => bt("Required. The file name of the backup to use. Use 'bee bam-saved' to get a list of available backups."),
    ),
    'aliases' => array('br'),
    'bootstrap' => BEE_BOOTSTRAP_FULL,
    'examples' => array(
      'bee bam-restore archive manual SiteBackup.tar.gz'  => bt("Restores to the default source 'archive' using the backup 'SiteBackup.tar.gz' from the default destination 'manual'."),
      'bee bam-restore db scheduled DBBackup.tar.gz'      => bt("Restores to the default source 'db' using the backup 'DBBackup.tar.gz' from the default destination 'scheduled'."),
      'bee bam-restore files my_files FilesBackup.tar.gz' => bt("Restores to the defualt source 'files' using the backup 'FilesBackup.tar.gz' from the destination 'my_files'."),
    ),
  );

  $items['bam-saved'] = array(
    'description' => bt("Get a list of previously created backup files."),
    'callback' => 'backup_migrate_list_saved_bee_callback',
    'group' => 'backup_migrate',
    'arguments' => array(
      'destination' => bt("Optional. The machine name of the destination to list backups from. Use 'bee bam-destinations' to get a list of destinations."),
    ),
    'optional_arguments' => array(
      'destination',
    ),
    'bootstrap' => BEE_BOOTSTRAP_FULL,
  );

  $items['bam-sources'] = array(
    'description' => bt("Get a list of available sources."),
    'callback' => 'backup_migrate_list_sources_bee_callback',
    'group' => 'backup_migrate',
    'bootstrap' => BEE_BOOTSTRAP_FULL,
  );

  return $items;
}

/**
 * Callback for bam-backup.
 *
 * @param $arguments
 * @param $options
 *
 * @return void
 */
function backup_migrate_backup_bee_callback($arguments, $options) {
  backup_migrate_include('profiles', 'destinations', 'sources');

  $source_id = 'db';
  $destination_id = 'manual';
  $profile_id = 'default';

  if (!empty($arguments)) {
    $source_id = $arguments['source'] ?: $source_id;
    $destination_id = $arguments['destination'] ?: $destination_id;
    $profile_id = $arguments['profile'] ?: $profile_id;

    switch (strtolower($destination_id)) {
      case 'download':
      case 'upload':
        backup_migrate_bee_message("The use of destination '" . $destination_id . "' is not supported.", [],'warning');
        return;
    }
  }

  if (!backup_migrate_get_source($source_id)) {
    backup_migrate_bee_message("Could not find the source '@source'. Try using 'bee bam-sources' to get a list of available sources or use 'db' to backup the Backdrop database.", array('@source' => $source_id), 'error');
    return;
  }

  if (!backup_migrate_get_destination($destination_id)) {
    backup_migrate_bee_message("Could not find the destination '@destination'. Try using 'bee bam-destinations' to get a list of available destinations.", array('@destination' => $destination_id), 'error');
    return;
  }

  $settings = backup_migrate_get_profile($profile_id);

  if(!$settings) {
    backup_migrate_bee_message("Could not find the profile '@profile'. Try using 'bee bam-profiles' to get a list of available profiles.", array('@profile' => $profile_id), 'error');
    return;
  }

  $settings->destination_id = $destination_id;
  $settings->source_id = $source_id;

  backup_migrate_bee_message('Starting backup...', array(), 'info', TRUE);
  backup_migrate_perform_backup($settings);
  backup_migrate_bee_message('Backup complete.', array(),'success');
}

/**
 * Get a list of available destinations.
 */
function backup_migrate_list_destinations_bee_callback() {
  backup_migrate_include('destinations');

  $op = 'all';
  $rows = array();

  // Iterate through each backup destination.
  foreach (backup_migrate_get_destinations($op) as $destination) {
    // Remove any destinations that are not supported.
    switch ($destination->get_id()) {
      case 'download':
      case 'upload':
        continue 2;
    }

    // Build the data for our row.
    $row = array(
      array('value' => $destination->get_id()),
      array('value' => $destination->get_name()),
      array('value' => implode (', ', $destination->ops())),
    );

    $rows[] = $row;
  }

  // Build our header row.
  $header = array(
    array('value' => bt('ID')),
    array('value' => bt('Name')),
    array('value' => bt('Operations')),
  );

  $tableRender = array(
    'header' => $header,
    'rows' => $rows,
  );

  bee_render_table($tableRender, TRUE);
}

/**
 * Get a list of files in a given destination
 *
 * @param $arguments
 * @param $options
 *
 * @return void
 */
function backup_migrate_list_saved_bee_callback($arguments, $options) {
  backup_migrate_include('destinations');

  $destination = NULL;

  if (isset($arguments['destination']) && !$destination = backup_migrate_get_destination($arguments['destination'])) {
    backup_migrate_bee_message("Could not find the destination '@destination'. Try using 'bee bam-destinations' to get a list of available destinations.", array('@destination' => $arguments['destination']), 'error');
    return;
  }

  // Single destination required.
  if ($destination) {
    $destinations = array($destination);
  }
  // List all destinations
  else {
    $destinations = backup_migrate_get_destinations('list files');
  }

  // Load all the files.
  $rows = array();
  $sort = array();

  // Iterate through each backup destination.
  foreach ($destinations as $destination) {
    $destination->file_cache_clear();
    $dest_files = $destination->list_files();

    foreach ($dest_files as $id => $file) {
      $info = $file->info();
      $row = array(
        array('value' => check_plain($info['filename'])),
        array('value' => $destination->get_id()),
        array('value' => format_date($info['filetime'], 'small')),
        array('value' => format_interval(time() - $info['filetime'], 1)),
        array('value' => format_size($info['filesize'])),
      );

      $rows[] = $row;
      $sort[] = $info['filetime'];
    }
  }

  // Build our header row.
  $header = array(
    array('value' => bt('Filename')),
    array('value' => bt('Destination')),
    array('value' => bt('Date')),
    array('value' => bt('Age')),
    array('value' => bt('Size')),
  );

  if (count($rows)) {
    // Sort our multi-dimensional array by date then by destination.
    array_multisort(
      array_column($rows, 1), SORT_ASC,
      $sort, SORT_DESC,
      SORT_REGULAR, $rows);

    $tableRender = array(
      'header' => $header,
      'rows' => $rows,
    );

    bee_render_table($tableRender, TRUE);
  }
  else {
    bee_message(bt('There are no backup files to display.'), 'warning');
  }
}

/**
 * Get a list of available profiles.
 */
function backup_migrate_list_settings_profiles_bee_callback() {
  backup_migrate_include('profiles');

  $rows = array();

  // Iterate through each profile.
  foreach (backup_migrate_get_profiles() as $profile) {
    // Build the data for our row.
    $row = array(
      array('value' => $profile->get_id()),
      array('value' => $profile->get_name()),
    );

    $rows[] = $row;
  }

  // Build our header row.
  $header = array(
    array('value' => bt('ID')),
    array('value' => bt('Name')),
  );

  $tableRender = array(
    'header' => $header,
    'rows' => $rows,
  );

  bee_render_table($tableRender, TRUE);
}

/**
 * Get a list of available sources.
 */
function backup_migrate_list_sources_bee_callback() {
  backup_migrate_include('sources');

  $op = 'source';
  $rows = array();

  // Iterate through each backup source.
  foreach (backup_migrate_get_sources($op) as $source) {
    // Build the data for our row.
    $row = array(
      array('value' => $source->get_id()),
      array('value' => $source->get_name()),
      array('value' => implode (', ', $source->ops())),
    );

    $rows[] = $row;
  }

  // Build our header row.
  $header = array(
    array('value' => bt('ID')),
    array('value' => bt('Name')),
    array('value' => bt('Operations')),
  );

  $tableRender = array(
    'header' => $header,
    'rows' => $rows,
  );

  bee_render_table($tableRender, TRUE);
}

/**
 * Callback for bam-restore.
 *
 * @param $arguments
 * @param $options
 *
 * @return void
 */
function backup_migrate_restore_bee_callback($arguments, $options) {
  backup_migrate_include('profiles', 'destinations', 'sources');

  $source_id = $arguments['source'];
  $destination_id = $arguments['destination'];
  $file_id = $arguments['backup'];

  // Set the message mode to bee output.
  if (!backup_migrate_get_source($source_id)) {
    backup_migrate_bee_message("Could not find the source '@source'. Try using 'bee bam-sources' to get a list of available sources or use 'db' to backup the Backdrop database.", array('@source' => $source_id), 'error');
    return;
  }

  if (!$destination = backup_migrate_get_destination($destination_id)) {
    backup_migrate_bee_message("Could not find the destination '@destination'. Try using 'bee bam-destinations' to get a list of available destinations.", array('@destination' => $destination_id), 'error');
    return;
  }
  else if (!$file_id || !$file = backup_migrate_destination_get_file($destination_id, $file_id)) {
    backup_migrate_bee_message("Could not find the file '@file'. Try using 'bee bam-backups @destination' to get a list of available backup files in this destination destinations.", array('@destination' => $destination_id, '@file' => $file_id), 'error');
    return;
  }

  backup_migrate_bee_message("Restoring will delete some or all of your data and cannot be undone!", array(), 'warning');
  backup_migrate_bee_message("Restoring the DB will result in all users being logged out.", array(), 'warning');
  backup_migrate_bee_message("ALWAYS TEST YOUR BACKUPS ON A NON-PRODUCTION SERVER!", array(), 'warning', TRUE);

  if (!bee_confirm(bt('Are you sure you want to perform the restore?'))) {
    bee_message('Cancelled', 'warning');
    return;
  }

  $settings = array('source_id' => $source_id);

  backup_migrate_bee_message('Starting restore...', array(), 'info', TRUE);
  backup_migrate_perform_restore($destination_id, $file_id, $settings);
  backup_migrate_bee_message('Restore complete.', array(), 'success');
}

/**
 * Displaying a message to the user.
 *
 * @param $message
 * @param $replace
 * @param $type
 * @param $printMessage
 *
 * @return void
 */
function backup_migrate_bee_message($message, $replace = array(), $type = 'info', $printMessage = FALSE) {
  global $_bee_messages;

  if ($type == 'error') {
    bee_message(strip_tags(bt($message, $replace)), 'error');
  }
  else {
    // Use bee_message to display to the user.
    bee_message(strip_tags(bt($message, $replace)), str_replace('status', 'notice', $type));
  }

  // Check to see if we should print all messages stored in the message cache now.
  if ($printMessage) {
    bee_print_messages();
    $_bee_messages = array();
  }

  // Watchdog log the message as well for admins.
  _backup_migrate_message_log($message, $replace, $type);
}
