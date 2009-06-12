<?php
require_once 'PHPUnit/Framework.php';
require_once SVNLIB_SRC . '/svn.php';

class SvnCommandInitTest extends PHPUnit_Framework_TestCase {

  public function setUp() {
    $this->wcConfig = new SvnCommandConfig();
    $this->repoConfig = new SvnCommandConfig();
    $this->wc = new SvnWorkingCopy(getcwd() . '/data/static/wc', $this->wcConfig);
    $this->repo = new SvnRepository('file://' . getcwd() . '/data/static/repo', $this->repoConfig);
    $this->instance = &$this->wc;
    $this->config = &$this->wcConfig;
  }

  /**
   * @expectedException InvalidArgumentException
   */
  public function testBadCommand() {
    $this->instance->svn('hooba');
  }
}

abstract class SvnCommandTest extends PHPUnit_Framework_TestCase {

  public static $comamnd;

  public function setUp() {
    $this->wcConfig = new SvnCommandConfig();
    $this->repoConfig = new SvnCommandConfig();
    $this->wc = new SvnWorkingCopy(getcwd() . '/data/static/wc', $this->wcConfig);
    $this->repo = new SvnRepository('file://' . getcwd() . '/data/static/repo', $this->repoConfig);
    $this->instance = &$this->wc;
    $this->config = &$this->wcConfig;
  }
}
