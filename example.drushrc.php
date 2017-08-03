<?php
/**
 * @file example.drushrc.php
 */

$options['skip-tables']['common'] = array('migration_*');
$command_specific['sql-sync']['skip-tables-key'] = 'common';
$options['structure-tables']['common'] = array('cache', 'cache_*', 'history', 'search_*', 'sessions', 'watchdog');
$command_specific['sql-sync']['structure-tables-key'] = 'common';

$command_specific['deploya-pull-data']['public-files-exclude-paths'] = 'advagg_css/:advagg_js/:css/:ctools/:feeds/:js/:languages/:styles/';
