--TEST--
testing variadics hint disallow
--SKIPIF--
<?php
if (!version_compare(phpversion(), "5.6.0", ">")) {
    die("skip: version 5.6+ required");
}
?>
--FILE--
<?php
function test(int ... $ints) {
    var_dump($ints);
}

test(1, 2, 3, 4, true);
?>
--EXPECTF--
Catchable fatal error: Argument 5 passed to test must be integer, boolean given in %s on line 2
