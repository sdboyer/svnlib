<?php

// TODO temporary straight includes until a smarter system is introduced
require_once dirname(__FILE__) . '/lib.inc';
require_once dirname(__FILE__) . '/parsers.inc';
require_once dirname(__FILE__) . '/commands/svn.commands.inc';
require_once dirname(__FILE__) . '/opts/svn.opts.inc';

/**
 * Abstract class that allows for commands that can be used on both an svn repo
 * and working copy to be handled via inheritance.
 * @author sdboyer
 *
 */
abstract class SvnInstance extends SplFileInfo implements CLI {
  protected $defaults = TRUE;
  protected $cmd;
  protected $cmdSwitches = 0, $cmdOpts = array();
  protected $cache = array();
  public $invocations, $cmdContainer, $retContainer, $errContainer;
  protected $subPath = '';

  public function __construct($path, $verify = TRUE) {
    parent::__construct($path);
    if ($verify) {
      $this->verify();
    }
    $this->retContainer = new SplObjectMap();
    $this->errContainer = new SplObjectMap();
  }

  public function defaults($use = TRUE) {
    $this->defaults = $use;
    return $this;
  }

  /**
   * Set a path, relative to the base path that was passed in to the SvnInstance
   * constructor, that should be used as the base path for all path-based
   * operations. Primarily useful for specifying a particular branch or tag that
   * operations should be run against in a fashion that will be transparent to
   * the subcommand invocations.
   *
   * IMPORTANT NOTE: internal handling of subpaths becomes copmlex if you change
   * the subpath while in the midst of queuing up a command. This internal
   * behavior is also different for repositories than it is for working copies.
   *
   * @param string $path
   */
  public function setSubPath($path) {
    $this->subPath = $path;
  }

  public function verify() {
    if (!$this->isDir()) {
      throw new Exception(__CLASS__ . ' requires a directory argument, but "' . $this->getPath() . '" was provided.', E_RECOVERABLE_ERROR);
    }
  }

  abstract protected function getInfo();

  public function getFullPath() {
    if (empty($this->subPath)) {
      return (string) $this;
    }
    else {
      return (string) $this . DIRECTORY_SEPARATOR . $this->subPath;
    }
  }

  abstract public function getPrependPath();
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
  public $username, $password, $configDir;

  const NO_AUTH_CACHE   = 0x001;

  protected function getInfo() {
    $orig_subpath = $this->subPath;
    $this->subPath = NULL;

    $info = new SvnInfo($this, FALSE);
    $output = $info->target('.')->configDir(dirname(__FILE__) . '/configdir')->execute();

    preg_match('/^Repository Root: (.*)\n/m', $output, $root);
    $this->repoRoot = $root[1];
    preg_match('/^Revision: (.*)\n/m', $output, $rev);
    $this->latestRev = (int) $rev[1];

    $this->subPath = $orig_subpath;
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
    return NULL;
  }

  public function verify() {
    parent::verify();
    if (!is_dir($this . '/.svn')) {
      throw new Exception($this . " contains no svn metadata; it is not a working copy directory.", E_RECOVERABLE_ERROR);
    }
  }

  public function getWorkingPath() {
    return $this->getFullPath();
  }

  public function getPrependPath() {
    return NULL;
  }

  public function svn($subcommand, $defaults = NULL) {
    $fullcommand = 'svn' . $subcommand;
    if (!class_exists($fullcommand)) {
      throw new Exception("Invalid svn subcommand '$subcommand' was requested.", E_RECOVERABLE_ERROR);
      return;
    }
    $this->cmd = new $fullcommand($this, is_null($defaults) ? $this->defaults : $defaults);

    // Add any global working copy opts that are set.
    foreach (array('username', 'password', 'configDir') as $prop) {
      if (!empty($this->$prop)) {
        $this->cmd->$prop($this->$prop);
      }
    }

    return $this->cmd;
  }
}

class SvnRepository extends SvnInstance {

  public function verify() {
    // Run a fast, low-overhead operation, verifying this is a working svn repository.
    system('svnadmin lstxns ' . escapeshellarg($this->getPath()), $exit);
    if ($exit) {
      throw new Exception($this->getPath() . " is not a valid Subversion repository.", E_RECOVERABLE_ERROR);
    }
  }

  protected function getInfo() {

  }

  public function getPrependPath() {
    return $this->getFullPath() . DIRECTORY_SEPARATOR;
  }

  public function getWorkingPath() {
    return NULL;
  }
}
