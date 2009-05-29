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
 * Parent class for opts that can be used by various svn subcommands.
 * @author sdboyer
 *
 */
abstract class SvnOpt implements CLICommandOpt {
  /**
   *
   * @var SvnCommand
   */
  protected $sc;
  protected $arg = '';
  protected $concatenator = ' ';

  public function __construct(SvnCommand $sc, $arg) {
    $this->sc = $sc;
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
    return $this->ordinal;
  }

  public function getShellString() {
    if (method_exists($this, 'prepare')) {
      $this->prepare();
    }
    return $this->opt . $this->concatenator . escapeshellarg($this->arg);
  }
}

class SvnOptRevision extends SvnOpt {
  protected $ordinal = 1;

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

//  public function prepare() {
//    if ($this->cmdOpts[self::TARGETS] && $this->cmdOpts[self::TARGETS]->usingRevs) {
//      throw new Exception('Setting explicit revision numbers (via -r, that is, SvnOptRevision) will override the revisions explicitly specified per target.', E_WARNING);
//    }
//    return $this;
//  }

  public function getShellString() {
    $string = $this->opt . escapeshellarg($this->arg);
    if (!empty($this->arg2)) {
      $string .= ':' . escapeshellarg($this->arg2);
    }
    return $string;
  }
}

class SvnOptAccept extends SvnOpt  {
  protected $ordinal = 2;

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
  protected $ordinal = 3;
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
  protected $ordinal = 4;
  protected $concatenator = '=';
  protected $opt = '--targets';
  protected $mp, $fp;
  protected $fileExternal, $fileOut;
  protected $usingRevs = FALSE;

  public function __construct(SvnCommand &$sc) {
    $this->sc = &$sc;
    $this->mp = fopen('php://memory', 'rw');
  }

  public function setTargetsFile($file) {
    if (!is_file($file)) {
      throw new Exception('Non-file argument passed to SvnOptTargets.', E_RECOVERABLE_ERROR);
    }
    else {
      $this->fileExternal = $file;
    }
    return $this;
  }

  public function addTarget($target, $rev = NULL) {
    $string = $this->sc->getPrependPath();
    // FIXME what's the proper approach to escaping input here?
    if ($rev !== NULL) {
      $this->usingRevs = TRUE;
      $string .= $target . '@' . $rev;
    }
    else {
      $string .= $target;
    }
    fwrite($this->mp, $string . "\n");
    return $this;
  }

  protected function cleanUp() {
    if (is_resource($this->mp)) {
      fclose($this->mp);
    }
    if (is_resource($this->fp)) {
      fclose($this->fp);
    }
    if (file_exists($this->fileOut)) {
      unlink($this->fileOut);
    }
  }

  public function __destruct() {
    $this->cleanUp();
  }

  public function prepare() {
    $this->fileOut = '/tmp/svncommand-' . spl_object_hash($this->sc);
    $this->fp = fopen($this->fileOut, 'w');
    //
    if (!empty($this->fileExternal)) {
      fwrite($this->fp, file_get_contents($this->fileExternal));
    }
    rewind($this->mp);
    fwrite($this->fp, stream_get_contents($this->mp));
    // Very important that we fclose() here, to ensure that a second call won't
    // generate duplicate data. We leave the memory handle open, though.
    fclose($this->fp);
    return $this;
  }

  public function getShellString() {
    $this->prepare();
    return '--targets ' . $this->fileOut;
  }
}

class SvnOptTarget extends SvnOpt {
  protected $ordinal = 5;
  protected $rev = FALSE;

  public function revision($rev) {
    if (!is_int($rev)) {
      throw new Exception('Non-integer revision argument, "' . $rev . '" passed to SvnOptTarget.', E_RECOVERABLE_ERROR);
    }
    $this->rev = $rev;
    return $this;
  }

  public function getShellString() {
    $string = $this->sc->getPrependPath();
    $string .= ($this->rev === FALSE) ?  $this->arg : $this->arg . '@' . $this->rev;
    return escapeshellarg($string);
  }
}

/**
 * Opt for handling `svn --username`.
 * @author sdboyer
 *
 * FIXME I'm a little uncomfortable about an inheritance hierarchy where this
 * has SvnOpt as its parent. Same goes for SvnPassword, SvnConfigdir.
 */
class SvnOptUsername  extends SvnOpt {
  protected $ordinal = 1;
  protected $opt = '--username';
}

class SvnOptPassword extends SvnOpt {
  protected $ordinal = 2;
  protected $opt = '--password';
}

class SvnOptConfigDir extends SvnOpt {
  protected $ordinal = 3;
  protected $opt = '--config-dir';

  protected function checkArg($arg) {
    if (!is_dir($arg)) {
      throw new Exception("'" . $arg . "' is not a directory; SvnConfigdir requires a valid directory be provided.", E_RECOVERABLE_ERROR);
      return FALSE;
    }
    return TRUE;
  }
}

class SvnOptExtensions extends SvnOpt {
  protected $ordinal = 6;
  protected $opt = '-x';

  protected function checkArg($arg) {
    if (!in_array($arg, array('-u', '-b', '-w', '--ignore-eol-style', '-p'))) {
      throw new Exception("An invalid arg, '$arg', was passed to SvnOptExtensions. Note that only the short forms of the args are permitted.", E_RECOVERABLE_ERROR);
      return FALSE;
    }
    return TRUE;
  }
}

class SvnOptChangelist extends SvnOpt {
  protected $ordinal = 7;
  protected $opt = '--changelist';
}

class SvnOptMessage extends SvnOpt {
  protected $ordinal = 8;
  protected $opt = '-m';
}

class SvnOptFile extends SvnOpt {
  protected $ordinal = 9;
  protected $opt = '-F';

  protected function checkArg($arg) {
    if (!is_file($arg)) {
      throw new Exception('Non-file argument passed to SvnOptFile.', E_RECOVERABLE_ERROR);
    }
  }
}

class SvnOptEncoding extends SvnOpt {
  protected $ordinal = 10;
  protected $opt = '--encoding';
}

class SvnOptWithRevprop extends SvnOpt {
  protected $ordinal = 11;
  protected $opt = '--with-revprop';
}

class SvnOptNativeEol extends SvnOpt {
  protected $ordinal = 12;
  protected $opt = '--native-eol';
}

class SvnOptShowRevs extends SvnOpt {
  protected $ordinal = 13;
  protected $opt = '--show-revs';
}
