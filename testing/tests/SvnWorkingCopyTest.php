<?php
require_once 'PHPUnit/Framework.php';
require_once '../src/svn.php';
class SvnWorkingCopyTest extends PHPUnit_Framework_TestCase {
  /**
   * @expectedException Exception
   */
  public function testVerifyFailureNotDir() {
    new SvnWorkingCopy('./foo');
  }

   /**
   * @expectedException Exception
   */
  public function testVerifyFailureNotSvnDir() {
    new SvnWorkingCopy('.');
  }
}
