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
  public function getProcDescriptor();
  public function &getProcPipes();
  public function procOpen();
  public function procHandle();
  public function procClose();
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
