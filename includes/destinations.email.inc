<?php
// $Id$


/**
 * @file
 * Functions to handle the email backup destination.
 */

/**
 * Email save destination callback.
 */
function backup_migrate_destination_email_save($destination, $file, &$settings) {
  if ($to = @$destination['location']) {
    $attachment = new stdClass();
    $attachment->filename = $settings['filename'];
    $attachment->path = $file;
    _backup_migrate_destination_email_mail_backup($attachment, $to);
    return $file;
  }
}

/**
 * Destination configuration callback.
 */
function backup_migrate_destination_email_conf($destination, $form) {
  $form['location'] = array(
    "#type" => "textfield",
    "#title" => t("Email Address"),
    "#default_value" => $destination['location'],
    "#required" => TRUE,
    "#description" => t('Enter the email address to send the backup files to. Make sure the email sever can handle large file attachments'),
  );
  $form['#validate'] = array('backup_migrate_destination_email_conf_validate');
  return $form;
}

/**
 * Validate the configuration form. Make sure the email address is valid.
 */
function backup_migrate_destination_email_conf_validate($form, &$form_state) {
  if (!valid_email_address($form_state['values']['location'])) {
    form_set_error('[location]', t('The e-mail address %mail is not valid.', array('%mail' => $form_state['values']['location'])));
  }
}

/**
 * @function
 * Temporary mail handler class.
 *
 * Defines a mail class to send a message with an attachment. Eventually Drupal
 * core should provide this functionality, at which time this code will be
 * removed.
 *
 * More info on sending email at <http://php.net/function.mail>.
 * This function taken from dba.module.
 *
 * @param $attachment
 *   An object which contains two variables "path" the path to the file and
 *   filename and "filename" which is just the filename.
 */
function _backup_migrate_destination_email_mail_backup($attachment, $to) {
  class mime_mail {
    var $parts;
    var $to;
    var $from;
    var $headers;
    var $subject;
    var $body;

    function mime_mail() {
      $this->parts   = array();
      $this->to      = "";
      $this->from    = "";
      $this->headers = "";
      $this->subject = "";
      $this->body    = "";
    }

    function add_attachment($message, $name = "", $ctype = "application/octet-stream", $encode = NULL) {
      $this->parts[] = array(
        "ctype" => $ctype,
        "message" => $message,
        "encode" => $encode,
        "name" => $name,
      );
    }

    function build_message($part) {
      $message  = $part["message"];
      $message  = chunk_split(base64_encode($message));
      $encoding = "base64";
      return "Content-Type: ". $part["ctype"] . ($part["name"] ? "; name = \"". $part["name"] ."\"" : "") ."\nContent-Transfer-Encoding: $encoding\n\n$message\n";
    }

    function build_multipart() {
      $boundary = "b". md5(uniqid(time()));
      $multipart = "Content-Type: multipart/mixed; boundary = $boundary\n\nThis is a MIME encoded message.\n\n--$boundary";
      for ($i = sizeof($this->parts) - 1; $i >= 0; $i--) {
        $multipart .= "\n". $this->build_message($this->parts[$i]) ."--$boundary";
      }
      return $multipart .= "--\n";
    }

    function send() {
      $mime = "";
      if (!empty($this->from)) $mime .= "From: ". $this->from ."\n";
      if (!empty($this->headers)) $mime .= $this->headers ."\n";
      if (!empty($this->body)) $this->add_attachment($this->body, "", "text/plain");
      $mime .= "MIME-Version: 1.0\n". $this->build_multipart();
      mail($this->to, $this->subject, "", $mime);
    }
  }

  // Send mail
  $attach        = fread(fopen($attachment->path, "r"), filesize($attachment->path));
  $mail          = new mime_mail();
  $mail->from    = variable_get('site_mail', ini_get('sendmail_from'));
  $mail->headers = 'Errors-To: [EMAIL='. $mail->from .']'. $mail->from .'[/EMAIL]';
  $mail->to      = $to;
  $mail->subject = t('Database backup from !site: !file', array('!site' => variable_get('site_name', 'drupal'), '!file' => $attachment->filename));
  $mail->body    = t('Database backup attached.');

  $mail->add_attachment("$attach", $attachment->filename, "Content-Transfer-Encoding: base64 /9j/4AAQSkZJRgABAgEASABIAAD/7QT+UGhvdG9zaG");
  $mail->send();
}

