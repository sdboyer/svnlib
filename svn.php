<?php

// TODO temporary straight includes until a smarter system is introduced
require_once dirname(__FILE__) . '/lib.inc';
require_once dirname(__FILE__) . '/parsers.inc';
require_once dirname(__FILE__) . '/commands/svn.commands.inc';
require_once dirname(__FILE__) . '/opts/svn.opts.inc';

/**
 * Abstract parent class for a Subversion 'instance,' i.e., working copy or
 * repository.
 *
 * SvnWorkingCopy and SvnRepository are the concrete subclasses that extend this
 * class to provide that functionality.
 * 
 * @author sdboyer
 *
 */
abstract class SvnInstance extends SplFileInfo implements CLIWrapper {
  protected $cmd;

  /**
   * We use this object to prevent the creation of circular references, since
   * PHP's circular reference handling kinda blows before 5.3.
   *
   * @var SvnCommandConfig
   */
  protected $config;

  const PASS_CONFIG    = 0x001;
  const PASS_DEFAULTS  = 0x002;
  const USE_DEFAULTS   = 0x004;
  const PCUD           = 0x005;

  public function __construct($path, $verify = TRUE) {
    if ($verify) {
      $this->verify($path);
    }
    parent::__construct($path);

    $this->config = new SvnCommandConfig();
    $this->config->attachInstance($this);

    // Because it's very easy for the svnlib to fail (hard and with weird errors)
    // if a config dir isn't present, we set it to the unintrusive default that
    // ships with svnlib.
    $this->config->configDir = dirname(__FILE__) . DIRECTORY_SEPARATOR . 'configdir';

    $this->getInfo();
  }

  protected function getInfo() {
    $output = $this->svn('info', self::PASS_CONFIG)->target('.')->execute();

    preg_match('/^Repository Root: (.*)\n/m', $output, $root);
    $this->config->repoRoot = $root[1];
    preg_match('/^Revision: (.*)\n/m', $output, $rev);
    $this->config->latestRev = (int) $rev[1];

    preg_match('/^URL: (.*)\n/m', $output, $url);
    if ($url[1] != $this->config->repoRoot) {
      // Do this with a separate $subpath variable to ensure we don't get any
      // icky trailing slashes messing it up.
      $subpath = substr($url[1], strlen($this->config->repoRoot));
      $this->setSubPath($subpath);
      // Need to re-call the SplFileInfo constructor to point to the real root
      parent::__construct(substr($this, 0, strlen($subpath)));
    }
  }

  public function getRootPath() {
    return (string) $this. (empty($this->config->subPath) ? '' : DIRECTORY_SEPARATOR . $this->config->subPath);
  }

  public function getRepoRoot() {
    return $this->config->repoRoot;
  }

  public function getLatestRev() {
    return $this->config->latestRev;
  }

  /**
   * Set a path, relative to the base path that was passed in to the SvnInstance
   * constructor, that should be used as the base path for all path-based
   * operations. Useful for, say, specifying a particular branch or tag that
   * operations should be run against in a way that will be transparent to the
   * subcommand invocations.
   *
   * IMPORTANT NOTE: internal handling of subpaths becomes complex if you change
   * the subpath while in the midst of queuing up a command. This internal
   * behavior is also different for repositories than it is for working copies.
   *
   * @param string $path
   */
  public function setSubPath($path) {
    $this->config->subPath = trim($path, '/');
  }

  /**
   * Appends additional subdirectories onto the current subpath
   *
   * @param string $path
   */
  public function appendSubPath($path) {
    // FIXME stupid dir separator, when to add it?
    $this->config->subPath .= DIRECTORY_SEPARATOR . trim($path, '/');
  }

  abstract public function verify($path);

  abstract public function getPrependPath();
  
  /**
   *
   * @param string $subcommand
   * @param bool $defaults
   * @return SvnCommand
   */
  abstract public function svn($subcommand, $defaults = self::PCUD);

  public function __call($name, $arguments) {
    if (method_exists($this->config, $name)) {
      call_user_func_array(array($this->config, $name), $arguments);
    }
    throw new Exception('Method ' . $name . ' is unknown.', E_RECOVERABLE_ERROR);
  }
}

/**
 * Class for managing the root of an Subversion working copy.
 *
 * Once created, it can spawn various invocations of the svn command-line to
 * gather information about or perform operations on the working copy.
 *
 * @author sdboyer
 *
 */
class SvnWorkingCopy extends SvnInstance {

  const NO_AUTH_CACHE   = 0x001;

  public function getRepository() {
    return new SvnRepository($this->config->repoRoot);
  }

  public function verify($path) {
    if (!is_dir($path)) {
      throw new Exception(get_class($this) . ' requires a directory argument, but "' . $path . '" was provided.', E_RECOVERABLE_ERROR);
    }

    if (!is_dir($path . DIRECTORY_SEPARATOR . '.svn')) {
      throw new Exception($path . " contains no svn metadata; it is not a working copy directory.", E_RECOVERABLE_ERROR);
    }
  }

  public function getWorkingPath() {
    return $this->getRootPath();
  }

  public function getPrependPath() {
    return;
  }

  public function svn($subcommand, $defaults = self::PCUD) {
    $classname = 'svn' . $subcommand;
    if (!class_exists($classname)) {
      throw new Exception("Invalid svn subcommand '$subcommand' was requested.", E_RECOVERABLE_ERROR);
    }
    $this->cmd = new $classname($this->config, $defaults);

    return $this->cmd;
  }
}

/**
 * Defines a Subversion repository location, and allows commands ordinarily
 * associated with the command line to be invoked on it.
 *
 * Note that this class is a bad SplFileInfo citizen, and calling certain of the
 * SplFileInfo methods on it WILL cause a php fatal error. We use it because the
 * methods SplFileInfo provides that do work are very handy, and it would be
 * foolish to reimplement in userland what's already been done in C.
 */
class SvnRepository extends SvnInstance {
  // Repo protocol capability flags
  const WRITE_CAPABLE = 0x001;
  const CAN_SVNADMIN  = 0x002;

  public static $protocols = array(
    'http' => 0,
    'https' => 0,
    'svn' => 0,
    'svn+ssh' => 1,
    'file' => 3,
  );

  public function verify($path) {
    // Need to explode out the URL into its respective parts, first
    if (preg_match('@^[A-Za-z+]+://@', $path)) {
      list($protocol, $path) = preg_split('@://@', $path, 2);
    }
    else { // assume it's a plain path, which means a local file - so, file://
      $protocol = 'file';
    }

    // Run a fast, low-overhead operation, verifying this is a working svn repository.
    if (self::$protocols[$protocol] & self::CAN_SVNADMIN) {
      system('svnadmin lstxns ' . escapeshellarg($path), $exit);
    }
    else {
      system('svn info --config-dir ' . dirname(__FILE__) . DIRECTORY_SEPARATOR . 'configdir ' . (string) $this, $exit);
    }
    if (!empty($exit)) {
      throw new Exception($path . " is not a valid Subversion repository.", E_RECOVERABLE_ERROR);
    }
  }

//  protected function getInfo() {
//    parent::getInfo();
//    $pieces = explode('://', (string) $this);
//    $this->protocol = $pieces[0];
//  }

  /**
   * Get the path to be prepended to individual file items
   * @return string
   */
  public function getPrependPath() {
    return $this->getRootPath() . DIRECTORY_SEPARATOR;
  }

  public function getWorkingPath() {
    return NULL;
  }

  public function svn($subcommand, $defaults = self::PCUD) {
    $classname = 'Svn' . $subcommand;
    if (!class_exists($classname)) {
      throw new Exception("Invalid svn subcommand '$subcommand' was requested.", E_RECOVERABLE_ERROR);
    }
    $reflection = new ReflectionClass($classname);
    if (!$reflection->getStaticPropertyValue('operatesOnRepositories')) {
      throw new Exception('Subversion repositories cannot do anything with the ' . $subcommand . ' svn subcommand.', E_RECOVERABLE_ERROR);
    }
    if ($reflection->isSubclassOf('SvnWrite') && !$this->isWritable()) {
      throw new Exception("Write operation '$subcommand' was requested, but the repository is not writable from here.", E_RECOVERABLE_ERROR);
    }

    $this->cmd = new $classname($this->config, $defaults);
    return $this->cmd;
  }

  /**
   * Indicate whether or not it is possible to perform write operations directly
   * on the repository.
   *
   * @return bool
   */
  public function isWritable() {
    // TODO write capable just gets us in the door, we then need to run
    // more checks
    return self::$protocols[$this->protocol] & self::PCUD;
  }

  public function svnadmin($subcommand, $defaults = NULL) {
    $classname = 'Svnadmin' . $subcommand;
    if (!class_exists($classname)) {
      throw new Exception("Invalid svnadmin subcommand '$subcommand' was requested.", E_RECOVERABLE_ERROR);
    }

    $this->cmd = new $classname($this, is_null($defaults) ? $this->defaults : $defaults);
    return $this->cmd;
  }
}

class SvnCommandConfig {
  /**
   *
   * @var SvnInstance
   */
  protected $instance;
  public $username = NULL;
  public $password = NULL;
  public $configDir = NULL;
  public $subPath = '';
  public $repoRoot;
  public $latestRev;
  public $protocol;
  public $path;

  public function __construct() {}

  public function attachInstance(SvnInstance $instance) {
    $this->instance = $instance;
  }

  public function username($username) {
    $this->username = $username;
  }

  public function password($password) {
    $this->password = $password;
  }

  public function configDir($configDir) {
    $this->configDir = $configDir;
  }

  public function getPrependPath() {
    return $this->instance->getPrependPath();
  }

  public function getWorkingPath() {
    return $this->instance->getWorkingPath();
  }
}

/**
 * Helper function that retrieves an actual Subversion repository as an
 * SvnInstance object, even if a working copy path is passed in.
 *
 * @param string $path
 * @return SvnRepository
 */
function svnlib_get_repository($path) {
  try {
    $repo = new SvnRepository($path);
  } catch (Exception $e) {
    $wc = new SvnWorkingCopy($path);
    $repo = $wc->getRepository();
    unset($wc);
  }
  return $repo;
}
