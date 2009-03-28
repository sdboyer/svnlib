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

interface CLIPipeStdIn {

}

interface CLIPipeStdOut {

}

/**
 * Dummy parser class that transparently passes output straight back without
 * any modification, but still implements the CLIParser interface so as to
 * reduce complexity in CLICommand implementations.
 */
class DummyParser implements CLIParser {
  protected $output;

  public function __construct() {}
  public function openOutputHandle() {
    $this->output = fopen('php://temp', 'rw');
    return $this->output;
  }

  public function parseOutput() {
    return stream_get_contents($this->output);
  }

  public function clear() {}

  public function procClose($destruct = FALSE) {
    if (is_resource($this->output)) {
      fclose($this->output);
    }
  }
}