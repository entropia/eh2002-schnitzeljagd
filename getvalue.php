<?
function getvar($foo) {
    return @join("", @file(dirname(__FILE__)."/".$foo));
}
?>

