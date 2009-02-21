<?php

// TODO temporary straight includes until a smarter system is introduced
require_once './lib.inc';
require_once './parsers.inc';
require_once './commands/svn.commands.inc';
require_once './opts/svn.opts.inc';

/*interface CLI {
  const IS_SWITCH = 0x0001;
  public function getVersion();
}*/

interface CLICommand {
  public function prepare();
  public function execute();
}

interface CLICommandOpt {
  public function getShellString();
}

/**
 * Abstract class that allows for commands that can be used on both an svn repo
 * and working copy to be handled via inheritance.
 * @author sdboyer
 *
 */
abstract class SvnInstance extends SplFileInfo {
  protected $defaults = TRUE;
  protected $path;
  protected $cmd;
  public $invocations, $cmdContainer, $retContainer, $errContainer;

  public function __construct($path, $verify = TRUE) {
    parent::__construct($path);
    $this->path = $path;
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

  abstract public function verify();
}

/**
 * Class for managing the root of an Subversion working copy.
 *
 * Once created, it can spawn various invocations of the svn command-line
 * in order to gather information about the state of the working copy.
 *
 * @author sdboyer
 *
 */
class SvnWorkingCopy extends SvnInstance {

  public function verify() {
    if (!is_dir("$this->path/.svn")) {
      throw new Exception("$this->path is not an svn working copy directory, as it contains no svn metadata.", E_RECOVERABLE_ERROR);
    }
  }

  public function svnInfo($defaults = NULL) {
    $this->cmd = new SvnInfo($this, is_null($defaults) ? $this->defaults : $defaults);
  }

  public function svnLog($defaults = NULL) {
    $this->cmd = new SvnLog($this, is_null($defaults) ? $this->defaults : $defaults);
  }
}

class SvnRepository extends SvnInstance {

  public function verify() {
    // Run a fast, low-overhead operation, verifying this is a working svn repository.
    system('svnadmin lstxns ' . escapeshellarg($path), $exit);
    if ($exit) {
      throw new Exception("$path is not a valid Subversion repository.", E_RECOVERABLE_ERROR);
    }
  }
}
