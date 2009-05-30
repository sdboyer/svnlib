#! /usr/bin/php -d safe_mode=Off
<?php
define('SVNLIB_SRC', dirname(__FILE__) . '/../src');
// require_once dirname(__FILE__) . '/suite.php';
require_once 'PHPUnit/Framework.php';
require_once 'PHPUnit/Util/Filter.php';

PHPUnit_Util_Filter::addFileToFilter(__FILE__, 'PHPUNIT');

require 'PHPUnit/TextUI/Command.php';
?>
