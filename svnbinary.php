<?php

// TODO temporary straight includes until a smarter system is introduced
require_once __FILE__ . '/commands/svn.commands.inc';
require_once __FILE__ . '/opts/svn.opts.inc';

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