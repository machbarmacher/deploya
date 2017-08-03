<?php
/**
 * @file deploya.drush.inc
 */

use GitElephant\Repository;

/**
 * Implements hook_drush_command().
 *
 * @todo Disable rsync.
 */
function drush_deploya_drush_command() {
  $items = array();

  $items['deploya-push-code'] = array(
    'callback' => 'drush_deploya_push_code',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_ROOT,
    'aliases' => ['depushc'],
    'description' => "",
    'arguments' => ['$destination' => 'The remote to push to.'],
    'pulldata' => 'If set, pull data on destination afterwards.'
  );

  $items['deploya-pull-data'] = array(
    'callback' => 'drush_deploya_pull',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_ROOT,
    'aliases' => ['depulld'],
    'description' => "",
    'arguments' => [],
    'options' => [
      'source' => 'Source, defaults to "live".',
      'destination' => 'Destination alias, if different from current installation.',
      'db' => 'Use --no-db to omit database.',
      'files' => 'Use --no-files to omit files.',
      'public-files' => 'Use --no-public-files to omit public file.',
      'private-files' => 'Use --no-private-files to omit private file.',
      'public-files-exclude-paths' => 'Option --exclude-pahts for public files rsync.',
      'private-files-exclude-paths' => 'Option --exclude-pahts for private files rsync.',
    ],
  );

  return $items;
}

function drush_deploya_push_code($destination) {
  // @todo Check status and abstract out.
  $repo = new Repository(DRUPAL_ROOT);

  // @see _drush_sitealias_all_list()
  drush_sitealias_load_all();
  $aliases = drush_get_context('site-aliases');
  if (!isset($aliases[$destination])) {
    drush_set_error("Unknown destination alias: $destination");
  }
  $destination_alias = $aliases[$destination];
  $uri = _drush_deploya_push_code_uri($destination_alias);

  try {
    $destination_remote = $repo->getRemote($destination);
  } catch (\InvalidArgumentException $e) {
    drush_log("Adding remote $destination");
    $repo->addRemote($destination, $uri);
    $destination_remote = $repo->getRemote($destination);
  }
  $current_uri = $destination_remote->getPushURL();
  if ($current_uri !== !$uri) {
    drush_log("Setting remote $destination from '$current_uri' to '$uri''.");
    $destination_remote->setPushURL($uri);
  }

  // Do it.
  $repo->push($destination);

  if (drush_get_option('pulldata')) {
    drush_invoke_process("@$destination", 'deploya-pull-data');
  }
}

function _drush_deploya_push_code_uri($destination_alias) {
  if (!empty($destination_alias['deploya-push-code-uri'])) {
    $uri = $destination_alias['deploya-push-code-uri'];
  }
  else {
    $uri = dirname($destination_alias['root']);
    if (!empty($destination_alias['remote-host'])) {
      $uri = "{$destination_alias['remote-host']}:$uri";
      if (!empty($destination_alias['remote-user'])) {
        $uri = "{$destination_alias['remote-user']}@$uri";
      }
    }
  }
  return $uri;
}

function drush_deploya_pull() {
  $source = drush_get_option('source', 'live');
  $destination = drush_get_option('destination', 'self');

  if ($source == $destination) {
    drush_log("No need to pull from $source to $destination.");
  }

  if (drush_get_option('db', TRUE)) {
    drush_invoke_process("@self", 'sql-sync', ["@$source", "@$destination"]);
  }
  _drush_deploya_rsync($source, $destination, '%files', 'public');
  _drush_deploya_rsync($source, $destination, '%private', 'private');
}

function _drush_deploya_rsync($source, $destination, $path, $prefix) {
  if (!drush_get_option('files', TRUE) || !drush_get_option("$prefix-files", TRUE)) {
    return;
  }
  $source_path = "@$source:$path";
  $destination_path = "@$destination:$path";

  // @see drush_core_rsync()
  $additional_options = [];
  $destination_settings = drush_sitealias_evaluate_path($destination_path, $additional_options, FALSE, "rsync", 'destination-');
  $source_settings = drush_sitealias_evaluate_path($source_path, $additional_options, FALSE, "rsync", 'source-');
  if (!isset($source_settings) || !isset($destination_settings)) {
    drush_log("No file path: $prefix");
    return;
  }

  drush_invoke_process("@self", 'rsync', [
    $source_path,
    $destination_path
  ], ['exclude_paths' => drush_get_option("$prefix-files-exclude-paths")]);
}
