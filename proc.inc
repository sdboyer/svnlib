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
  public function attachCommand(CLICommand $command);
  public function attachConfig(CLIWrapperConfig $config);
  public function getProcDescriptor();
  public function &getProcPipes();
  public function procOpen();
  public function procHandle();
  public function procClose();
}

interface CLIProcHandlerStdin {

}

interface CLIProcHandlerStdout {

}

interface CLIProcHandlerStderr {

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

abstract class ProcHandleGeneric implements CLIProcHandler {
  protected $procDescriptor = array();
  protected $procPipes = array();
  protected $process;
  protected $command;
  protected $ps;
  protected $config;

  public function __construct() {
    $this->ps = ProcStorage::getInstance();
  }

  public function getProcDescriptor() {
    return $this->procDescriptor;
  }

  public function &getProcPipes() {
    return $this->procPipes;
  }

  public function procOpen() {
    $this->procClose();
    $this->process = proc_open(implode(' ', $this->cmds), $this->getProcDescriptor(), $this->procPipes, $this->config->getWorkingPath(), NULL);
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

  public function attachCommand(CLICommand &$command) {
    $this->command = &$command;
    $this->ps->attachCommand($command, $this);
  }

  public function attachConfig(CLIWrapperConfig &$config) {
    $this->wrapper = &$config;
  }
}

class ProcHandleErrOnly extends ProcHandleGeneric implements CLIProcHandlerStderr {
  protected $procDescriptor = array(
    2 => array('pipe', 'w'),
  );
  protected $stdErr;

  public function procHandle() {
    $this->stdErr = stream_get_contents($this->procPipes[2]);
  }

  public function getStdErr() {
    return $this->stdErr;
  }
}

class ProcHandleErrOut extends ProcHandleGeneric implements CLIProcHandlerStderr, CLIProcHandlerStdout {
  protected $output;

  public function getProcDescriptor() {
    return array(
      1 => $this->output,
      2 => array('pipe', 'w'),
    );
  }

  public function openOutputHandle() {
    $this->output = fopen('php://temp', 'rw');
    return $this->output;
  }

  public function procHandle() {
    $this->stdErr = stream_get_contents($this->procPipes[2]);
    if ($this->stdErr) {
      $status = proc_get_status($this->process);
      if ($status['exitcode']) {
        throw new Exception('svn failed with the following message: ' . $this->stdErr, E_RECOVERABLE_ERROR);
      }
    }
  }
}

class ProcHandlerErrMulti extends ProcHandleGeneric implements CLIProcHandlerStderr {

}

