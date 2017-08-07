<?php
/**
 * @file deploya.drush.inc
 */

use GitElephant\Repository;

/**
 * Implements hook_drush_command().
 */
function deploya_drush_command() {
  $items = array();

  $items['deploya-push-code'] = array(
    'callback' => 'deploya_push_code',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_ROOT,
    'aliases' => ['dpushc'],
    'description' => 'Push code to other installation.',
    'arguments' => [
      '$destination' => 'The remote installation\'s alias (without @) to push to.',
    ],
    'required-arguments' => TRUE,
    'options' => [
      'fix-no-ff' => 'Do a pull -s=ours first. Use if pushing is rejected due to "non fast-forward".',
      'pulldata' => 'If set, pull data on destination afterwards.',
      'pulldata-source' => 'If pulldata is set, the source to pull data from. Defaults to "live".',
    ],
  );

  $items['deploya-pull-data'] = array(
    'callback' => 'deploya_pull',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_ROOT,
    'aliases' => ['dpulld'],
    'description' => 'Pull DB and files from live.',
    'options' => [
      'source' => 'Source alias (without @), defaults to "live".',
      'destination' => 'Destination  alias (without @), if different from current site.',
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

function deploya_push_code($destination) {
  // @todo Check status and abstract out.
  $repo = new Repository(DRUPAL_ROOT);

  // @see _drush_sitealias_all_list()
  drush_sitealias_load_all();$aliases = drush_get_context('site-aliases');
  if (!isset($aliases["@$destination"])) {
    return drush_set_error("Unknown destination alias: $destination");
  }
  $destination_alias = $aliases["@$destination"];
  $uri = _deploya_push_code_uri($destination_alias);
  $branch = !empty($destination_alias['git-deployment-branch'])
    ? $destination_alias['git-deployment-branch'] : 'master';

  // We set up a named remote, but might as well push to uri.
  try {
    $destination_remote = $repo->getRemote($destination);
  } catch (\InvalidArgumentException $e) {
    drush_log("Adding remote $destination");
    $repo->addRemote($destination, $uri);
    $destination_remote = $repo->getRemote($destination);
  }
  $current_uri = $destination_remote->getPushURL();
  $t_args = [
    '!destination' => $destination,
    '!uri' => $uri,
    '!current-uri' => $current_uri
  ];
  if ($current_uri !== $uri) {
    drush_log(dt("Setting remote $destination from '$current_uri' to '$uri'.", $t_args), \Drush\Log\LogLevel::OK);
    $destination_remote->setPushURL($uri);
  }
  else {
    drush_log(dt("Checked remote $destination is '$uri'.", $t_args));
  }

  if (drush_get_option('fix-no-ff')) {
    // Do a pull --autostash --strategy=ours
    // Git can autostash for pull, but needs 2.9 for --autostash, and only 2.6 for -c rebase.autoStash=true
    $repo->addGlobalConfig('rebase.autoStash', 'true');
    $repo->addGlobalCommandArgument('--strategy=ours');
    $repo->pull($destination, 'HEAD', FALSE);
    $repo->removeGlobalCommandArgument('--strategy=ours');
    $repo->removeGlobalConfig('rebase.autoStash');
  }

  // Do it.
  drush_log(dt('Push to remote: !remote', ['!remote' => $destination]));
  $repo->push($destination, "+HEAD:$branch");
  // @fixme Find a way to print command output.

  if (drush_get_option('pulldata')) {
    $pulldata_source = drush_get_option('pulldata-source', 'live');
    drush_invoke_process("@$destination", 'deploya-pull-data', [], ['source' => $pulldata_source]);
  }
}

function _deploya_push_code_uri($destination_alias) {
  if (!empty($destination_alias['git-deployment-uri'])) {
    $uri = $destination_alias['git-deployment-uri'];
  }
  else {
    // Use parent directory of drupal root as git remote.
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

function deploya_pull() {
  $source = drush_get_option('source', 'live');
  $destination = drush_get_option('destination', 'self');

  if ($source == $destination) {
    drush_log("No need to pull from $source to $destination.");
  }

  if (drush_get_option('db', TRUE)) {
    drush_invoke_process("@self", 'sql-sync', ["@$source", "@$destination"]);
  }
  _deploya_rsync($source, $destination, '%files', 'public');
  _deploya_rsync($source, $destination, '%private', 'private');
}

function _deploya_rsync($source, $destination, $path, $prefix) {
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
