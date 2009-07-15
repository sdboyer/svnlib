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
  public function attachInputHandler(ProcInputHandler &$io, $descriptor = 0);
  public function attachOutputHandler(ProcOutputHandler &$io, $descriptor = 1);
  public function attachConfig(CLIWrapperConfig &$config);
  public function getProcDescriptor();
  public function &getProcPipes();
  public function procOpen();
  public function procHandle();
  public function procClose();
  public function execute();
}

interface ProcIO {
  public function procHandle();
  public function procClose();
}

/**
 * These go on stdin, and/or some other nonstandard file descriptor.
 */
interface ProcInputHandler extends ProcIO {
  public function getInputDescriptor();
}

interface ProcInputPipe extends ProcInputHandler {
  public function &connectOut();
}

/**
 * These go on stdout and stderr, and/or some other nonstandard file descriptor.
 */
interface ProcOutputHandler extends ProcIO  {
  public function getOutputDescriptor();
}

interface ProcOutputPipe extends ProcOutputHandler {
  public function connectIn(&$pipe);
}

class ProcIOPipe implements ProcInputPipe, ProcOutputPipe {
  protected $outConnected = FALSE;
  protected $pipe;

  public function procHandle() {
    if (!$this->outConnected) return stream_get_contents($this->pipe);
  }

  public function getOutputDescriptor() {
    return array('pipe', 'w');
  }

  public function getInputDescriptor() {
    return array('pipe', 'r');
  }

  public function &connectOut() {
    $this->outConnected = TRUE;
    return $this->pipe;
  }

  public function connectIn(&$pipe) {
    $this->pipe = &$pipe;
  }

  public function procClose() {
    // do nothing, should always be handled by the connected proc objects
  }
}

class ProcIOFileTemp implements ProcOutputHandler {
  protected $output;

  public function getOutputDescriptor() {
    $this->output = fopen('php://temp', 'rw');
    return $this->output;
  }

  public function procHandle() {
    rewind($this->output);
    return stream_get_contents($this->output);
  }

  public function procClose($destruct = FALSE) {
    if (is_resource($this->output)) {
      fclose($this->output);
    }
  }
}

abstract class ProcHandlerGeneric implements CLIProcHandler {
  protected $pipes = array();
  protected $process;
  protected $command;
  protected $config;
  protected $io;
  protected $inputs = array();
  protected $outputs = array();
  public $env;
  public $ret = array();

  public function __construct($fds) {
    $this->pipes = array_fill_keys($fds, FALSE);
  }

  public function attachCommand(CLICommand &$command) {
    $this->command = &$command;
  }

  public function attachConfig(CLIWrapperConfig &$config) {
    $this->config = &$config;
  }

  public function getProcDescriptor() {
    $descriptor = array();
    foreach ($this->inputs as $fd => $handler) {
      $descriptor[$fd] = $handler->getInputDescriptor();
    }
    foreach ($this->outputs as $fd => $handler) {
      $descriptor[$fd] = $handler->getOutputDescriptor();
    }
    return $descriptor;
  }

  public function attachInputHandler(ProcInputHandler &$io, $descriptor = 0) {
    if ($descriptor === 1) {
      throw new InvalidArgumentException("Illegal file descriptor specification: stdout cannot take an input handler.", E_RECOVERABLE_ERROR);
    }
    if ($descriptor === 2) {
      throw new InvalidArgumentException("Illegal file descriptor specification: stderr cannot take an input handler..", E_RECOVERABLE_ERROR);
    }

    $this->inputs[$descriptor] = &$io;
    $this->io[$descriptor] = &$io;
  }

  public function attachOutputHandler(ProcOutputHandler &$io, $descriptor = 1) {
    if ($descriptor === 0) {
      throw new InvalidArgumentException("Illegal file descriptor specification: stdin cannot take an output handler.", E_RECOVERABLE_ERROR);
    }

    $this->outputs[$descriptor] = &$io;
    $this->io[$descriptor] = &$io;
  }

  public function &getProcPipes() {
    return $this->pipes;
  }

  public function procOpen() {
    $this->procClose();
    $this->process = proc_open($this->command->getShellString(),
      $this->getProcDescriptor(), $this->pipes,
      $this->config->getWorkingPath(), $this->env);
    // Can only connect pipes AFTER the proc has been opened, because the
    // internals of proc_open() do not respect pre-existing symbol table aliases
    // (references) set to the appropriate item in $this->pipes.
    $this->connectPipes();
  }

  protected function connectPipes() {
    foreach ($this->inputs as $descriptor => &$io) {
      if ($io instanceof ProcInputPipe) {
        $this->pipes[$descriptor] = &$io->connectOut();
      }
    }
    foreach ($this->outputs as $descriptor => &$io) {
      if ($io instanceof ProcOutputPipe) {
        $io->connectIn($this->pipes[$descriptor]);
      }
    }
  }

  public function procHandle() {
    foreach ($this->io as $fd => $io) {
      $this->ret[$fd] = $io->procHandle();
    }
  }

  public function procClose() {
    if (!is_resource($this->process)) {
      return;
    }

    foreach ($this->io as $fd => &$io) {
      $io->procClose();
      if (isset($this->pipes[$fd]) && is_resource($this->pipes[$fd])) {
        fclose($this->pipes[$fd]);
      }
    }

    $exit = proc_close($this->process);
    if ($exit !== 0) {
      $text = "Invocation of command '{$this->command->getShellString()}' failed with exit code $exit";
      // if ($this->io[2] instanceof ProcIO && $stderr = (string) $this->io[2]) {
      if (!empty($this->ret[2])) {
        $text .= " and the following stderr output: '{$this->ret[2]}'";
      }
      throw new RuntimeException($text, E_RECOVERABLE_ERROR);
    }
  }

  public function execute($fds = NULL) {
    $this->procOpen();
    $this->procHandle();
    $this->procClose();
    if (is_numeric($fds) || is_array($fds)) {
      return is_array($fds) ? array_intersect_key($this->ret, array_flip($fds)) : $this->ret[$fds];
    }
    return $this->ret;
  }
}

class CLIProcHandlerSingle extends ProcHandlerGeneric {

  public function __construct($fds = array(1, 2)) {
    parent::__construct($fds);
    // FIXME can easily blow up here
    $this->outputs = array(
      1 => new ProcIOPipe(),
      2 => new ProcIOPipe(),
    );
    $this->io = array(
      1 => &$this->outputs[1],
      2 => &$this->outputs[2],
    );
    // $this->outputs[2]->connectIn($this->pipes[2]);
  }
}
