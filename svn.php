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
abstract class SvnInstance extends SplFileInfo {
  protected $defaults = TRUE;
  protected $cmd;
  protected $cmdSwitches = 0, $cmdOpts = array();
  protected $cache = array();
  public $invocations, $cmdContainer, $retContainer, $errContainer;

  public function __construct($path, $verify = TRUE) {
    parent::__construct($path);
    if ($verify) {
      $this->verify();
    }
    $this->retContainer = new SplObjectMap();
    $this->errContainer = new SplObjectMap();
    $this->cmdContainer = new SplObjectMap();
    $this->invocations = new SplObjectMap();
  }

  public function defaults($use = TRUE) {
    $this->defaults = $use;
    return $this;
  }

  public function verify() {
    if (!$this->isDir()) {
      throw new Exception(__CLASS__ . ' requires a directory argument, but "' . $this->getPath() . '" was provided.', E_RECOVERABLE_ERROR);
    }
  }

  protected abstract function getInfo();
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

  protected function getInfo() {
    $info = new SvnInfo($this, FALSE);
    $output = $info->target('.')->configDir(dirname(__FILE__) . '/configdir')->execute();
    preg_match('/^Repository Root: (.*)\n/m', $output, $root);
    $this->repoRoot = $root[1];
    preg_match('/^Revision: (.*)\n/m', $output, $rev);
    $this->latestRev = (int) $rev[1];
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

  public function prepare() {
    // FIXME This borders on klugey in comparison to the relatively elegant
    // systematicity of the rest of this library.
    $opts = array();
    foreach ($this->cmdOpts as $const => $opt) {
      $opts[] = $opt->getShellString();
    }
    if ($this->cmdSwitches & self::NO_AUTH_CACHE) {
      $opts[] = '--no-auth-cache';
    }
    return $opts;
  }

  /**
   *
   * @param bool $defaults
   * @return SvnInfo
   */
  public function svnInfo($defaults = NULL) {
    $this->cmd = new SvnInfo($this, is_null($defaults) ? $this->defaults : $defaults);
    return $this->cmd;
  }

  /**
   *
   * @param bool $defaults
   * @return SvnLog
   */
  public function svnLog($defaults = NULL) {
    $this->cmd = new SvnLog($this, is_null($defaults) ? $this->defaults : $defaults);
    return $this->cmd;
  }

  /**
   *
   * @param bool $defaults
   * @return SvnList 
   */
  public function svnList($defaults = NULL) {
    $this->cmd = new SvnList($this, is_null($defaults) ? $this->defaults : $defaults);
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
}
