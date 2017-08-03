<?php
/**
 * @file example.drushrc.php
 */

// Leave out unnecessary DB data.
$options['skip-tables']['common'] = array('migration_*');
$command_specific['sql-sync']['skip-tables-key'] = 'common';
$options['structure-tables']['common'] = array('cache', 'cache_*', 'history', 'search_*', 'sessions', 'watchdog');
$command_specific['sql-sync']['structure-tables-key'] = 'common';

// Leave out unnecessary files.
$command_specific['deploya-pull-data']['public-files-exclude-paths'] = 'advagg_css/:advagg_js/:css/:ctools/:feeds/:js/:languages/:styles/';

// Always pull data after push, if not to live.
# $command_specific['deploya-push-code']['pull-data'] = TRUE;

// If we have stage file proxy, no need to pull public files.
# $command_specific['deploya-pull-data']['public-files'] = FALSE;
