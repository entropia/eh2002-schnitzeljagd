<?
$a=range(0,255);shuffle($a);
foreach($a as $b=>$c) {
    echo sprintf("0x%02x, ", $c);
    if ($b % 16==15) echo "\n";
}
?>

