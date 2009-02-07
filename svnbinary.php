<?php

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
 * Class for managing the root of an Subversion working copy.
 *
 * Once created, it can spawn various invocations of the svn command-line
 * in order to gather information about the state of the working copy.
 *
 * @author sdboyer
 *
 */
class SvnWorkingCopy extends SplFileInfo {
  // const BIN_SVN     = 0x001; // only necessary if there are multiple binaries we might invoke on the working copy

  protected $cmd;
  public $invocations, $retContainer;

  public function __construct($path) {
    parent::__construct($path);
    if (!is_dir("$path/.svn")) {
      throw new Exception("$path is not an svn working copy directory, as it contains no svn metadata.", E_ERROR);
    }
    $this->retContainer = new SplObjectMap();
    $this->cmdContainer = new SplObjectMap();
    $this->invocations = new SplObjectMap();
  }

  /**
   * Total hack right now, made-up passthrough that just points straight to an
   * svn info
   *
   * FIXME probably gonna screw this whole approach and make methods for each
   * subcommand, b/c this blows
   */
  public function newInvocation($defaults = TRUE) {
    $this->cmd = new SvnInfo($this, $defaults);
    return $this->cmd;
  }
}

/*
class SvnlookCLI {
  const NO_AUTO_PROPS = 16;
  const NO_DIFF_DELETED = 17;
}
*/
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

  public function __construct(SvnWorkingCopy $wc, $defaults = TRUE) {
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
    $this->cmdSwitches |= self::XML;
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

/*  public function targetFile($path) {
    if (!is_file($path)) {
      throw new Exception("'$path' is not a file, but was passed to `svn info --targets`.", E_ERROR);
    }
    $this->cmdSwitches |= self::TARGETFILE;
    $this->args[self::TARGETFILE] = $path;
    return $this;
  }*/

/*  public function recursive($arg = TRUE) {
    if ($arg) {
      $this->args[self::DEPTH] = 'infinity';
    }
    else {
      $this->cmdOpts[self::DEPTH] = 'empty';
    }
    return $this;
  }*/

  /**
   * Toggle the `--xml` switch on or off.
   * @return SvnCommand
   */
  public function xml() {
    $this->cmdSwitches ^= self::XML;
    return $this;
  }

  /**
   * Toggle the `--incremental` switch on or off.
   * @return SvnCommand
   */
  public function incremental() {
    $this->cmdSwitches ^= self::INCREMENTAL;
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

/**
 * Parent class for opts that can be used by various Subversion `svn` subcommands.
 * @author sdboyer
 *
 */
abstract class SvnOpt implements CLICommandOpt {
  public function __construct(SvnCommand &$sc) {
    $this->sc = &$sc;
  }
}

class SvnOptRevision extends SvnOpt {
  protected $arg1 = '', $arg2 = '';

  public function __construct(SvnCommand &$sc, $rev) {
    parent::__construct($sc);
    if (self::checkArg($rev)) {
      $this->arg1 = $rev;
    }
  }

  public function range($rev) {
    if (self::checkArg($rev)) {
      $this->arg2 = $rev;
    }
    return $this;
  }

  public static function checkArg($arg) {
    if (!is_int($arg)) {
      // FIXME currently does not allow date-based revision range args
      if (!in_array($arg, array('HEAD', 'BASE', 'COMMITTED', 'PREV'))) {
        throw new Exception("Invalid revision information passed as an argument to SvnOptRevision", E_ERROR);
        // return FALSE;
      }
    }
    return TRUE;
  }

  public function getShellString() {
    $string = '-r ' . escapeshellarg($this->arg1);
    if (!empty($this->arg2)) {
      $string .= ':' . escapeshellarg($this->arg2);
    }
    return $string;
  }
}

class SvnOptAccept extends SvnOpt  {
  public function build() {

  }

  public function process() {

  }

  public function getShellString() {
    return '-r';
  }
}

class SvnOptDepth extends SvnOpt {

  public function __construct(SvnCommand $sc, $arg) {
    parent::__construct($sc, $arg);
    if (in_array($arg, array('infinity', 'files', 'immediates', 'empty'))) {
      $this->arg = $arg;
    }
  }

  public function process() {

  }

  public function getShellString() {
    return '--depth=' . $this->arg;
  }
}

class SvnOptTargets extends SvnOpt {

  public function process() {

  }

  public function getShellString() {

  }
}

class SvnOptTarget extends SvnOpt {
  protected $target = '', $rev = FALSE;

  public function __construct(SvnCommand $sc, $target) {
    parent::__construct($sc);
    $this->target = $target;
  }

  public function getOrdinal() {
    return SvnCommand::TARGET;
  }

  public function revision($rev) {
    if (!is_int($rev)) {
      throw new Exception('Non-integer revision argument, "' . $rev . '" passed to SvnOptTarget.', E_ERROR);
    }
    $this->rev = $rev;
    return $this;
  }

  public function getShellString() {
    $string = $this->rev === FALSE ? $this->target : $this->target . '@' . $this->rev;
    return $string;
  }
}

/**
 * Class that handles invocation of `svn info`.
 * @author sdboyer
 *
 */
class SvnInfo extends SvnCommand {

  protected $command = 'info';
  public $parserClass = 'SvnInfoParser';
  // protected $target = array();

  public function revision($arg1) {
/*    if (!is_null($arg2)) {
      throw new Exception('`svn info` can take only a single revision argument, not a revision range. The second argument will be ignored.', E_WARNING);
    }*/
    $this->args[self::REVISION] = new SvnOptRevision($this, $arg1);
    return $this;
  }

  public function target($target, $rev = NULL) {
    $target = new SvnOptTarget($this, $target);
    if (!is_null($rev)) {
      $target->revision($rev);
    }
    $this->cmdOpts[self::TARGET][] = $target;
    return $this;
  }
}

class SvnStatus {
  const SHOW_UPDATES = '';
}

class SvnLog {
  const WITH_ALL_REVPROPS = '';
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

/**
 * A class specifically tailored to parse the incremental xml output of an
 * invocation of `svn info`.
 *
 * @author sdboyer
 *
 */
class SvnInfoParser extends SimpleXMLIterator {

  public function current() {
    $entry = parent::current();
    $item = array();
    $item['url'] = (string) $entry->url;
    $item['repository_root'] = (string) $entry->repository->root;
    $item['repository_uuid'] = (string) $entry->repository->uuid;

    if ($item['url'] == $item['repository_root']) {
      $item['path'] = '/';
    }
    else {
      $item['path'] = substr($item['url'], strlen($item['repository_root']));
    }
    $item['type'] = (string) $entry['kind'];
    $relative_path = (string) $entry['path'];
    $item['rev'] = intval((string) $entry['revision']); // current state of the item
    $item['created_rev'] = intval((string) $entry->commit['revision']); // last edit
    $item['last_author'] = (string) $entry->commit->author;
    $item['time_t'] = strtotime((string) $entry->commit->date);
    // $i = 'break on me';
    return $item;
  }

  /**
   * Override the parent implementation and always return FALSE on hasChildren,
   * because we know that we never want or need to recurse.
   */
  public function hasChildren() {
    return FALSE;
  }
}

/**
 * To compensate for ArrayAccess not being implemented on SplObjectStorage until
 * PHP 5.3
 *
 * @author sdboyer
 *
 */
class SplObjectMap extends SplObjectStorage implements ArrayAccess {
  protected $container = array();

  public function offsetExists($o) {
    return parent::contains($o);
  }

  public function offsetGet($o) {
    return parent::contains($o) ? $this->container[spl_object_hash($o)] : NULL;
  }

  public function offsetSet($o, $v) {
    parent::attach($o);
    $this->container[spl_object_hash($o)] = $v;
  }

  public function offsetUnset($o) {
    unset ($this->container[spl_object_hash($o)]);
    parent::detach($o);
  }
}

$wc = new SvnWorkingCopy('/home/sdboyer/ws/vcs/gj/trunk');
$info = $wc->newInvocation(FALSE);
$info->internalSwitches |= SvnCommand::PARSE_OUTPUT;
$info->xml();
$info->setParserClass('SvnInfoParser');
$info->target('index.php', 328);
$it = $info->execute();
/*
 * Thee preceding five lines, aside from setting the flag, could just as easily have been:
 *    $it = $wc->newInvocation(FALSE)->xml()->setParserClass('SvnInfoParser')->target('index.php', 328)->execute();
 *
 * And the xml() and setParserClass() are only there as an illustration; they're set by default, if I hadn't passed FALSE to SvnWorkingCopy::newInvocation:
 *    $it = newInvocation()->target('index.php', 328)->execute();
 *
 * For a lot of purposes, though, it may not be a good idea to bypass getting a copy of the SvnCommand object outside,
 * b/c with the way SplObjectMap works, you need the object to reference the contents of the arrays
 *
 */
foreach ($it as $key => $item) {
  echo $key;
  print_r($item);
}

$i = 'break on me';