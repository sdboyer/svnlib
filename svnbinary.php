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
  public function __construct($path) {
    parent::__construct($path);
    $this->retContainer = new SplObjectMap();
    $this->cmdContainer = new SplObjectMap();
    $this->invocations = new SplObjectMap();
  }
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
  // const BIN_SVN     = 0x001; // only necessary if there are multiple binaries we might invoke on the working copy

  protected $cmd;
  public $invocations, $cmdContainer, $retContainer;

  public function __construct($path) {
    if (!is_dir("$path/.svn")) {
      throw new Exception("$path is not an svn working copy directory, as it contains no svn metadata.", E_RECOVERABLE_ERROR);
    }
    parent::__construct($path);
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

class SvnRepository extends SvnInstance {
  protected $cmd;
  public $invocations, $retContainer, $cmdContainer;

  public function __construct($path) {
    // Run a low-overhead operation, verifying this is a working svn repository.
    system('svnadmin lstxns ' . $path, $exit);
    if ($exit) {
      throw new Exception("$path is not a valid Subversion repository.", E_RECOVERABLE_ERROR);
    }
    parent::__construct($path);
  }
}

/*
class SvnlookCLI {
  const NO_AUTO_PROPS = 16;
  const NO_DIFF_DELETED = 17;
}
*/

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