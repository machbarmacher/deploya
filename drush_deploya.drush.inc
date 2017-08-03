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
    'description' => "",
  );

  $items['deploya-push-code'] = array(
  );

  $items['deploya-pull-db'] = array(
  );

  $items['deploya-pull-files'] = array(
  );

  $items['deploya-move-db'] = array(
  );

  $items['deploya-move-files'] = array(
  );

  return $items;
}

/**
 * Implementation of drush_hook_COMMAND().
 */
function drush_deploya_remotes() {
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
