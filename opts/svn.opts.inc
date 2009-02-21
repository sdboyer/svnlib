<?php

/**
 * @file svn.opts.inc
 *
 * Class definitions for svn command opts (command line parameters that take
 * arguments). Note that, under the circumstances envisioned by the author,
 * these will only ever be used internally. There is no way to attach them
 * directly to an SvnCommand object from outside the object.
 */

/**
 * Parent class for opts that can be used by various Subversion `svn` subcommands.
 * @author sdboyer
 *
 */
abstract class SvnOpt implements CLICommandOpt {
  protected $arg = '';
  protected $concatenator = ' ';

  public function __construct(SvnCommand &$sc, $arg) {
    $this->sc = &$sc;
    if (method_exists($this, 'checkArg')) {
      if ($this->checkArg($arg)) {
        $this->arg = $arg;
      }
    }
    else {
      $this->arg = $arg;
    }
  }

  public function getOrdinal() {
    return self::ORDINAL;
  }

  public function getShellString() {
    return $this->opt . $this->concatenator . escapeshellarg($this->arg);
  }
}

class SvnOptRevision extends SvnOpt {
  const ORDINAL = 1;

  protected $arg2 = '', $opt = '-r';

  public function range($rev) {
    if ($this->checkArg($rev)) {
      $this->arg2 = $rev;
    }
    return $this;
  }

  protected function checkArg($arg) {
    if (!is_int($arg)) {
      // FIXME currently does not allow date-based revision range args
      if (!in_array($arg, array('HEAD', 'BASE', 'COMMITTED', 'PREV'))) {
        throw new Exception("Invalid revision information passed as an argument to SvnOptRevision", E_RECOVERABLE_ERROR);
        return FALSE;
      }
    }
    return TRUE;
  }

  public function getShellString() {
    $string = $this->opt . escapeshellarg($this->arg);
    if (!empty($this->arg2)) {
      $string .= ':' . escapeshellarg($this->arg2);
    }
    return $string;
  }
}

class SvnOptAccept extends SvnOpt  {
  const ORDINAL = 2;

  protected $opt = '--accept ';

  protected function checkArg($arg) {
    if (!in_array($arg, array('base', 'working', 'mine-full', 'theirs-full'))) {
      throw new Exception("Invalid argument '$arg' passed to SvnOptAccept", E_RECOVERABLE_ERROR);
      return FALSE;
    }
    return TRUE;
  }
}

class SvnOptDepth extends SvnOpt {
  const ORDINAL = 3;
  protected $concatenator = '=';
  protected $opt = '--depth';

  protected function checkArg($arg) {
    if (!in_array($arg, array('infinity', 'files', 'immediates', 'empty'))) {
      throw new Exception("Invalid argument '$arg' passed to SvnOptDepth", E_RECOVERABLE_ERROR);
      return FALSE;
    }
    return TRUE;
  }

  public function changeArg($arg) {
    if ($this->checkArg($arg)) {
      $this->arg = $arg;
    }
    return $this;
  }
}

class SvnOptTargets extends SvnOpt {
  const ORDINAL = 4;
  protected $concatenator = '=';
  protected $opt = '--targets';

  protected function checkArg($arg) {
    if (!is_file($arg)) {
      throw new Exception('Non-file argument passed to SvnOptTargets.', E_RECOVERABLE_ERROR);
      return FALSE;
    }
    return TRUE;
  }
}

class SvnOptTarget extends SvnOpt {
  const ORDINAL = 5;
  protected $rev = FALSE;

  public function revision($rev) {
    if (!is_int($rev)) {
      throw new Exception('Non-integer revision argument, "' . $rev . '" passed to SvnOptTarget.', E_RECOVERABLE_ERROR);
    }
    $this->rev = $rev;
    return $this;
  }

  public function getShellString() {
    $string = escapeshellarg($this->arg);
    return ($this->rev === FALSE) ?  $string : $string . '@' . escapeshellarg($this->rev);
  }
}
