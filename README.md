Backup Migrate for Backdrop CMS
===============================

This module makes the task of backing up your Backdrop database and migrating
data from one Backdrop install to another easier. It provides a function to
backup the entire database to file or download, and to restore from a previous
backup.
You can also schedule the backup operation. Compression of backup files is also
supported.

There are options to exclude the data from certain tables (such as cache or
search index tables) to increase efficiency by ignoring data that does not need
to be backed up or migrated.

The backup files are a list of SQL statements which can be executed with a tool
such as phpMyAdmin or the command-line mysql client.

**PHP 8 NOTE**:
Due to changes in Backdrop CMS 1.19 and newer to be PHP 8 compatible, the backup_migrate
module *must* be updated when you update your Backdrop installation.
See [Issue #64](https://github.com/backdrop-contrib/backup_migrate/issues/64) for more details.

Installation
------------

* Install this module using the official Backdrop CMS instructions at
  https://backdropcms.org/guide/modules
* Put the module in your Backdrop modules directory and enable it in
  admin/modules.
* Go to admin/people/permissions and grant permission to any roles that need to
  be able to backup or restore the database.
* Configure and use the module at admin/config/system/backup_migrate
* Visit the configuration page under Administration > Configuration > Category >
  Backup Migrate (admin/config/category/backup_migrate) and enter the required
  information.
* With the token.module now in Backdrop core, your site name from: admin/config/system/site-information will automatically be the backup file name. Other tokens like `[site:url]` may also be used.

OPTIONAL:
* To backup to Amazon S3:
    - Download the S3 library from https://github.com/tpyo/amazon-s3-php-class
      and place the file 'S3.php' in the includes directory in this module.
* Nodesquirrel has been removed as the service no longer exists.

LIGHTTPD USERS:
Add the following code to your lighttp.conf to secure your backup directories:

    $HTTP["url"] =~ "^/sites/default/files/backup_migrate/" {
         url.access-deny = ( "" )
    }
    
You may need to adjust the path to reflect the actual path to the files.

-------------------------------------------------------------------------------

VERY IMPORTANT SECURITY NOTE:
Backup files may contain sensitive data and by default, are saved to your web
server in a directory normally accessible by the public. This could lead to a
very serious security vulnerability. Backup and Migrate attempts to protect
backup files using a .htaccess file, but this is not guaranteed to work on all
environments (and is guaranteed to fail on web servers that are not apache). You
should test to see if your backup files are publicly accessible, and if in doubt
do not save backups to the server, or use the destinations feature to save to a
folder outside of your webroot.

OTHER WARNINGS:
A failed restore can destroy your database and therefore your entire Backdrop
installation. ALWAYS TEST BACKUP FILES ON A TEST ENVIRONMENT FIRST. If in doubt
do not use this module.

This module has only been tested with MySQL and does not work with any other dbms.

Make sure your php timeout is set high enough to complete a backup or restore
operation. Larger databases require more time. Also, while the module attempts
to keep memory needs to a minimum, a backup or restore will require
significantly more memory than most Backdrop operations.

If your backup file contains the 'sessions' table all other users will be logged
out after you run a restore. To avoid this, exclude the sessions table when
creating your backups. Be aware though that you will need to recreate the
sessions table if you use this backup on an empty database.

Do not change the file extension of backup files, or the restore function will be
unable to determine the compression type the file and will not function
correctly.

IF A RESTORE FAILS:
Don't panic, the restore file should work with phpMyAdmin's import function, or
with the mysql command line tool. If it does not, then it is likely corrupt; you
may panic now. MAKE SURE THAT THIS MODULE IS NOT YOUR ONLY FORM OF BACKUP.

-------------------------------------------------------------------------------

Bee integration
---------------

Backup & Migrate now provides [bee](https://backdropcms.org/project/bee) integration. `bee` is the command line interface for Backdrop. At the moment, it provides the following commands. 

* `bam-backup`: Backup a specified source associated with a Backdrop CMS website.
* `bam-destinations`: Get a list of available destinations.
* `bam-profiles`: Get a list of available settings profiles.
* `bam-restore`: Restore a saved backup to a specified source.
* `bam-saved`: Get a list of previously created backup files.
* `bam-sources`: Get a list of available sources.

Thanks to [rbargerhuff](https://github.com/rbargerhuff) for providing this integration!

Wiki
----

The [WIKI pages](https://github.com/backdrop-contrib/backup_migrate/wiki) of this module contain some tips and tricks that may help people troubleshoot configuration issues.

License
-------

This project is GPL v2 software. See the LICENSE.txt file in this directory for
complete text.

Current Maintainers
-------------------

- Jason Ramsey (https://github.com/quackers8me)
- [argiepiano](https://github.com/argiepiano)
- Luke McCormick (https://github.com/cellear)

Credits
-------
This module was originally written for Drupal by
- Ronan Dowling, Gorton Studios (https://www.drupal.org/u/ronan)
