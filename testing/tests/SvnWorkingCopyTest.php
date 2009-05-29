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
    $config = new SvnCommandConfig();
    $wc = new SvnWorkingCopy(getcwd() . '/wc/trunk', $config);
    $this->assertEquals(getcwd() . DIRECTORY_SEPARATOR . 'wc', (string) $wc, "Wrapper was not properly reset to the working copy root.");
    $this->assertEquals($config->subPath, 'trunk', "Subpath was not properly extracted from initial path argument.");
  }
}

class SvnWorkingCopyTest extends PHPUnit_Framework_TestCase {
  public function setUp() {
    $this->config = new SvnCommandConfig();
    $this->instance = new SvnWorkingCopy('./wc', $this->config);
  }

  public function testInfoBuild() {
    $this->assertEquals("file://" . getcwd() . DIRECTORY_SEPARATOR . 'repo', $this->instance->getRepoRoot(), "Incorrect repository root was retrieved.");
    $this->assertEquals(340, $this->instance->getLatestRev(), "Incorrect latest revision was retrieved.");
  }

  public function testSetSubpath() {
    $this->instance->setSubPath('trunk');
    $this->assertEquals('trunk', $this->config->subPath, "Subpaths are not being set correctly by SvnInstance::setSubPath().");
    $this->instance->setSubPath('');
    $this->instance->appendSubPath('trunk');
    $this->assertEquals('trunk', $this->config->subPath, "Subpaths are not being built correctly by SvnInstance::appendSubPath().");
  }
}
