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
  public $defaults = TRUE;
  protected $cmd;
  // protected $cmdSwitches = 0, $cmdOpts = array();
  // public $invocations, $cmdContainer, $retContainer, $errContainer;
  protected $subPath = '';
  public $username, $password, $configDir;

  public function __construct($path, $verify = TRUE) {
    parent::__construct($path);

   // Because it's very easy for the svnlib to fail (hard and with weird errors)
   // if a config dir isn't present, we set it to the unintrusive default that
   // ships with svnlib.
   $this->configDir = dirname(__FILE__) . DIRECTORY_SEPARATOR . 'configdir';

    if ($verify) {
      $this->verify();
    }
    $this->getInfo();
    // $this->retContainer = new SplObjectMap();
    // $this->errContainer = new SplObjectMap();
  }

  protected function getInfo() {
    $orig_subpath = $this->subPath;
    $this->subPath = NULL;

    $output = $this->svn('info', FALSE)->target('.')->configDir($this->configDir)->execute();

    preg_match('/^Repository Root: (.*)\n/m', $output, $root);
    $this->repoRoot = $root[1];
    preg_match('/^Revision: (.*)\n/m', $output, $rev);
    $this->latestRev = (int) $rev[1];

    $this->subPath = $orig_subpath;
  }

  /**
   * Set a path, relative to the base path that was passed in to the SvnInstance
   * constructor, that should be used as the base path for all path-based
   * operations. Primarily useful for specifying a particular branch or tag that
   * operations should be run against in a way that will be transparent to the
   * subcommand invocations.
   *
   * IMPORTANT NOTE: internal handling of subpaths becomes copmlex if you change
   * the subpath while in the midst of queuing up a command. This internal
   * behavior is also different for repositories than it is for working copies.
   *
   * @param string $path
   */
  public function setSubPath($path) {
    $this->subPath = trim($path, '/');
  }

  abstract public function verify();

  public function getRootPath() {
    if (empty($this->subPath)) {
      return (string) $this;
    }
    else {
      return (string) $this . DIRECTORY_SEPARATOR . $this->subPath;
    }
  }


  public function defaultConfigDir() {

  }

  abstract public function getPrependPath();
  /**
   *
   * @param string $subcommand
   * @param bool $defaults
   * @return SvnCommand
   */
  abstract public function svn($subcommand, $defaults = NULL);
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
  protected $repoRoot;
  protected $latestRev;

  const NO_AUTH_CACHE   = 0x001;

  public function getRepository() {
    return new SvnRepository($this->repoRoot);
  }

  public function __get($name) {
    switch ($name) {
      case 'repoRoot':
      case 'latestRev':
        if (!$this->$name) {
          $this->getInfo();
        }
        return $this->$name;
    }
    return;
  }

  public function verify() {
    if (!$this->isDir()) {
      throw new Exception(__CLASS__ . ' requires a directory argument, but "' . $this->getPathname() . '" was provided.', E_RECOVERABLE_ERROR);
    }

    if (!is_dir($this . DIRECTORY_SEPARATOR . '.svn')) {
      throw new Exception($this . " contains no svn metadata; it is not a working copy directory.", E_RECOVERABLE_ERROR);
    }
  }

  public function getWorkingPath() {
    return $this->getRootPath();
  }

  public function getPrependPath() {
    return;
  }

  public function svn($subcommand, $defaults = NULL) {
    $classname = 'svn' . $subcommand;
    if (!class_exists($classname)) {
      throw new Exception("Invalid svn subcommand '$subcommand' was requested.", E_RECOVERABLE_ERROR);
    }
    $this->cmd = new $classname($this, is_null($defaults) ? $this->defaults : $defaults);

    // Add any global working copy opts that are set.
    foreach (array('username', 'password', 'configDir') as $prop) {
      if (!empty($this->$prop)) {
        $this->cmd->$prop($this->$prop);
      }
    }

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

//  public static $protocols = array(
//    'http' => array(
//      'write capable' => FALSE,
//    ),
//    'https' => array(
//      'write capable' => FALSE,
//    ),
//    'svn' => array(
//      'write capable' => FALSE,
//    ),
//    'svn+ssh' => array(
//      'write capable' => TRUE,
//    ),
//    'file' => array(
//      'write capable' => TRUE,
//    ),
//  );

  public static $protocols = array(
    'http' => 0,
    'https' => 0,
    'svn' => 0,
    'svn+ssh' => 1,
    'file' => 3,
  );

  protected $protocol, $path;

  public function verify() {
    // Need to explode out the URL into its respective parts, first
    list($this->protocol, $this->path) = preg_split('@://@', (string) $this, 2);

    // Run a fast, low-overhead operation, verifying this is a working svn repository.
    if (self::$protocols[$this->protocol] & self::CAN_SVNADMIN) {
      system('svnadmin lstxns ' . escapeshellarg($this->path), $exit);
    }
    else {
      system('svn info --config-dir ' . $this->configDir . ' ' . (string) $this);
    }
    if ($exit) {
      throw new Exception($this->getPathname() . " is not a valid Subversion repository.", E_RECOVERABLE_ERROR);
    }
  }

  protected function getInfo() {
    parent::getInfo();
    $pieces = explode('://', (string) $this);
    $this->protocol = $pieces[0];
  }

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

  public function svn($subcommand, $defaults = NULL) {
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

    $this->cmd = new $classname($this, is_null($defaults) ? $this->defaults : $defaults);
    return $this->cmd;
  }

  /**
   * Indicate whether or not it is possible to perform write operations directly
   * on the repository.
   *
   * @return bool
   */
  public function isWritable() {
    return self::$protocols[$this->protocol] & self::WRITE_CAPABLE;
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
