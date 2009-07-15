<?php

/**
 * To compensate for ArrayAccess not being implemented on SplObjectStorage until
 * PHP 5.3
 *
 * Documentation refers to this as a "splos."
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
  const SKIP_VERIFY = 0x001;
  const SKIP_BUILD = 0x002;
  public function getWorkingPath();
}

interface CLIWrapperConfig {
  public function getWorkingPath();
  public function getPrependPath();
  public function attachWrapper(CLIWrapper &$wrapper);
}

interface CLICommand {
  /**
   * The name of the class, as a string, that this object prefers as its default
   * process handler. This process handler will be used if no higher-priority
   * specification for a different process handler is made.
   *
   * @var string
   */
  const PROC_HANDLER = 'CLIProcHandlerSingle';
  public function __construct(CLIWrapperConfig $config, CLIProcHandler &$proc, $defaults);
  public function prepare();
  public function execute();
  public function attachProcHandler(CLIProcHandler &$proc);
}

interface CLICommandOpt {
  public function getOrdinal();
  public function getShellString();
}