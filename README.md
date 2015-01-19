Backup Migrate for Backdrop cms 1.x
===================================

DESCRIPTION:
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

Installation
------------

- Install this module using the official Backdrop CMS instructions at
  https://backdropcms.org/guide/modules

- Visit the configuration page under Administration > Configuration > Category >
  Backup Migrate (admin/config/category/backup_migrate) and enter the required 
  information.


License
-------

This project is GPL v2 software. See the LICENSE.txt file in this directory for
complete text.

Current Maintainers
-------------------

- Jason Ramsey (https://github.com/quackers8me)

Past Maintainers
----------------

- Ronan Dowling, Gorton Studios (https://www.drupal.org/u/ronan)

