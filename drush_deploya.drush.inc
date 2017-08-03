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

  $items['deploya-remotes'] = array(
    'callback' => 'drush_deploya_remotes',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_ROOT,
    'aliases' => ['derem'],
    'description' => "",
  );

  $items['deploya-push-code'] = array(
    'callback' => 'drush_deploya_push_code',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_ROOT,
    'aliases' => ['depushc'],
    'description' => "",
    'arguments' => ['$target' => 'The remote to push to.'],
    // @todo Implement --pulldata.
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

/**
 * Implementation of drush_hook_COMMAND().
 */
function drush_deploya_remotes() {
  // @todo Check status and abstract out.
  $repo = new Repository(DRUPAL_ROOT);
  // @see _drush_sitealias_all_list()
  drush_sitealias_load_all();
  $aliases = drush_get_context('site-aliases');
  foreach ($aliases as $name => $alias) {
    if (drush_sitealias_is_remote_site($alias)) {
      $uri = "$alias[remote_user]@$alias[remote_host]";
      drush_log("Setting remote $name to $uri.", \Drush\Log\LogLevel::INFO);
      try {
        $remote = $repo->getRemote($name);
        $remote->setPushURL($uri);
        $remote->setFetchURL($uri);
      } catch (\InvalidArgumentException $e) {
        $remote = $repo->addRemote($name, $uri);
      }
    }
  }
}

function drush_deploya_push_code($target) {
  // @todo Check status and abstract out.
  $repo = new Repository(DRUPAL_ROOT);
  // @todo Ensure remote here.
  $repo->push($target);
}

function drush_deploya_pull() {
  $source = drush_get_option('source', 'live');
  $target = drush_get_option('target', 'self');
  if (drush_get_option('db', TRUE)) {
    drush_invoke_process("@self", 'sql-sync', ["@$source", "@$target"]);
  }
  if (drush_get_option('files', TRUE) && drush_get_option('public-files', TRUE)) {
    drush_invoke_process("@self", 'rsync', ["@$source:%public", "@$target:%public"], ['exclude_paths'=> drush_get_option('public-files-exclude-paths')]);
  }
  if (drush_get_option('files', TRUE) && drush_get_option('private-files', TRUE)) {
    drush_invoke_process("@self", 'rsync', ["@$source:%private", "@$target:%private"], ['exclude_paths'=> drush_get_option('private-files-exclude-paths')]);
  }
}
