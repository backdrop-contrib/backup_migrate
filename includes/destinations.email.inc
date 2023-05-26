<?php


/**
 * @file
 * Functions to handle the email backup destination.
 */

/**
 * A destination for emailing database backups.
 *
 * @ingroup backup_migrate_destinations
 */
class backup_migrate_destination_email extends backup_migrate_destination {
  var $supported_ops = array('scheduled backup', 'manual backup', 'remote backup', 'configure');

  /**
   * Save to (ie. email the file) to the email destination.
   */
  function save_file($file, $settings) {
    $size = filesize($file->filepath());
    $max = config_get('backup_migrate.settings','backup_migrate_max_email_size');
    if ($size > $max) {
      _backup_migrate_message('Could not email the file @file because it is @size and Backup and Migrate only supports emailing files smaller than @max.', array('@file' => $file->filename(), '@size' => format_size($size), '@max' => format_size($max)), 'error');
      return FALSE;
    }
    
    $attachment = new stdClass();
    
    $attachment->filename = $file->filename();
    $attachment->path = $file->filepath();
    
    _backup_migrate_destination_email_mail_backup($attachment, $this->get_location());
    return $file;
  }

  /**
   * Get the form for the settings for this filter.
   */
  function edit_form() {
    $form = parent::edit_form();
    $form['location'] = array(
      "#type" => "email",
      "#title" => t("Email Address"),
      "#default_value" => $this->get_location(),
      "#required" => TRUE,
      "#description" => t('Enter the email address to send the backup files to. Make sure the email sever can handle large file attachments'),
    );
    return $form;
  }

  /**
   * Validate the configuration form. Make sure the email address is valid.
   */
  function edit_form_validate($form, &$form_state) {
    if (!valid_email_address($form_state['values']['location'])) {
      form_set_error('[location]', t('The e-mail address %mail is not valid.', array('%mail' => $form_state['values']['location'])));
    }
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
  // Send mail.
  $attach        = fread(fopen($attachment->path, "r"), filesize($attachment->path));
  $mail          = new mime_mail();
  $from = config_get('system.core', 'site_mail');
  $mail->from    = !empty($from) ? $from : ini_get('sendmail_from');
  $mail->headers = 'Errors-To: [EMAIL=' . $mail->from . ']' . $mail->from . '[/EMAIL]';
  $mail->to      = $to;
  $mail->subject = t('Database backup from !site: !file', array('!site' => config_get('system.core', 'site_name'), '!file' => $attachment->filename));
  $mail->body    = t('Database backup attached.') . "\n\n";

  $mail->add_attachment($attach, $attachment->filename, "application/octet-stream");
  $mail->send();
}

class mime_mail {
  var $parts;
  var $to;
  var $from;
  var $headers;
  var $subject;
  var $body;

  function __construct() {
    $this->parts   = array();
    $this->to      = "";
    $this->from    = "";
    $this->headers = "";
    $this->subject = "";
    $this->body    = "";
  }

  function add_attachment($message, $name, $ctype) {
    $this->parts[] = array(
      "ctype" => $ctype,
      "message" => $message,
      "name" => $name,
    );
  }

  function build_message($part) {
    $crlf = "\r\n";
    // See RFC 2184.
    $continuation = $crlf . '  ';
    $name = $part['name'];
    $len = strlen($name);

    // RFC 5322 recommends lines of no longer than 78 chars, which in
    // this case comes down to filenames of no longer than 64 chars.
    if ($len > 64) {
      // We want to preserve the time stamp and extension and such.
      $head = substr($name, 0, 28);
      $tail = substr($name, ($len - 32));
      $name = $head . '___' . $tail;
    }
    $message = chunk_split(base64_encode($part["message"]), 70, $crlf);
    $disposition = $name ? "Content-Disposition: attachment; {$continuation}filename=\"$name\"$crlf" : "";
    return "Content-Type: " . $part["ctype"] . ($name ? ";{$continuation}name=\"$name\"" : "") .
    "{$crlf}Content-Transfer-Encoding: base64$crlf$disposition$crlf$message";
  }

  function build_multipart($boundary) {
    $multipart = "This is a MIME encoded message.\r\n\r\n--$boundary";
    for ($i = count($this->parts) - 1; $i >= 0; $i--) {
      $multipart .= "\r\n" . $this->build_message($this->parts[$i]) . "--$boundary";
    }
    return $multipart . "--\r\n";
  }

  function send() {
    $headers = array();
    if (!empty($this->body)) {
      $this->add_attachment($this->body, "", "text/plain");
    }
    $headers['MIME-Version'] = "1.0";
    $boundary = "b" . md5(uniqid(time()));
    $headers['Content-Type'] = "multipart/mixed; boundary=\"$boundary\"";
    $message = $this->build_multipart($boundary);
    $params = array();
    $params['body'] = $message;
    $params['headers'] = $headers;
    $params['subject'] = $this->subject;

    backdrop_mail('backup_migrate', 'destination_mail', trim($this->to), '', $params, $this->from);
  }
}
