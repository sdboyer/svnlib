<?php
require_once 'PHPUnit/Framework.php';
require_once '../src/svn.php';
class SvnWorkingCopyTest extends PHPUnit_Framework_TestCase {
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
}
