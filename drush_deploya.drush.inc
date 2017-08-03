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
    'arguments' => ['$target' => 'The remote to push to.'],
    'pulldata' => 'If set, pull data on target afterwards.'
  );

  $items['deploya-pull-data'] = array(
    'callback' => 'drush_deploya_pull',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_ROOT,
    'aliases' => ['depulld'],
    'description' => "",
    'arguments' => [],
    'options' => [
      'source' => 'Source, defaults to "live".',
      'target' => 'Target alias, if different from current installation.',
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

function drush_deploya_push_code($target) {
  // @todo Check status and abstract out.
  $repo = new Repository(DRUPAL_ROOT);

  // @see _drush_sitealias_all_list()
  drush_sitealias_load_all();
  $aliases = drush_get_context('site-aliases');
  if (!isset($aliases[$target])) {
    drush_set_error("Unknown target alias: $target");
  }
  $target_alias = $aliases[$target];
  $uri = _drush_deploya_push_code_uri($target_alias);

  try {
    $target_remote = $repo->getRemote($target);
  } catch (\InvalidArgumentException $e) {
    drush_log("Adding remote $target");
    $repo->addRemote($target, $uri);
    $target_remote = $repo->getRemote($target);
  }
  $current_uri = $target_remote->getPushURL();
  if ($current_uri !== !$uri) {
    drush_log("Setting remote $target from '$current_uri' to '$uri''.");
    $target_remote->setPushURL($uri);
  }

  // Do it.
  $repo->push($target);

  if (drush_get_option('pulldata')) {
    drush_invoke_process("@$target", 'deploya-pull-data');
  }
}

function _drush_deploya_push_code_uri($target_alias) {
  if (!empty($target_alias['deploya-push-code-uri'])) {
    $uri = $target_alias['deploya-push-code-uri'];
  }
  else {
    $uri = dirname($target_alias['root']);
    if (!empty($target_alias['remote-host'])) {
      $uri = "{$target_alias['remote-host']}:$uri";
      if (!empty($target_alias['remote-user'])) {
        $uri = "{$target_alias['remote-user']}@$uri";
      }
    }
  }
  return $uri;
}

function drush_deploya_pull() {
  $source = drush_get_option('source', 'live');
  $target = drush_get_option('target', 'self');

  if ($source == $target) {
    drush_log("No need to pull from $source to $target.");
  }

  if (drush_get_option('db', TRUE)) {
    drush_invoke_process("@self", 'sql-sync', ["@$source", "@$target"]);
  }
  if (drush_get_option('files', TRUE) && drush_get_option('public-files', TRUE)) {
    _drush_deploya_rsync($source, $target, '%files');
  }

  $has_private_files = TRUE;
  if ($has_private_files && drush_get_option('files', TRUE) && drush_get_option('private-files', TRUE)) {
    drush_invoke_process("@self", 'rsync', ["@$source:%private", "@$target:%private"], ['exclude_paths'=> drush_get_option('private-files-exclude-paths')]);
  }
}

function _drush_deploya_rsync($source, $target, $path) {
  $source_path = "@$source:$path";
  $target_path = "@$target:$path";

  // @see drush_core_rsync()
  $additional_options = [];
  $target_settings = drush_sitealias_evaluate_path($target_path, $additional_options, FALSE, "rsync", 'target-');
  $source_settings = drush_sitealias_evaluate_path($source_path, $additional_options, FALSE, "rsync", 'source-');
  if (!isset($source_settings)) {
    return drush_set_error('DRUSH_BAD_PATH', dt('Could not evaluate source path !path.', array('!path' => $source)));
  }
  if (!isset($target_settings)) {
    return drush_set_error('DRUSH_BAD_PATH', dt('Could not evaluate destination path !path.', array('!path' => $destination)));
  }


  drush_invoke_process("@self", 'rsync', [
    $source_path,
    $target_path
  ], ['exclude_paths' => drush_get_option('public-files-exclude-paths')]);
}

function _drush_deploya_ensure_alias_path($path) {
  // @see drush_core_rsync()
  $additional_options = [];
  $values = drush_sitealias_evaluate_path($path, $additional_options);

}
