<?php

/**
 * To compensate for ArrayAccess not being implemented on SplObjectStorage until
 * PHP 5.3
 *
 * @author sdboyer
 *
 */
class SplObjectMap extends ArrayObject {
  protected $sos;
  protected $container = array();

  public function __construct() {
    $this->sos = new SplObjectStorage();
    parent::__construct($this->sos);
  }

  public function offsetExists($o) {
    return $this->sos->contains($o);
  }

  public function offsetGet($o) {
    return $this->sos->contains($o) ? $this->container[spl_object_hash($o)] : NULL;
  }

  public function offsetSet($o, $v) {
    $this->sos->attach($o);
    $this->container[spl_object_hash($o)] = $v;
  }

  public function offsetUnset($o) {
    unset ($this->container[spl_object_hash($o)]);
    $this->sos->detach($o);
  }
}
/*
$a = new SplObjectMap();
$b = new SplFileInfo('/home/sdboyer/ws/dd/6dev/index.php');
$a[$b] = 'c';

$i = 'break on me';*/