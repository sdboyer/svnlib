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
    $wc = new SvnWorkingCopy(getcwd() . '/data/static/wc/trunk', $config);
    $this->assertEquals(getcwd() . DIRECTORY_SEPARATOR . 'data/static/wc', (string) $wc,
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
    $this->assertEquals("file://" . getcwd() . DIRECTORY_SEPARATOR . 'data/static/repo', $this->instance->getRepoRoot(),
      "Incorrect repository root was retrieved.");
    $this->assertEquals(340, $this->instance->getLatestRev(),
      "Incorrect latest revision was retrieved.");
  }

  public function testGlobalOpts() {
    $this->instance->username('usernametest');
    $this->assertEquals('usernametest', $this->config->username,
      "Username are not being set properly by {$this->instanceClass()}::username().");
    $this->instance->password('passwordtest');
    $this->assertEquals('passwordtest', $this->config->password,
      "Username are not being set properly by {$this->instanceClass()}::username().");
    $this->instance->username('usernametest');
    $this->assertEquals('usernametest', $this->config->username,
      "Username are not being set properly by {$this->instanceClass()}::username().");
  }

  public function testSubpathing() {
    $this->instance->setSubPath('trunk');
    $this->assertEquals('trunk', $this->config->subPath,
      "Subpaths are not being set correctly by {$this->instanceClass()}::setSubPath().");
    $this->instance->setSubPath('');
    $this->instance->appendSubPath('trunk');
    $this->assertEquals('trunk', $this->config->subPath,
      "Subpaths are not being built correctly by {$this->instanceClass()}::appendSubPath().");
  }

  public function testConfigMethod() {

  }

  protected function instanceClass() {
    return get_class($this->instance);
  }
}

class SvnWorkingCopyTest extends SvnInstanceTest {

  public function setUp() {
    parent::setUp();
    $this->instance = new SvnWorkingCopy(getcwd() . '/data/static/wc', $this->config);
  }
}

class SvnRepositoryTest extends SvnInstanceTest {

  public function setUp() {
    parent::setUp();
    $this->instance = new SvnRepository('file://' . getcwd() . '/data/static/repo', $this->config);
  }

  public function testCreateWorkingCopy() {
    $wc = $this->instance->checkoutWorkingCopy(getcwd() . '/data/static/wctmp');
    $this->assertTrue($wc instanceof SvnWorkingCopy, "SvnRepository::checkoutWorkingCopy failed to generate a working copy appropriately.");
    rmdirr($wc);
  }
}

function rmdirr($path) {
  foreach (new RecursiveIteratorIterator(new RecursiveDirectoryIterator($path), RecursiveIteratorIterator::CHILD_FIRST) as $item) {
    $item->isFile() ? unlink($item) : rmdir($item);
  }
  rmdir($path);
}
