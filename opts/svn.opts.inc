<?php


/**
 * Parent class for opts that can be used by various Subversion `svn` subcommands.
 * @author sdboyer
 *
 */
abstract class SvnOpt implements CLICommandOpt {
  public function __construct(SvnCommand &$sc) {
    $this->sc = &$sc;
  }

  public function getOrdinal() {
    return self::ORDINAL;
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

  public function getShellString() {
    return '--depth=' . $this->arg;
  }
}

class SvnOptTargets extends SvnOpt {

  public function getShellString() {

  }
}

class SvnOptTarget extends SvnOpt {
  protected $target = '', $rev = FALSE;

  public function __construct(SvnCommand $sc, $target) {
    parent::__construct($sc);
    $this->target = $target;
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
