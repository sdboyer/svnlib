<?php
require_once 'PHPUnit/Framework.php';
require_once '../src/svn.php';

abstract class SvnCommandInitTest extends PHPUnit_Framework_TestCase {

  public function setUp() {
    $this->config = new SvnCommandConfig();
  }

  /**
   * @expectedException InvalidArgumentException
   */
  public function testBadCommand() {
    new SvnWorkingCopy('./foo');
  }
}

class SvnWorkingCopyTest extends SvnInstanceTest {

  public function setUp() {
    parent::setUp();
    $this->instance = new SvnWorkingCopy(getcwd() . '/wc', $this->config);
  }
}

class SvnRepositoryTest extends SvnInstanceTest {

  public function setUp() {
    parent::setUp();
    $this->instance = new SvnRepository('file://' . getcwd() . '/repo', $this->config);
  }
}
