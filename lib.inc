<?php

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

interface CLIWrapper {
  public function getWorkingPath();
}

interface CLICommand {
  public function prepare();
  public function execute();
}

interface CLICommandOpt {
  public function getOrdinal();
  public function getShellString();
}

interface CLIParser {
  /**
   * @return Resource
   */
  public function openOutputHandle();
  public function parseOutput();
  public function procClose($destruct = FALSE);
  public function clear();
}

interface CLIProcHandler {
  public function getProcDescriptor();
  public function &getProcPipes();
  public function procOpen();
  public function procHandle();
  public function procClose();
}

interface CLIPipeStdIn {

}

interface CLIPipeStdOut {

}

abstract class ProcHandleGeneric implements CLIProcHandler {
  protected $procDescriptor = array();
  protected $procPipes = array();
  protected $process;

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

  // abstract public function procHandle();
}

class ProcHandleErrOnly extends ProcHandleGeneric {
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

class ProcHandleErrOut extends ProcHandleGeneric {
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
}
