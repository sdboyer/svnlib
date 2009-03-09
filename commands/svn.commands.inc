<?php

/**
 * @author sdboyer
 *
 */
abstract class SvnCommand implements CLICommand {

  // internal state switches
  const PREPARED      = 0x001;
  const CACHE_OUTPUT  = 0x002;

  // opts
  const USERNAME      = 1;
  const PASSWORD      = 2;
  const CONFIG_DIR    = 3;
  const ACCEPT        = 4;
  const DEPTH         = 5;
  const ENCODING      = 6;
  const FILE          = 7;
  const LIMIT         = 8;
  const MESSAGE       = 9;
  const REVISION      = 10;
  const TARGETS       = 11;
  const WITH_REVPROP  = 12;
  const TARGET        = 13;

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

  protected $switchInfo = array();
  public $retContainer, $cmds = array();

  /**
   *
   * @var SvnInstance
   */
  protected $svnRoot;

  protected $procPipes = array();

  protected $process = 0;

  public $internalSwitches = 0;
  protected $cmdSwitches = 0;
  protected $cmdOpts = array();

  public function __construct(SvnInstance $svnRoot, $defaults = TRUE) {
    $this->svnRoot = &$svnRoot;
    
    // Set up the cmdSwitches array for use.
    $this->setSwitches();

    if ($defaults) {
      $this->setDefaults();
    }

  }

  protected function setSwitches() {
    $this->cmdSwitches = array(
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
  }

  /**
   * Set some sane defaults that apply for most invocations of the svn binary.
   *
   * @return SvnCommand
   */
  public function setDefaults() {
    $this->cmdSwitches |= self::XML | self::NON_INTERACTIVE;
    // @todo configdir needs to be done more intelligently
    $this->configDir(dirname(__FILE__) . '/../configdir');
    return $this;
  }

  /**
   * Wrapper for proc_open() that ensures any existing processes have already
   * been cleaned up.
   *
   * @return void
   */
  protected function procOpen() {
    $this->procClose();
    $this->process = proc_open(implode(' ', $this->cmds), $this->procDescriptor, $this->procPipes, $this->svnRoot->getWorkingPath(), NULL);
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
      $this->cmdOpts[self::DEPTH] = new SvnOptDepth($this, $arg);
    }
    else {
      $this->cmdOpts[self::DEPTH]->changeArg($arg);
    }
    return $this;
  }

  public function targets($path) {
    $this->internalTargets()->setTargetsFile($path);
    return $this;
  }

  /**
   * Helper method to lazy-load the complex targets opt as needed.
   * @return SvnOptTargets
   */
  protected function internalTargets() {
    if (empty($this->cmdOpts[self::TARGETS])) {
      $this->cmdOpts[self::TARGETS] = new SvnOptTargets($this);
    }
    return $this->cmdOpts[self::TARGETS];
  }

  public function username($name) {
    $this->cmdOpts[self::USERNAME] = new SvnOptUsername($this, $name);
    return $this;
  }

  public function password($pass) {
    $this->cmdOpts[self::PASSWORD] = new SvnOptPassword($this, $pass);
    return $this;
  }

  public function configDir($dir) {
    $this->cmdOpts[self::CONFIG_DIR] = new SvnOptConfigDir($this, $dir);
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

  public function verbose($arg = TRUE) {
    $this->fiddleSwitch($arg, self::VERBOSE);
    return $this;
  }

  public function quiet($arg = TRUE) {
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

    foreach ($this->switchInfo as $switch => $info) {
      if ($this->cmdSwitches & $switch) {
        $this->prepSwitch($switch, $info);
      }
    }
    ksort($this->cmds);

    $opts = array();
    $this->processOpts($opts, $this->cmdOpts);
    asort($opts, SORT_NUMERIC);
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
      foreach ($arg as $obj) {
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
    // TODO consider killing this entirely; really, it's babysitting broken code
    if (!empty($info['requires'])) {
      foreach ($info['requires'] as $req_switch) {
        if (!$this->cmdSwitches & $req_switch && empty($this->cmds[$req_switch])) {
          $this->prepSwitch($req_switch, $this->switchInfo[$req_switch]);
        }
      }
    }
  }
  
  /**
   *
   * @param string $target
   *    The target item (file or directory), relative to the instance root.
   * @param mixed $peg_rev
   *    Optional. The desired peg revision for the target item. See the svn
   *    manual for an explanation of the difference between operative revisions
   *    and peg revisions.
   * @param boolean $aggregate
   *    Optional. If TRUE, an internal optimization will be used whereby all of
   *    the target arguments that are passed are grouped into a single --targets
   *    file. This means a little more overhead initially, but scales far better
   *    than creating individual object SvnOptTarget instances for each target
   *    item.
   *
   *    Defaults to FALSE. Set this to TRUE if the invocation will have anything
   *    more than three or four targets.
   *    @see SvnOptTargets
   *
   * @return SvnRead
   */
  public function target($target, $peg_rev = NULL, $aggregate = FALSE) {
    if ($aggregate) {
      $this->internalTargets()->addTarget($target, $peg_rev);
    }
    else {
      $target = new SvnOptTarget($this, $target);
      if (!is_null($peg_rev)) {
        $target->revision($peg_rev);
      }
      $this->cmdOpts[self::TARGET][] = $target;
    }
    return $this;
  }

  public function __destruct() {
    $this->procClose();
  }
}


abstract class SvnWrite extends SvnCommand {
  protected $procDescriptor = array(
      2 => array('pipe', 'w'),
    );
    
  public function dryRun($arg = TRUE) {
    $this->fiddleSwitch($arg, self::DRY_RUN);
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
      throw new Exception('svn failed with the following message: ' . $this->svnRoot->errContainer[$this], E_RECOVERABLE_ERROR);
      return;
    }

    $this->procClose();

    if ($fluent) {
      return $this;
    }
  }
}

abstract class SvnRead extends SvnCommand {

  // internal switches
  const PARSE_OUTPUT  = 0x004;

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
  
  /**
   * Adds an operative revision to the currently queuing command. Note that
   * operative revisions are somewhat less intuitive than peg revisions. If you
   * have any doubts about whether to use peg or operative revisions, you should
   * either read the subversion manual for clarification of the differences, or
   * simply use peg revisions probably the safer bet.
   *
   * @param mixed $op_rev1
   * @param mixed $op_rev2
   * @return SvnCommand
   */
  public function revision($op_rev1, $op_rev2 = NULL) {
    $this->cmdOpts[self::REVISION] = new SvnOptRevision($this, $op_rev1);
    if (!is_null($op_rev2)) {
      $this->cmdOpts[self::REVISION]->range($op_rev2);
    }
    return $this;
  }

  public function setDefaults() {
    parent::setDefaults();
    $this->internalSwitches |= self::PARSE_OUTPUT;
    if (isset($this->parserClass)) {
      $this->setParserClass($this->parserClass);
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
      // will always output 'SvnRead'. Keeping it in anyway, in anticipation.
      throw new Exception("Undeclared class '$class' provided to " . __CLASS__ . "::setParserClass.", E_RECOVERABLE_ERROR);
    }
    $this->parser = new ReflectionClass($class);
    return $this;
  }

  /**
   * Execute the command according to dimensions of the object's internal state.
   *
   * Prepares (if necessary) all the various dimensions of the cli invocation's
   * state, then fires up a process and gets into output and/or error handling.
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

    $ret = stream_get_contents($this->procPipes[1]);

    if ($this->svnRoot->errContainer[$this] = stream_get_contents($this->procPipes[2])) {
      throw new Exception('svn failed with the following message: ' . $this->svnRoot->errContainer[$this], E_RECOVERABLE_ERROR);
      return;
    }

    $this->svnRoot->retContainer[$this] =
      ($this->internalSwitches & self::PARSE_OUTPUT) ? $this->parser->newInstance($ret) : $ret;


    $this->procClose();

    if ($fluent) {
      return $this;
    }
    return $this->svnRoot->retContainer[$this];
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
  public $parserClass = 'SvnLogParser';

  public function setSwitches() {
    parent::setSwitches();
    $this->cmdSwitches[self::WITH_ALL_REVPROPS] = array(
      'shell string' => '--with-all-revprops',
    );
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
  const SHOW_UPDATES = 0x20000;
  
  protected $command = 'status';

  public function setSwitches() {
    parent::setSwitches();
    $this->cmdSwitches[self::SHOW_UPDATES] = array(
      'shell string' => '--show-updates',
    );
  }
}

class SvnMerge extends SvnWrite {
  const REINTEGRATE = 0x20000;
  const RECORD_ONLY = 0x40000;

  protected $command = 'merge';

  public function setSwitches() {
    parent::setSwitches();
    $this->cmdSwitches[self::REINTEGRATE] = array(
      'shell string' => '--reintegrate',
    );
    $this->cmdSwitches[self::RECORD_ONLY] = array(
      'shell string' => '--record-only',
    );
  }
}

class SvnPropGet extends SvnRead {
  const STRICT = 0x20000;

  protected $command = 'propget';

  public function setSwitches() {
    parent::setSwitches();
    $this->cmdSwitches[self::STRICT] = array(
      'shell string' => '--strict',
    );
  }
}

class SvnCommit extends SvnWrite {
  const NO_UNLOCK = 0x20000;

  protected $command = 'commit';

  public function setSwitches() {
    parent::setSwitches();
    $this->cmdSwitches[self::NO_UNLOCK] = array(
      'shell string' => '--no-unlock',
    );
  }
}

class SvnDelete extends SvnWrite {
  const KEEP_LOCAL = 0x20000;
  protected $command = 'delete';

  public function setSwitches() {
    parent::setSwitches();
    $this->cmdSwitches[self::KEEP_LOCAL] = array(
      'shell string' => '--keep-local',
    );
  }
}

class SvnAdd extends SvnWrite {
  protected $command = 'add';
}

class SvnBlame extends SvnRead {
  protected $command = 'blame';
}

class SvnCat extends SvnRead {
  protected $command = 'cat';
}

class SvnChangelist extends SvnWrite {
  protected $command = 'changelist';
}

class SvnCheckout extends SvnWrite {
  protected $command = 'checkout';
}

class SvnCleanup extends SvnWrite {
  protected $command = 'cleanup';
}

class SvnCopy extends SvnWrite {
  protected $command = 'copy';
}

class SvnDiff extends SvnRead {
  protected $command = 'diff';
}

class SvnExport extends SvnWrite {
  protected $command = 'export';
}

class SvnImport extends SvnWrite {
  protected $command = 'import';
}

class SvnLock extends SvnRead {
  protected $command = 'lock';
}

class SvnMergeinfo extends SvnRead {
  protected $command = 'mergeinfo';
}

class SvnMkdir extends SvnWrite {
  protected $command = 'mkdir';
}

class SvnMove extends SvnWrite {
  protected $command = 'move';
}

class SvnPropdel extends SvnWrite {
  protected $command = 'propdel';
}

class SvnPropedit extends SvnWrite {
  protected $command = 'propedit';
}

class SvnPropget extends SvnRead {
  protected $command = 'propget';
}

class SvnProplist extends SvnRead {
  protected $command = 'proplist';
}

class SvnPropset extends SvnWrite {
  protected $command = 'propset';
}

class SvnResolve extends SvnWrite {
  protected $command = 'resolve';
}

class SvnResolved extends SvnWrite {
  protected $command = 'resolved';
}

class SvnRevert extends SvnWrite {
  protected $command = 'revert';
}

class SvnSwitch extends SvnWrite {
  protected $command = 'switch';
}

class SvnUnlock extends SvnWrite {
  protected $command = 'unlock';
}

class SvnUpdate extends SvnWrite {
  protected $command = 'update';
}
