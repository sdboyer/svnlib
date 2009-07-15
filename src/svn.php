<?php

// TODO temporary straight includes until a smarter system is introduced
require_once dirname(__FILE__) . '/lib.inc';
require_once dirname(__FILE__) . '/proc.inc';
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

  public function __construct($path, SvnCommandConfig $config = NULL, $verify = TRUE) {
    if ($verify) {
      $this->verify($path);
    }
    parent::__construct($path);

    $this->config = is_null($config) ? new SvnCommandConfig() : $config;
    $this->config->attachWrapper($this);

    $this->getInfo();
  }

  protected function getInfo() {
    $output = $this->svn('info', $proc, self::PASS_CONFIG)->target('.')->execute();

    preg_match('/^Repository Root: (.*)\n/m', $output[1], $root);
    $this->config->repoRoot = $root[1];
    preg_match('/^Revision: (.*)\n/m', $output[1], $rev);
    $this->config->latestRev = (int) $rev[1];

    preg_match('/^URL: (.*)\n/m', $output[1], $url);
    if ($url[1] != $this->config->repoRoot) {
      // Do this with a separate $subpath variable to ensure we don't get any
      // icky trailing slashes messing it up.
      $subpath = substr($url[1], strlen($this->config->repoRoot));
      $this->setSubPath($subpath);
      // Need to re-call the SplFileInfo constructor to point to the real root
      parent::__construct(substr($this, 0, -strlen($subpath)));
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
   * To reset the current subpath, simply pass an empty string to this method.
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
    $this->config->subPath .= (empty($this->config->subPath) ? '' : DIRECTORY_SEPARATOR) . trim($path, '/');
  }

  abstract public function verify($path);

  abstract public function getPrependPath();

  /**
   *
   * @param string $subcommand
   * @param bool $defaults
   * @return SvnCommand
   */
  abstract public function svn($subcommand, CLIProcHandler &$proc = NULL, $defaults = self::PCUD);

  protected function buildCommand($classname, &$proc) {
    if (!class_exists($classname)) {
      throw new InvalidArgumentException("Invalid svn subcommand class '$classname' was requested.", E_RECOVERABLE_ERROR);
    }
    $reflection = new ReflectionClass($classname);
    $this->getProcHandler($reflection, $proc);
    return $reflection;
  }

  /**
   * Kind of a silly little helper function. Verifies the requested command
   * class exists, then figures out what the proc handler should be. Returns the
   * reflection class for the concrete implementation to mess around with as
   * needed.
   *
   * @param string $classname
   * @return void
   */
  protected function getProcHandler($reflection, &$proc) {
    if (is_null($proc)) {
      // If no proc handler was explicitly specified, then first try the global
      // proc handler specified on this CLIWrapper's config object (if any).
      if (!empty($this->config->proc) && $this->config->proc instanceof CLIProcHandler) {
        $proc = &$this->config->proc;
      }
      // Lowest priority (and most common) case: no global proc handler
      // specified on this CLIWrapperConfig object; use the default specified by
      // the command. This is the most common case.
      else {
        $proc_class = $reflection->getConstant('PROC_HANDLER');
        if (!class_exists($proc_class)) {
          throw new InvalidArgumentException("The requested command specified an unknown class, '$proc_class', as the default process handler", E_RECOVERABLE_ERROR);
        }
        $proc = new $proc_class();
        $proc->attachConfig($this->config);
      }
    }
    if (!$proc instanceof CLIProcHandler) {
      throw new LogicException("Svnlib was unable to create a process handler.", E_RECOVERABLE_ERROR);
    }
  }

  public function __call($name, $arguments) {
    if (method_exists($this->config, $name)) {
      return call_user_func_array(array($this->config, $name), $arguments);
    }
    throw new BadMethodCallException('Method ' . $name . ' is unknown.', E_RECOVERABLE_ERROR);
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
      throw new InvalidArgumentException(get_class($this) . ' requires a directory argument, but "' . $path . '" was provided.', E_RECOVERABLE_ERROR);
    }

    if (!is_dir($path . DIRECTORY_SEPARATOR . '.svn')) {
      throw new InvalidArgumentException($path . " contains no svn metadata; it is not a working copy directory.", E_RECOVERABLE_ERROR);
    }
  }

  public function getWorkingPath() {
    return $this->getRootPath();
  }

  public function getPrependPath() {}

  public function svn($subcommand, CLIProcHandler &$proc = NULL, $defaults = self::PCUD) {
    $classname = 'svn' . $subcommand;
    $reflection = $this->buildCommand($classname, $proc);
    return new $classname($this->config, $proc, $defaults);
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
      throw new InvalidArgumentException($path . " is not a valid Subversion repository.", E_RECOVERABLE_ERROR);
    }
  }

  protected function getInfo() {
    parent::getInfo();
    $pieces = explode('://', (string) $this);
    $this->config->protocol = $pieces[0];
  }

  /**
   * Get the path to be prepended to individual file items
   * @return string
   */
  public function getPrependPath() {
    return $this->getRootPath() . DIRECTORY_SEPARATOR;
  }

  public function getWorkingPath() {}

  public function svn($subcommand, CLIProcHandler &$proc = NULL, $defaults = self::PCUD) {
    $classname = 'Svn' . $subcommand;
    $reflection = $this->buildCommand($classname, $proc);
    if (!$reflection->getConstant('OPERATES_ON_REPOSITORIES')) {
      throw new InvalidArgumentException('Subversion repositories cannot do anything with the ' . $subcommand . ' svn subcommand.', E_RECOVERABLE_ERROR);
    }
    if ($reflection->isSubclassOf('SvnWrite') && !$this->isWritable()) {
      throw new InvalidArgumentException("Write operation '$subcommand' was requested, but the repository is not writable from here.", E_RECOVERABLE_ERROR);
    }

    return new $classname($this->config, $proc, $defaults);
  }

  /**
   * Indicate whether or not it is possible to perform write operations directly
   * on the repository.
   *
   * @return bool
   */
  public function isWritable() {
    // TODO WRITE_CAPABLE just gets us in the door, we then need to run
    // more checks
    return self::$protocols[$this->config->protocol] & self::WRITE_CAPABLE;
  }

  public function svnadmin($subcommand, CLIProcHandler $proc = NULL, $defaults = NULL) {
    $classname = 'Svnadmin' . $subcommand;
    $reflection = $this->getProcHandler($classname, $proc);

    $cmd = new $classname($this, is_null($defaults) ? $this->defaults : $defaults);
    $proc->attachCommand($cmd);
    return $cmd;
  }

  /**
   * Creates a new working copy checkout of this repository at the location
   * specified in the first parameter.
   *
   * If an SvnCommandConfig object is provided, the new SvnWorkingCopy object
   * will use it; otherwise it will spawn its own (per the default behavior).
   *
   * @param string $path
   * @param SvnCommandConfig $config
   * @return SvnWorkingCopy
   */
  public function checkoutWorkingCopy($path, SvnCommandConfig $config = NULL) {
    $this->config->usePrependPath = FALSE;
    $this->svn('checkout')
      ->target($path)
      ->target($this->getRepoRoot())
      ->execute();
    $this->config->usePrependPath = TRUE;
    return new SvnWorkingCopy($path, $config);
  }
}

class SvnCommandConfig implements CLIWrapperConfig {
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
  public $usePrependPath = TRUE;

  public function __construct() {
    // Because it's very easy for the svnlib to fail (hard and with weird errors)
    // if a config dir isn't present, we set it to the unintrusive default that
    // ships with svnlib.
    $this->configDir = dirname(__FILE__) . DIRECTORY_SEPARATOR . 'configdir';
  }

  public function attachWrapper(CLIWrapper &$wrapper) {
    $this->instance = &$wrapper;
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
  } catch (InvalidArgumentException $e) {
    $wc = new SvnWorkingCopy($path);
    $repo = $wc->getRepository();
    unset($wc);
  }
  return $repo;
}
