<?php
require_once 'PHPUnit/Framework.php';
require_once '../src/svn.php';
class SvnWorkingCopyInitTest extends PHPUnit_Framework_TestCase {
  /**
   * @expectedException InvalidArgumentException
   */
  public function testVerifyFailureNotDir() {
    new SvnWorkingCopy('./foo');
  }

   /**
   * @expectedException InvalidArgumentException
   */
  public function testVerifyFailureNotSvnDir() {
    new SvnWorkingCopy('.');
  }

  public function testAutoSubPathing() {
    $wc = new SvnWorkingCopy('wc/trunk');

  }
}

class SvnWorkingCopyTest extends PHPUnit_Framework_TestCase {
  public function setUp() {
    $this->wc = new SvnWorkingCopy('./wc');
  }

  public function testInfoBuild() {
    $this->assertEquals(getcwd() . DIRECTORY_SEPARATOR . 'repo', $this->wc->getRepoRoot(), "Incorrect repository root was retrieved.");
    $this->assertEquals(340, $this->wc->getLatestRev(), "Incorrect latest revision was retrieved.");
  }
}
