<?php

/**
 * TODO Add destructor that kills any open processes
 *
 * @author sdboyer
 *
 */
abstract class SvnCommand implements CLICommand {

  // internal state switches
  const PARSE_OUTPUT  = 0x001;
  const PREPARED      = 0x002;
  const CACHE_OUTPUT  = 0x004;

  // opts
  const AUTH          = 1;
  const CONFIG_DIR    = 2;
  const ACCEPT        = 3;
  // const CHANGE = 3; // use revision
  // const CHANGELIST = 4;
  const DEPTH         = 4;
  const ENCODING      = 5;
  const FILE          = 6;
  const LIMIT         = 7;
  const MESSAGE       = 8;
  // const DIFF_NEW = 15;
  // const DIFF_OLD = 21;
  const REVISION      = 9;
  const TARGETS       = 10;
  const WITH_REVPROP  = 11;
  const TARGET        = 12;

  // cli switches
  const VERBOSE           = 0x00001;
  const INCREMENTAL       = 0x00002;
  const XML               = 0x00004;
  const FORCE             = 0x00008;
  const FORCE_LOG         = 0x00010;
  const DRY_RUN           = 0x00020;
  const STOP_ON_COPY      = 0x00040;
  const USE_MERGE_HISTORY = 0x00080;
  const REVPROP           = 0x00100;
  const QUIET             = 0x00200;
  const PARENTS           = 0x00400;
  const NO_IGNORE         = 0x00800;
  const USE_ANCESTRY      = 0x01000; // represents two switches
  const IGNORE_EXTERNALS  = 0x02000;
  const AUTO_PROPS        = 0x04000;
  const NO_AUTH_CACHE     = 0x08000;
  const NON_INTERACTIVE   = 0x10000;

  protected $switchInfo = array(
    self::VERBOSE => array(
      'shell string' => '-v',
    ),
    self::INCREMENTAL => array(
      'shell string' => '--incremental',
      'requires' => array(self::XML),
    ),
    self::XML => array(
      'shell string' => '--xml',
    ),
    self::FORCE => array(
      'shell string' => '--force',
    ),
    self::FORCE_LOG => array(
      'shell string' => '--force-log',
    ),
    self::DRY_RUN => array(
      'shell string' => '--dry-run',
    ),
    self::STOP_ON_COPY => array(
      'shell string' => '--stop-on-copy',
    ),
    self::USE_MERGE_HISTORY => array(
      'shell string' => '-g',
    ),
    self::REVPROP => array(
      'shell string' => '--revprop',
      'requires' => array(self::REVISION),
    ),
    self::QUIET => array(
      'shell string' => '-q',
    ),
    self::PARENTS => array(
      'shell string' => '--parents',
    ),
    self::NO_IGNORE => array(
      'shell string' => '--no-ignore',
    ),
    self::USE_ANCESTRY => array(
      'shell string' => '--stop-on-copy',
    ),
    self::IGNORE_EXTERNALS => array(
      'shell string' => '--ignore-externals',
    ),
    self::AUTO_PROPS => array(
      'shell string' => '--auto-props',
    ),
    self::NO_AUTH_CACHE => array(
      'shell string' => '--no-auth-cache',
    ),
    self::NON_INTERACTIVE => array(
      'shell string' => '--non-interactive',
    ),
  );
  public $retContainer, $cmds = array();

  /**
   *
   * @var SvnWorkingCopy
   */
  protected $svnRoot;

  protected $procPipes = array(), $process = 0;

  protected $procDescriptor = array(
      1 => array('pipe', 'w'),
      2 => array('pipe', 'w'),
    );

  /**
   * Used to spawn the the parsing class object, if/as needed.
   *
   * @var ReflectionClass
   */
  protected $parser;
  public $internalSwitches = 0;
  protected $cmdSwitches = 0, $cmdOpts = array();

  public function __construct(SvnInstance $svnRoot, $defaults = TRUE) {
    $this->svnRoot = &$svnRoot;
    // $this->retContainer = new SplObjectMap();

    if ($defaults) {
      $this->setDefaults();
    }
  }

  /**
   * If set to provide output parsing, set the workhorse class that will do the
   * parsing.
   *
   * @param string $class
   * @return SvnCommand
   */
  public function setParserClass($class) {
    if (!class_exists($class)) {
      // Until we have late static binding (PHP 5.3), __CLASS__ used in this way
      // will always output 'SvnCommand'. Keeping it in anyway, in anticipation.
      throw new Exception("Undeclared class '$class' provided to " . __CLASS__ . "::setParserClass.", E_RECOVERABLE_ERROR);
    }
    else if ($this->internalSwitches & self::PARSE_OUTPUT) {
      $this->parser = new ReflectionClass($class);
    }
    return $this;
  }

  /**
   * Set some sane defaults that apply for most invocations of the svn binary.
   *
   * @return SvnCommand
   */
  public function setDefaults() {
    $this->internalSwitches |= self::PARSE_OUTPUT;
    $this->cmdSwitches |= self::XML | self::NON_INTERACTIVE;
    if (isset($this->parserClass)) {
      $this->setParserClass($this->parserClass);
    }
    return $this;
  }

  /**
   * Execute the command according to dimensions of the object's internal state.
   *
   * Prepares (if necessary) all the various dimensions of the cli invocation's
   * state, then fires up a process and gets into output and/or error handling.
   *
   * @see CLICommand::execute()
   *
   * @param bool $fluent
   *  Indicates whether or not this method should behave fluently (should return
   *  $this instead of the possibly parsed return value). Defaults to FALSE.
   * @return mixed
   */
  public function execute($fluent = FALSE) {
    if (!($this->internalSwitches & self::PREPARED)) {
      $this->prepare(FALSE);
    }

    $this->procOpen();
    if ($this->svnRoot->errContainer[$this] = stream_get_contents($this->procPipes[2])) {
      throw new Exception('svn failed with the following message: ' . $this->svnRoot->errContainer[$this], E_WARNING);
      return;
    }

    $this->svnRoot->retContainer[$this] = ($this->internalSwitches & self::PARSE_OUTPUT) ?
      $this->parser->newInstance(stream_get_contents($this->procPipes[1])) :
      stream_get_contents($this->procPipes[1]);
    $this->procClose();

    if ($fluent) {
      return $this;
    }
    return $this->svnRoot->retContainer[$this];
  }

  /**
   * Wrapper for proc_open() that ensures any existing processes have already
   * been cleaned up.
   *
   * @return void
   */
  protected function procOpen() {
    $this->procClose();
    $this->process = proc_open(implode(' ', $this->cmds), $this->procDescriptor, $this->procPipes, (string) $this->svnRoot, NULL);
  }

  /**
   * Wrapper for proc_close() that cleans up the currently running process.
   * @return void
   */
  protected function procClose() {
    if (is_resource($this->process)) {
      foreach ($this->procPipes as $pipe) {
        fclose($pipe);
      }
      $this->procPipes = array();
      $this->process = proc_close($this->process);
    }
  }

  /**
   * Gets the version number for the svn binary that will be called by
   * SvnCommand::procOpen.
   * @return SvnCommand
   */
  public function getVersion() {
    return system('svn -q --version');
  }

  public function depth($arg) {
    if (!isset($this->cmdOpts[self::DEPTH])) {
      $this->cmdOpts[self::DEPTH] = new SvnOptDepth($arg);
    }
    else {
      $this->cmdOpts[self::DEPTH]->changeArg($arg);
    }
    return $this;
  }

  public function targets($path) {
    $this->cmdOpts[self::TARGETS] = new SvnOptTargets($path);
    return $this;
  }

  public function recursive() {
    return $this->depth('infinity');
  }

  public function nonRecursive() {
    return $this->depth('none');
  }

  protected function fiddleSwitch($arg, $switch) {
    if ($arg) {
      $this->cmdSwitches |= $switch;
    }
    else {
      $this->cmdSwitches ^= ($this->cmdSwitches & $switch) ? $switch : 0;
    }
  }

  public function toggleSwitches($bits) {
    $this->cmdSwitches ^= $bits;
    return $this;
  }

  public function verbose() {
    $this->fiddleSwitch($arg, self::VERBOSE);
    return $this;
  }

  public function quiet() {
    $this->fiddleSwitch($arg, self::QUIET);
    return $this;
  }

  /**
   * Toggle the `--xml` switch on or off.
   * @return SvnCommand
   */
  public function xml($arg = TRUE) {
    $this->fiddleSwitch($arg, self::XML);
    return $this;
  }

  /**
   * Toggle the `--incremental` switch on or off.
   * @return SvnCommand
   */
  public function incremental($arg = TRUE) {
    $this->fiddleSwitch($arg, self::INCREMENTAL);
    return $this;
  }

  public function force($arg = TRUE) {
    $this->fiddleSwitch($arg, self::FORCE);
    return $this;
  }

  public function forceLog($arg = TRUE) {
    $this->fiddleSwitch($arg, self::DRY_RUN);
    return $this;
  }

  public function noIgnore($arg = TRUE) {
    $this->fiddleSwitch($arg, self::NO_IGNORE);
    return $this;
  }

  public function autoProps($arg = TRUE) {
    $this->fiddleSwitch($arg, self::AUTO_PROPS);
    return $this;
  }

  public function parents($arg = TRUE) {
    $this->fiddleSwitch($arg, self::PARENTS);
    return $this;
  }

  /**
   * Prepares the assembled data in the current class for execution by
   * SvnCommand::execute().
   *
   * Note that this function is public such that it can be called separately in
   * order to allow client code to muck about with the cmds array that will be
   * used by SvnCommand::execute().
   * @param bool $fluent
   * @return mixed
   */
  public function prepare($fluent = TRUE) {
    $this->internalSwitches |= self::PREPARED;
    $this->cmds = array();

    if (method_exists($this, 'resolveParamConflicts')) {
      $this->resolveParamConflicts();
    }

    foreach ($this->switchInfo as $switch => $info) {
      if ($this->cmdSwitches & $switch) {
        $this->prepSwitch($switch, $info);
      }
    }
    ksort($this->cmds);

    $opts = array();
    $this->processOpts($opts, $this->cmdOpts);
    asort($opts, SORT_NUMERIC);
    // for (reset($opts); list(, $string) = each($opts); $this->cmds[] = $string);
    $this->cmds = array_merge($this->cmds, array_keys($opts));
    if ($this->svnRoot instanceof SvnWorkingCopy) {
      $this->cmds = array_merge($this->svnRoot->prepare(), $this->cmds);
    }

    array_unshift($this->cmds, 'svn', $this->command);
    return $fluent ? $this : $this->cmds;
  }

  /**
   * Helper function for SvnCommand::prepare().
   *
   * @param $opts
   * @param $arg
   * @return void
   */
  protected function processOpts(&$opts, $arg) {
    if (is_array($arg)) {
      foreach ($arg as $opt => $obj) {
        $this->processOpts($opts, $obj);
      }
    }
    else {
      // TODO This will probably work well (may even have some elegant benefits
      // b/c it ought to effectively prevent duplicate shell params. HOWEVER, a
      // better, smarter system that leverages an ArrayObject + SplObjectStorage
      // structure is the eventual goal.
      $opts[$arg->getShellString()] = $arg->getOrdinal();
    }
  }

  /**
   * Helper function for SvnCommand::prepare().
   * @param $switch
   * @param $info
   * @return void
   */
  protected function prepSwitch($switch, $info) {
    $this->cmds[$switch] = $info['shell string'];
    if (!empty($info['requires'])) {
      foreach ($info['requires'] as $req_switch) {
        if (!$this->cmdSwitches & $req_switch && empty($this->cmds[$req_switch])) {
          $this->prepSwitch($req_switch, $this->switchInfo[$req_switch]);
        }
      }
    }
  }

  public function __destruct() {
    $this->procClose();
  }
}


abstract class SvnWrite extends SvnCommand {

  public function dryRun($arg = TRUE) {
    $this->fiddleSwitch($arg, self::DRY_RUN);
    return $this;
  }
}

abstract class SvnRead extends SvnCommand {

  public function revision($rev1, $rev2 = NULL) {
    $this->cmdOpts[self::REVISION] = new SvnOptRevision($this, $rev1);
    if (!is_null($rev2)) {
      $this->cmdOpts[self::REVISION]->range($rev2);
    }
    return $this;
  }

  public function target($target, $rev = NULL) {
/*    if (!$this->cmdOpts[self::TARGET] instanceof SplObjectMap) {
      $this->cmdOpts[self::TARGET] = new SplObjectMap();
    }*/

    $target = new SvnOptTarget($this, $target);
    if (!is_null($rev)) {
      $target->revision($rev);
    }
    $this->cmdOpts[self::TARGET][] = $target;
    return $this;
  }
}

/**
 * Class that handles invocation of `svn info`.
 * @author sdboyer
 *
 */
class SvnInfo extends SvnRead {
  protected $command = 'info';
  public $parserClass = 'SvnInfoXMLParser';
  // protected $target = array();

  public function revision($rev1, $rev2 = NULL) {
    if (!is_null($rev2)) {
      throw new Exception('`svn info` can take only a single revision argument, not a revision range. The second argument will be ignored.', E_WARNING);
    }
    $this->cmdOpts[self::REVISION] = new SvnOptRevision($this, $rev1);
    return $this;
  }
}

class SvnLog extends SvnRead {
  const WITH_ALL_REVPROPS = 0x20000;
  protected $command = 'log';
  public $parserClass = 'SvnLogXMLParser';

  public function revision($arg1, $arg2 = NULL) {

  }

  public function stopOnCopy() {
    $this->cmdSwitches ^= self::STOP_ON_COPY;
  }

}

class SvnList extends SvnRead {
  protected $command = 'list';
  public $parserClass = 'SvnListParser';
}

class SvnStatus extends SvnRead {
  protected $command = 'status';
  const SHOW_UPDATES = '';
}

class SvnMerge {
  const REINTEGRATE = '';
  const RECORD_ONLY = 24;
}

class SvnPropGet {
  const STRICT = '';
}

class SvnCommit {
  const NO_UNLOCK = 19;
}

class SvnDelete {
  public $x = array(
      self::KEEP_LOCAL => array(
      'shell string' => '--stop-on-copy',
    ),
  );

  const KEEP_LOCAL = 20;
}

function _svnlib_sort_opts($a, $b) {
  return $a->getOrdinal() - $b->getOrdinal();
}
