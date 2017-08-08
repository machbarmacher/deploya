<?php
/**
 * @file deploya.drush.inc
 */

use Drush\Log\LogLevel;
use GitElephant\Repository;

/**
 * Implements hook_drush_command().
 */
function deploya_drush_command() {
  $items = array();

  $items['deploya-info'] = array(
    'callback' => 'deploya_info',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_ROOT,
    'aliases' => ['dinfo'],
    'description' => 'Get URIs for alias.',
    'arguments' => [
      'alias' => 'The remote installation\'s alias (without @).',
    ],
    'outputformat' => [
      'default' => 'key-value',
      'pipe-format' => 'var_export',
    ],
  );

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
      'pulldata' => 'Use --pulldata to pull data from "live" afterwards. Use --pulldata=foo to pull from foo.',
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

function deploya_info($alias_name) {
  $info = [];
  // @see _drush_sitealias_all_list()
  drush_sitealias_load_all();
  $aliases = drush_get_context('site-aliases');
  if (!isset($aliases["@$alias_name"])) {
    return drush_set_error('DEPLOYA_ERROR', dt('Unknown alias: !alias', ['!alias' => $alias_name]));
  }
  $alias = $aliases["@$alias_name"];
  $root_uri = _deploya_root_uri($alias);
  if ($root_uri) {
    $info['SFTP root'] = "sftp://$root_uri";
  }
  return $info;
}

function deploya_push_code($destination) {
  // @todo Check status and abstract out.
  $repo = new Repository(DRUPAL_ROOT);

  // @see _drush_sitealias_all_list()
  drush_sitealias_load_all();
  $aliases = drush_get_context('site-aliases');
  if (!isset($aliases["@$destination"])) {
    return drush_set_error('DEPLOYA_ERROR', dt('Unknown destination alias: !destination', ['!destination' => $destination]));
  }
  $destination_alias = $aliases["@$destination"];
  $uri = _deploya_push_code_uri($destination_alias);
  $branch = !empty($destination_alias['git-deployment-branch'])
    ? $destination_alias['git-deployment-branch'] : 'master';
  $t_args = [
    '!destination' => $destination,
    '!uri' => $uri,
  ];

  // We set up a named remote, but might as well push to uri.
  try {
    $destination_remote = $repo->getRemote($destination);
  } catch (\InvalidArgumentException $e) {
    drush_log('Adding remote !destination', $t_args);
    $repo->addRemote($destination, $uri);
    $destination_remote = $repo->getRemote($destination);
  }
  $current_uri = $destination_remote->getPushURL();
  $t_args += [
    '!current-uri' => $current_uri
  ];
  if ($current_uri !== $uri) {
    drush_log(dt('Setting uri for remote !destination from !current_uri to !uri.', $t_args), LogLevel::OK);
    $destination_remote->setPushURL($uri);
  }
  else {
    drush_log(dt('Checked uri for remote !destination is !uri.', $t_args));
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
  drush_log(dt('Pushing to remote: !remote', ['!remote' => $destination]), LogLevel::OK);
  $repo->push($destination, "+HEAD:$branch");
  $errorOutput = $repo->getCaller()->getErrorOutput();
  drush_log($errorOutput, LogLevel::OK);

  if ($pulldata_source = drush_get_option('pulldata')) {
    $pulldata_source = ($pulldata_source === TRUE) ? 'live' : $pulldata_source;
    // Pull via self as we don't know if remotes can access each other.
    drush_invoke_process("@self", 'deploya-pull-data', [], ['source' => $pulldata_source, 'destination' => $destination]);
  }
}

function _deploya_push_code_uri($alias) {
  if (!empty($alias['git-deployment-uri'])) {
    $uri = $alias['git-deployment-uri'];
  }
  else {
    $uri = _deploya_root_uri($alias, ':');

  }
  return $uri;
}

function _deploya_root_uri($alias, $separator = '') {
// Use parent directory of drupal root as git remote.
  $uri = dirname($alias['root']);
  if (!empty($alias['remote-host'])) {
    $uri = "{$alias['remote-host']}$separator$uri";
    if (!empty($alias['remote-user'])) {
      $uri = "{$alias['remote-user']}@$uri";
    }
  }
  return $uri;
}

function deploya_pull() {
  $source = drush_get_option('source', 'live');
  $destination = drush_get_option('destination', 'self');
  $t_args = ['!source' => $source, '!destination' => $destination];

  drush_sitealias_load_all();
  $aliases = drush_get_context('site-aliases');
  if (!isset($aliases["@$source"])) {
    return drush_set_error('DEPLOYA_ERROR', dt('Unknown source: !source', $t_args));
  }
  if (!isset($aliases["@$destination"])) {
    return drush_set_error('DEPLOYA_ERROR', dt('Unknown destination: !destination', $t_args));
  }


  if ($source == $destination) {
    return drush_set_error('DEPLOYA_ERROR', dt('No need to pull data from !source to !destination', $t_args));
  }

  // Prompt for confirmation. This is destructive.
  if (!drush_get_context('DRUSH_SIMULATE')) {
    drush_print(dt("You will delete data in !destination and replace with data from !source", $t_args));
    if (!drush_confirm(dt('Do you really want to continue?'))) {
      return drush_user_abort();
    }
  }

  if (drush_get_option('db', TRUE)) {
    drush_log(dt('Pulling DB...'), LogLevel::OK);
    drush_invoke_process("@self", 'sql-sync', ["@$source", "@$destination"]);
  }
  _deploya_rsync($source, $destination, '%files', 'public');
  // That function will check if the file path does exist.
  _deploya_rsync($source, $destination, '%private', 'private');
}

function _deploya_rsync($source, $destination, $path, $prefix) {
  $t_args = ['!prefix' => $prefix];
  if (!drush_get_option('files', TRUE) || !drush_get_option("$prefix-files", TRUE)) {
    drush_log(dt('Skipping !prefix files...', $t_args));
    return;
  }
  $source_path = "@$source:$path";
  $destination_path = "@$destination:$path";

  // @see drush_core_rsync()
  $additional_options = [];
  $destination_settings = drush_sitealias_evaluate_path($destination_path, $additional_options, FALSE, "rsync", 'destination-');
  $source_settings = drush_sitealias_evaluate_path($source_path, $additional_options, FALSE, "rsync", 'source-');
  if (!isset($source_settings['evaluated-path']) || !isset($destination_settings['evaluated-path'])) {
    drush_log(dt("No file path: !prefix", $t_args), LogLevel::WARNING);
    return;
  }

  drush_log(dt('Pulling !prefix files...', $t_args), LogLevel::OK);

  drush_invoke_process("@self", 'rsync', [
    $source_path,
    $destination_path
  ], ['exclude-paths' => drush_get_option("$prefix-files-exclude-paths")]);
}
