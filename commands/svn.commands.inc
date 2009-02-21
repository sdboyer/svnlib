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
  const VERBOSE           = 0x0001;
  const INCREMENTAL       = 0x0002;
  const XML               = 0x0004;
  const FORCE             = 0x0008;
  const DRY_RUN           = 0x0010;
  const STOP_ON_COPY      = 0x0020;
  const USE_MERGE_HISTORY = 0x0040;
  const REVPROP           = 0x0080;
  const QUIET             = 0x0100;
  const PARENTS           = 0x0200;
  const NO_IGNORE         = 0x0400;
  const USE_ANCESTRY      = 0x0800; // represents two switches
  const IGNORE_EXTERNALS  = 0x1000;
  const AUTO_PROPS        = 0x2000;
  const NO_AUTH_CACHE     = 0x4000;
  const NON_INTERACTIVE   = 0x8000;

/*  protected $optInfo = array(
    self::ACCEPT => array(
      'shell string' => '--accept',
      'version' => 1.5,
      'allowed args' => array('postpone', 'base', 'mine-full', 'theirs-full', 'edit', 'launch'),
      // 'concatenator' => '=',
    ),
    self::DEPTH => array(
      'shell string' => '--depth',
      'version' => 1.5,
      'allowed args' => array('empty', 'files', 'immediates', 'infinity'),
    ),
    self::ENCODING => array(
      'shell string' => '--encoding',
    // 'version' => 1.4,
    ),
    self::FILE => array(
      'shell string' => '-F',
    ),
    self::LIMIT => array(
      'shell string' => '-l'
    ),
    self::MESSAGE => array(
      'shell string' => '-m'
    ),
    self::REVISION => array(
      'shell string' => '-r'
    ),
    self::TARGETS => array(
      'shell string' => '--targets'
    ),
    self::WITH_REVPROP => array(
      'shell string' => '--with-revprop'
    ),
  );*/

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
  protected $wc;

  protected $procPipes = array(), $process = 0;

  protected $procDescriptor = array(
      1 => array('pipe', 'w'),
      2 => array('pipe', 'w'),
      // 2 => array("file", "/tmp/$num-error-output.txt", "a"),
    );

  /**
   * Used to spawn the the parsing class object, if/as needed.
   *
   * @var ReflectionClass
   */
  protected $parser;
  public $internalSwitches = 0;
  protected $cmdSwitches = 0, $cmdOpts = array();

  public function __construct(SvnInstance $wc, $defaults = TRUE) {
    $this->wc = &$wc;
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
      throw new Exception('Nonexistent parser class provided.', E_ERROR);
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
   * Execute the
   * @see CLICommand::execute()
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
    if ($err = stream_get_contents($this->procPipes[2])) {
      throw new Exception('svn failed with the following message: ' . $err, E_RECOVERABLE_ERROR);
      return;
    }

    $this->wc->retContainer[$this] = ($this->internalSwitches & self::PARSE_OUTPUT) ? $this->parser->newInstance(stream_get_contents($this->procPipes[1])) : stream_get_contents($this->procPipes[1]);
    $this->procClose();

    if ($fluent) {
      return $this;
    }
    return $this->wc->retContainer[$this];
  }

  /**
   * Wrapper for proc_open() that ensures any existing processes have already
   * been cleaned up.
   *
   * @return void
   */
  protected function procOpen() {
    if (is_resource($this->process)) {
      $this->procClose();
    }
    $this->process = proc_open(implode(' ', $this->cmds), $this->procDescriptor, $this->procPipes, (string) $this->wc, NULL);
  }

  /**
   * Wrapper for proc_close() that cleans up the currently running process.
   * @return void
   */
  protected function procClose() {
    foreach ($this->procPipes as $pipe) {
      fclose($pipe);
    }
    $this->procPipes = array();
    $this->process = proc_close($this->process);
  }

  /**
   * Gets the version number for the svn binary that will be called by
   * SvnCommand::procOpen.
   * @return SvnCommand
   */
  public function getVersion() {

  }

  public function toggleSwitch($bits) {
    $this->cmdSwitches ^= $bits;
    return $this;
  }

  public function targets($path) {
    $this->cmdOpts[self::TARGETS] = new SvnOptTargets($path);
    return $this;
  }

/*  public function recursive($arg = TRUE) {
    if ($arg) {
      $this->cmdOpts[self::DEPTH] = 'infinity';
    }
    else {
      $this->cmdOpts[self::DEPTH] = 'empty';
    }
    return $this;
  }*/

  public function verbose() {
    $this->switchChange($arg, self::VERBOSE);
    return $this;
  }

  public function quiet() {
    $this->switchChange($arg, self::QUIET);
    return $this;
  }

  protected function switchChange($arg, $switch) {
    if ($arg) {
      $this->cmdSwitches |= $switch;
    }
    else {
      $this->cmdSwitches ^= ($this->cmdSwitches & $switch) ? $switch : 0;
    }
  }

  /**
   * Toggle the `--xml` switch on or off.
   * @return SvnCommand
   */
  public function xml($arg = TRUE) {
    $this->switchChange($arg, self::XML);
    return $this;
  }

  /**
   * Toggle the `--incremental` switch on or off.
   * @return SvnCommand
   */
  public function incremental($arg = TRUE) {
    $this->switchChange($arg, self::INCREMENTAL);
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
    ksort($opts);
    $this->cmds = array_merge($this->cmds, $opts);

    array_unshift($this->cmds, 'svn', $this->command);
    return $fluent ? $this : $this->cmds;
  }

  /**
   * Helper function for SvnCommand::prepare().
   *
   * @param $opts
   * @param $arg
   * @return void
   * TODO this is where the real, really legit and really interesting use for SplObjectMap is.
   */
  protected function processOpts(&$opts, $arg) {
    if (is_array($arg)) {
      foreach ($arg as $opt => $obj) {
        $this->processOpts($opts, $obj);
      }
    }
    else {
      $opts[$arg->getOrdinal()] = $arg->getShellString();
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
  public $parserClass = 'SvnInfoParser';
  // protected $target = array();

  public function revision($rev1, $rev2 = NULL) {
    if (!is_null($arg2)) {
      throw new Exception('`svn info` can take only a single revision argument, not a revision range. The second argument will be ignored.', E_WARNING);
    }
    $this->cmdOpts[self::REVISION] = new SvnOptRevision($this, $arg1);
    return $this;
  }
}

class SvnLog extends SvnRead {
  const WITH_ALL_REVPROPS = 0x10000;
  protected $command = 'log';
  public $parserClass = 'SvnInfoParser';

  public function revision($arg1, $arg) {

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
