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
  protected $cmd;
  protected $cmdSwitches = 0, $cmdOpts = array();
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
      throw new Exception('SvnInstance require a directory argument, but "' . $this->getPath() . '" was provided.', E_RECOVERABLE_ERROR);
    }
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

  const USERNAME    = 1;
  const PASSWORD    = 2;

  public function verify() {
    parent::verify();
    if (!is_dir($this . '/.svn')) {
      throw new Exception($this . " contains no svn metadata; it is not a working copy directory.", E_RECOVERABLE_ERROR);
    }
  }

  public function username($name) {
    $this->cmdOpts[self::USERNAME] = new SvnUsername($name);
    return $this;
  }

  public function password($pass) {
    $this->cmdOpts[self::PASSWORD] = new SvnPassword($pass);
    return $this;
  }

  public function prepare() {
    // FIXME This borders on klugey in comparison to the relative elegant
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

  public function svnInfo($defaults = NULL) {
    $this->cmd = new SvnInfo($this, is_null($defaults) ? $this->defaults : $defaults);
    return $this->cmd;
  }

  public function svnLog($defaults = NULL) {
    $this->cmd = new SvnLog($this, is_null($defaults) ? $this->defaults : $defaults);
    return $this->cmd;
  }

  public function svnList($defaults = NULL) {
    $this->cmd = new SvnList($this, is_null($defaults) ? $this->defaults : $defaults);
    return $this->cmd;
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

/**
 * Opt for handling `svn --username`.
 * @author sdboyer
 *
 * FIXME I'm a little uncomfortable about an inheritance hierarchy where this
 * has SvnOpt as its parent. Same goes for SvnPassword.
 */
class SvnUsername extends SvnOpt {
  protected $ordinal = 1;
  protected $opt = '--username';
}

class SvnPassword extends SvnOpt {
  protected $ordinal = 1;
  protected $opt = '--password';
}
