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

interface CLIWrapperConfig {
  public function getWorkingPath();
  public function getPrependPath();
  public function attachWrapper(CLIWrapper $wrapper);
}

interface CLICommand {
  const NOTIFY_EXECUTE = 0x001;
  /**
   * The name of the class, as a string, that this object prefers as its default
   * process handler. This process handler will be used if no higher-priority
   * specification for a different process handler is made.
   *
   * @var string
   */
  const PROC_HANDLER = NULL;
  public function prepare();
  public function execute();
  /**
   * Returns an object implementing the CLIProcHandler interface which should be
   * used as the default proc handler for this command - i.e., when no other
   * proc handler is specified.
   *
   * @return CLIProcHandler
   */
  public static function getDefaultProcHandler();
}

interface CLICommandRead {
  // public function parse();
}

interface CLICommandWrite {

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
