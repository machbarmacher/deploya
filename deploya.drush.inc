<?php
/**
 * @file deploya.drush.inc
 */

use Drush\Log\LogLevel;
use GitElephant\Repository;

/**
 * Implements hook_drush_command().
 *
 * @todo Add command to generate and show ssh key.
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

  $items['deploya-allow'] = array(
    'callback' => 'deploya_allow_ssh',
    'bootstrap' => DRUSH_BOOTSTRAP_NONE,
    'aliases' => ['dallow'],
    'description' => 'Allow to ssh from source to destination.',
    'arguments' => [
      'source' => 'Source alias (without @).',
      'destination' => 'Destination alias (without @).',
    ],
  );

  return $items;
}

function deploya_info($alias_name) {
  _deploya_autoload();
  $info = [];
  $alias = drush_sitealias_get_record("@$alias_name");
  if (!$alias) {
    return drush_set_error('DEPLOYA_ERROR', dt('Unknown alias: !alias', ['!alias' => $alias_name]));
  }
  if ($root_uri = _deploya_root_uri($alias)) {
    $info['SFTP root'] = "sftp://$root_uri";
  }
  if ($ssh = _deploya_remote_login($alias)) {
    $info['SSH'] = $ssh;
  }
  return $info;
}

function deploya_push_code($destination) {
  _deploya_autoload();
  // @todo Check status and abstract out.
  $repo = new Repository(DRUPAL_ROOT);

  $destination_alias = _deploya_get_remote_alias($destination);
  if (!$destination_alias) {
    return drush_set_error('DEPLOYA_ERROR', dt('Unknown destination alias: !destination', ['!destination' => $destination]));
  }
  if (!isset($destination_alias['remote-host']) || !isset($destination_alias['remote-user'])) {
    return drush_set_error('DEPLOYA_ERROR', dt('Non-remote destination alias: !destination', ['!destination' => $destination]));
  }
  $uri = _deploya_push_code_uri($destination_alias);
  $branch = !empty($destination_alias['git-deployment-branch'])
    ? $destination_alias['git-deployment-branch'] : 'master';
  $t_args = [
    '!destination' => $destination,
    '!uri' => $uri,
  ];

  // We set up a named remote, but might as well push to uri.
  try {
    $destination_remote = $repo->getRemote($destination, FALSE);
  } catch (\InvalidArgumentException $e) {
    // Remote does not exist.
    drush_log('Adding remote !destination', $t_args);
    $repo->addRemote($destination, $uri);
    $destination_remote = $repo->getRemote($destination);
  } catch (\Exception $e) {
    // Something else happened.
  }
  $current_uri = $destination_remote->getPushURL();
  $t_args += [
    '!current_uri' => $current_uri
  ];
  if ($current_uri !== $uri) {
    drush_log(dt('Setting uri for remote !destination from !current_uri to !uri.', $t_args), LogLevel::OK);
    $repo->getCaller()->execute(sprintf('remote set-url %s %s',
      drush_escapeshellarg($destination), drush_escapeshellarg($uri)));
  }
  else {
    drush_log(dt('Checked uri for remote !destination is !uri.', $t_args));
  }

  if (drush_get_option('fix-no-ff')) {
    // Do a pull --autostash --strategy=ours
    // Git can autostash for pull, but needs 2.9 for --autostash, and only 2.6 for -c rebase.autoStash=true
    $repo->addGlobalConfig('rebase.autoStash', 'true');
    $repo->addGlobalCommandArgument('--strategy=ours');
    // Needed since git 2.9. @todo Not on lower version.
    $repo->addGlobalCommandArgument('--allow-unrelated-histories');
    $repo->pull($destination, 'HEAD', FALSE);
    $repo->removeGlobalCommandArgument('--allow-unrelated-histories');
    $repo->removeGlobalCommandArgument('--strategy=ours');
    $repo->removeGlobalConfig('rebase.autoStash');
  }

  // Do it.
  $local_branch = $repo->getMainBranch()->getName();
  _deploya_log_branch_info($repo, $local_branch);
  $repo->fetch($destination, $branch);
  _deploya_log_branch_info($repo, "refs/remotes/$destination/$branch");
  drush_log(dt('Pushing...'), LogLevel::OK);
  $repo->push($destination, "+HEAD:$branch");
  _deploya_log_branch_info($repo, "refs/remotes/$destination/$branch");

  if ($pulldata_source = drush_get_option('pulldata')) {
    $pulldata_source = ($pulldata_source === TRUE) ? 'live' : $pulldata_source;
    // Pull via self as we don't know if remotes can access each other.
    // This breaks for rsync remote / remote.
    // @todo Pull from remote, after checking and fixing live access.
    drush_invoke_process("@self", 'deploya-pull-data', [], ['source' => $pulldata_source, 'destination' => $destination]);
  }
}

function _deploya_get_remote_alias($alias_name) {
  $alias = drush_sitealias_get_record("@$alias_name");
  if (isset($alias['site-list'])) {
    $aliases = drush_sitealias_resolve_sitelist($alias);
    $alias = _deploya_intersect($aliases);
  }
  return $alias;
}

function _deploya_intersect($aliases) {
  // Get all keys with equal value.
  // Muscle this as array_intersect_assoc only works with string values.
  if ($aliases) {
    $result = array_shift($aliases);
  }
  else {
    $result = [];
  }
  foreach ($aliases as $alias) {
    foreach ($alias as $key => $value) {
      $equal = isset($result[$key]) && $value === $result[$key];
      if (!$equal) {
        unset($result[$key]);
      }
    }
  }
  return $result;
}

function _deploya_log_branch_info(Repository $repo, $ref) {
  $commit = $repo->getCommit($ref);
  drush_log(dt('  !branch !sha !message', [
    '!branch' => $ref,
    '!sha' => $commit->getSha(TRUE),
    '!message' => $commit->getMessage(),
  ]), LogLevel::OK);
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
  $path = dirname($alias['root']);
  $login = _deploya_remote_login($alias);
  $uri = "$login$separator$path";
  return $uri;
}

function _deploya_remote_login($alias) {
  $login = '';
  if (!empty($alias['remote-host'])) {
    $login = "{$alias['remote-host']}";
    if (!empty($alias['remote-user'])) {
      $login = "{$alias['remote-user']}@$login";
    }
    if (!empty($alias['ssh-options'])) {
      $login = "$login {$alias['ssh-options']}";
    }
  }
  return $login;
}

function deploya_allow_ssh($source, $destination) {
  _deploya_autoload();
  $source_alias = _deploya_get_remote_alias($source);
  $destination_alias = _deploya_get_remote_alias($destination);
  if (!$source_alias) {
    return drush_set_error('DEPLOYA_ERROR', dt('Unknown destination alias: !alias', ['!alias' => $source]));
  }
  if (!isset($source_alias['remote-host']) || !isset($source_alias['remote-user'])) {
    return drush_set_error('DEPLOYA_ERROR', dt('Non-remote destination alias: !alias', ['!alias' => $source]));
  }
  if (!$destination_alias) {
    return drush_set_error('DEPLOYA_ERROR', dt('Unknown destination alias: !alias', ['!alias' => $destination]));
  }
  if (!isset($destination_alias['remote-host']) || !isset($destination_alias['remote-user'])) {
    return drush_set_error('DEPLOYA_ERROR', dt('Non-remote destination alias: !alias', ['!alias' => $destination]));
  }

  $result = drush_invoke_process($source_alias, 'exec', ['ssh', '-q', _deploya_remote_login($destination_alias), 'exit']);
  if (empty($result['error_status'])) {
    drush_log(dt('Login attampt succeeded, nothing to do.'), LogLevel::OK);
    return;
  }
  else {
    drush_log(dt('Login attampt failed, transferring key.'), LogLevel::OK);
  }

  $pubkey = _deploya_get_pubkey($source_alias);

  if (!$pubkey) {
    drush_log(dt('Generating key.'), LogLevel::OK);
    drush_invoke_process($source_alias, 'exec', ['cd', '&&', 'ssh-keygen', '-t', 'rsa', '-f', '.ssh/id_rsa']);
    $pubkey = _deploya_get_pubkey($source_alias);
  }
  if (!$pubkey) {
    return drush_set_error('DEPLOYA_ERROR', dt('Can not get pubkey.'));
  }
  $pubkeyfile = drush_save_data_to_temp_file($pubkey);

  drush_invoke_process('@self', 'exec', ['ssh-copy-id', _deploya_remote_login($destination_alias), '-i', $pubkeyfile]);
  drush_log(dt('Transferred key.'), LogLevel::SUCCESS);
}

/**
 * @param array $source_alias
 * @return string|null
 */
function _deploya_get_pubkey($source_alias) {
  //$result = drush_invoke_process($source_alias, 'exec', ['cd', '&&', 'cat', '.ssh/id_rsa.pub'], []);
  // Muscle this as drush_invoke_process does not return command output.
  $cmd = 'cat ~/.ssh/id_rsa.pub';
  $exec = _drush_backend_generate_command($source_alias, $cmd);
  $success = drush_shell_exec($exec);
  $output = drush_shell_exec_output();

  if ($success && $output) {
    return $output[0];
  }
  else {
    return NULL;
  }
}

function deploya_pull() {
  _deploya_autoload();
  $source = drush_get_option('source', 'live');
  $destination = drush_get_option('destination', 'self');
  $t_args = ['!source' => $source, '!destination' => $destination];

  if (!drush_sitealias_get_record("@$source")) {
    return drush_set_error('DEPLOYA_ERROR', dt('Unknown source: !source', $t_args));
  }
  if (!drush_sitealias_get_record("@$destination")) {
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
    // Dunno why we need this sometimes.
    drush_sitealias_create_self_alias();
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
  if (
    !isset($source_settings['path-aliases'][$path])
    || !isset($destination_settings['path-aliases'][$path])
  ) {
    drush_log(dt("Skipping nonexistent file path: !prefix", $t_args), LogLevel::WARNING);
    return;
  }

  drush_log(dt('Pulling !prefix files...', $t_args), LogLevel::OK);

  drush_invoke_process("@self", 'rsync', [
    $source_path,
    $destination_path
  ], ['exclude-paths' => drush_get_option("$prefix-files-exclude-paths")]);
}

function _deploya_autoload() {
  $autoloaderLocations = [
    __DIR__ . '/../vendor/autoload.php',
    __DIR__ . '/../../../vendor/autoload.php',
    // drush/deploya
    __DIR__ . '/../../vendor/autoload.php',
    // docroot/sites/all/drush/deploya
    __DIR__ . '/../../../../../vendor/autoload.php',
  ];
  foreach ($autoloaderLocations as $file) {
    if (is_file($file)) {
      require $file;
    }
  }
}
