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
