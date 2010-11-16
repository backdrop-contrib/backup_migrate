<?php
// $Id$


/**
 * @file
 * Functions to handle the s3 backup destination.
 */

/**
 * A destination for sending database backups to an s3 server.
 *
 * @ingroup backup_migrate_destinations
 */
class backup_migrate_destination_s3 extends backup_migrate_destination_remote {
  var $supported_ops = array('scheduled backup', 'manual backup', 'restore', 'list files', 'configure', 'delete');
  var $s3 = NULL; 

  /**
   * Save to to the s3 destination.
   */
  function save_file($file, $settings) {
    $s3 = $this->s3_object();
    $path = $file->filename();
    if ($s3->putObject($s3->inputFile($file->filepath(), FALSE), $this->dest_url['path'], $this->remote_path($file->filename()), S3::ACL_PRIVATE)) {
      return $file;
    }
    return FALSE;
  }

  /**
   * Load from the s3 destination.
   */
  function load_file($file_id) {
    backup_migrate_include('files');
    $file = new backup_file(array('filename' => $file_id));
    $s3 = $this->s3_object();
    $data = $s3->getObject($this->dest_url['path'], $this->remote_path($file_id), $file->filepath());
    if (!$data->error) {
      return $file;
    }
    return NULL;
  }

  /**
   * Delete from the s3 destination.
   */
  function delete_file($file_id) {
    $s3 = $this->s3_object();
    $s3->deleteObject($this->dest_url['path'], $this->remote_path($file_id));
  }

  /**
   * List all files from the s3 destination.
   */
  function list_files() {
    backup_migrate_include('files');
    $files = array();
    $s3 = $this->s3_object();
    $s3_files = $s3->getBucket($this->dest_url['path'], $this->settings('subdir'));
    foreach ((array)$s3_files as $id => $file) {
      $info = array(
        'filename' => $this->local_path($file['name']),
        'filesize' => $file['size'],
        'filetime' => $file['time'],
      );
      $files[$info['filename']] = new backup_file($info);
    }
    return $files;
  }

  /**
   * Get the form for the settings for this filter.
   */
  function edit_form() {
    $form = parent::edit_form();
    $form['scheme']['#type'] = 'value';
    $form['scheme']['#value'] = 'https';
    $form['host']['#type'] = 'value';
    $form['host']['#value'] = 's3.amazonaws.com';
    $form['path']['#title'] = 'S3 Bucket';
    $form['path']['#description'] = 'This bucket must already exist. It will not be created for you.';
    $form['user']['#title'] = 'Access Key ID';
    $form['pass']['#title'] = 'Secret Access Key';

    $form['settings']['subdir'] = array(
      '#type' => 'textfield',
      '#title' => t('Subdirectory'),
      '#default_value' => $this->settings('subdir'),
    );
    $form['settings']['#weight'] = 25;
    $form['settings']['#tree'] = TRUE;

    return $form;
  }

  /**
   * Generate a filepath with the correct prefix.
   */
  function remote_path($path) {
    if ($subdir = $this->settings('subdir')) {
      $path = $subdir .'/'. $path;
    }
    return $path;
  }

  /**
   * Generate a filepath with the correct prefix.
   */
  function local_path($path) {
    if ($subdir = $this->settings('subdir')) {
      $path = str_replace($subdir .'/', '', $path);
    }
    return $path;
  }

  function s3_object() {
    require_once './'. drupal_get_path('module', 'backup_migrate') .'/includes/S3.php';
    if (!$this->s3) {
      $this->s3 = new S3($this->dest_url['user'], $this->dest_url['pass']);
    }
    return $this->s3;
  }
}


