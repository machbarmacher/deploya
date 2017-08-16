<?php
/**
 * @file example.drushrc.php
 */

// Leave out unnecessary DB data in sql-sync and sql-dump.
$options['skip-tables-list'] = 'migration_*';
$options['structure-tables-list'] = 'cache,cache_*,history,search_*,sessions,watchdog';

// Leave out unnecessary files.
$command_specific['deploya-pull-data']['public-files-exclude-paths'] = 'advagg_css/:advagg_js/:css/:ctools/:feeds/:js/:languages/:styles/';

// Always pull data after push, if not to live.
# $command_specific['deploya-push-code']['pull-data'] = TRUE;

// If we have stage file proxy, no need to pull public files.
# $command_specific['deploya-pull-data']['public-files'] = FALSE;
