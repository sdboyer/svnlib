<?php
require_once 'PHPUnit/Framework.php';
require_once SVNLIB_SRC . '/svn.php';

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
    $wc = new SvnWorkingCopy(getcwd() . '/testdata/wc/trunk', $config);
    $this->assertEquals(getcwd() . DIRECTORY_SEPARATOR . 'testdata/wc', (string) $wc,
      "Wrapper was not properly reset to the working copy root.");
    $this->assertEquals($config->subPath, 'trunk',
      "Subpath was not properly extracted from initial path argument.");
  }
}

abstract class SvnInstanceTest extends PHPUnit_Framework_TestCase {

  public function setUp() {
    $this->config = new SvnCommandConfig();
  }

  public function testInfoBuild() {
    $this->assertEquals("file://" . getcwd() . DIRECTORY_SEPARATOR . 'testdata/repo', $this->instance->getRepoRoot(),
      "Incorrect repository root was retrieved.");
    $this->assertEquals(340, $this->instance->getLatestRev(),
      "Incorrect latest revision was retrieved.");
  }

  public function testGlobalOpts() {
    $this->instance->username('usernametest');
    $this->assertEquals('usernametest', $this->config->username,
      "Username are not being set properly by SvnInstance::username().");
    $this->instance->password('passwordtest');
    $this->assertEquals('passwordtest', $this->config->password,
      "Username are not being set properly by SvnInstance::username().");
    $this->instance->username('usernametest');
    $this->assertEquals('usernametest', $this->config->username,
      "Username are not being set properly by SvnInstance::username().");
  }

  public function testSubpathing() {
    $this->instance->setSubPath('trunk');
    $this->assertEquals('trunk', $this->config->subPath,
      "Subpaths are not being set correctly by SvnInstance::setSubPath().");
    $this->instance->setSubPath('');
    $this->instance->appendSubPath('trunk');
    $this->assertEquals('trunk', $this->config->subPath,
      "Subpaths are not being built correctly by SvnInstance::appendSubPath().");
  }

  public function testConfigMethod() {

  }
}

class SvnWorkingCopyTest extends SvnInstanceTest {

  public function setUp() {
    parent::setUp();
    $this->instance = new SvnWorkingCopy(getcwd() . '/testdata/wc', $this->config);
  }
}

class SvnRepositoryTest extends SvnInstanceTest {

  public function setUp() {
    parent::setUp();
    $this->instance = new SvnRepository('file://' . getcwd() . '/testdata/repo', $this->config);
  }
}
