<?php

/**
 * @file proc.inc
 *
 * Contains generic interfaces and classes for the fully abstracted proc
 * handling system.
 *
 * @author sdboyer
 *
 */

interface CLIProcHandler {
  public function attachCommand(CLICommand &$command);
  public function attachIOHandler(CLIIOHandler &$parser, int $descriptor);
  public function attachConfig(CLIWrapperConfig &$config);
  public function getProcDescriptor();
  public function &getProcPipes();
  public function procOpen();
  public function procHandle();
  public function procClose();
  public function execute();
}

interface CLIProcHandlerStdin {
  public function getStdinSource();
}

interface CLIProcHandlerStdout {
  const PARSE_OUTPUT = 0x001;
  const PRESERVE_PARSER = 0x002;

  public function setParser(CLIParser &$parser);
  public function getStdoutParser();
}

interface CLIProcHandlerStderr {
  public function getStderr();
  public function getStderrParser();
}

/**
 * The ProcStorage class (a Singleton) provides an associational layer that maps
 * command objects to the process handling objects that manage them.
 *
 * This class has little use for plain, single-command invocations; it is
 * intended primarily for complex chains of piped commands, where client code
 * may need to directly communicate with the proc handler their commands are
 * using. It is also easy to perform a simple 'lookup' that determines which
 * process handler manages any given command object.
 *
 * IMPORTANT NOTE: currently, ProcStorage only allows for commands to be
 * associated with a single proc handler. Attempting to attach a command to
 * multiple proc handlers will result in an E_RECOVERABLE_ERROR. Because it is
 * entirely reasonable that client code might want to reuse a single command as
 * a step in an oft-repeated chain of piped commands, it is on our TODO list
 * that we do allow such multiple attachments. However, if you require this
 * functionality right now, you will have to settle for manually cloning that
 * reused command object for each additional proc handler to which you attach
 * it.
 *
 * @author sdboyer
 *
 */
class ProcStorage {
  protected static $instance;

  protected $storage;

	protected function __construct() {
		$this->storage = new SplObjectMap();
	}

	/**
	 * Attach a command and its managing proc handler to the storage object.
	 *
	 * @param CLICommand $command
	 * @param CLIProcHandler $handler
	 * @return void
	 */
	public function attachCommand(CLICommand $command, CLIProcHandler $handler) {
	  if (isset($this->storage[$command])) {
	    throw new Exception("Attempted to assign a command to a proc handler, but that command already has a proc handler.", E_RECOVERABLE_ERROR);
	  }
	  $this->storage[$command] = $handler;
	}

	public function hasCommand(CLICommand $command) {
	  return isset($this->storage[$command]);
	}

	public function getProcHandler(CLICommand $command) {
	  return $this->storage[$command];
	}

	public function detachCommand(CLICommand $command) {
	  unset($this->storage[$command]);
	}

  public static function getInstance() {
    if (empty(self::$instance)) {
      self::$instance = new ProcStorage();
    }
    return self::$instance;
  }
}

abstract class ProcHandlerGeneric implements CLIProcHandler {
  protected $procDescriptor = array();
  protected $procPipes = array();
  protected $process;
  protected $command;
  // protected $ps;
  protected $config;
  protected $io = array();
  protected $activeIo = array();

  public function __construct() {

  }

  public function attachCommand(CLICommand &$command) {
    $this->command = &$command;
    // $this->ps->attachCommand($command, $this);
  }

  public function attachConfig(CLIWrapperConfig &$config) {
    $this->wrapper = &$config;
  }

/*  public function getProcDescriptor() {
    return $this->procDescriptor;
  }*/

  public function attachIOHandler(CLIIOHandler &$io, int $descriptor) {
    $this->io[$descriptor] = &$io;
  }

  public function &getProcPipes() {
    return $this->procPipes;
  }

  public function procOpen() {
    $this->procClose();
    $this->process = proc_open($this->command->getShellString(),
      $this->getProcDescriptor(), $this->procPipes,
      $this->config->getWorkingPath(), NULL);
  }

  public function procHandle() {
    $this->stderr = stream_get_contents($this->procPipes[2]);
    if ($this->stderr) {
      $status = proc_get_status($this->process);
      if ($status['exitcode'] > 0) {
        throw new Exception('svn failed with the following message: ' . $this->stderr, E_RECOVERABLE_ERROR);
      }
    }
  }

  public function procClose() {
    if (is_resource($this->process)) {
      foreach ($this->procPipes as $pipe) {
        fclose($pipe);
      }
      $this->procPipes = array();
      $this->process = proc_close($this->process);
    }
  }

  public function execute() {
    $this->procOpen();
    $this->procHandle();
    $this->procClose();
  }
}

class CLIProcHandlerSingle extends ProcHandlerGeneric {

  public function __construct() {
    $this->io = array(
      1 => new PassthruParser(),
      2 => new PassthruParser(),
    );
  }

  public function getProcDescriptor() {
    $descriptor = array();
    foreach ($this->io as $fd => $handler) {
      $descriptor[$fd] = $handler->getProcDescriptor();
    }
    return $descriptor;
  }
}

/*class CLIProcHandlerDouble extends ProcHandlerGeneric {

}*/

/*class CLIProcHandlerMulti extends ProcHandlerGeneric {

}*/


/*class ProcHandlerErrOnly extends ProcHandlerGeneric implements CLIProcHandlerStderr {
  protected $procDescriptor = array(
    2 => array('pipe', 'w'),
  );

  public function getStderr() {
    return $this->stderr;
  }

  public function getStderrParser() {

  }
}

class ProcHandlerErrOut extends ProcHandlerGeneric implements CLIProcHandlerStderr, CLIProcHandlerStdout {
  protected $stderr;

  public function getProcDescriptor() {
    return array(
      1 => $this->activeParser->openOutputHandle(),
      2 => array('pipe', 'w'),
    );
  }

  public function openOutputHandle() {
    $this->output = fopen('php://temp', 'rw');
    return $this->output;
  }

  public function getStderr() {
    return $this->stderr;
  }

  public function setParser(CLIParser &$parser) {
    $this->parser = &$parser;
  }

  public function execute() {
    if ($this->parser instanceof CLIParser) {
      $this->activeParser = $this->parser;
    }
    else {
      $this->activeParser = new PassthruParser();
    }
    parent::execute();
    // Unlink the active parser in the symbol table. Just an object identifier
    // if a real parser was explicitly set (which means the object itself will
    // remain). Otherwise it's the passthru parser and this will remove the sole
    // symbol table reference to it, thereby inducing GC on the object.
    unset($this->activeParser);
    return $this->ret;
  }
}

class ProcHandlerErrMulti extends ProcHandlerGeneric implements CLIProcHandlerStderr {

}

*/