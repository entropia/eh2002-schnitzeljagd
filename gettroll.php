<?
set_time_limit(0);
mysql_connect("localhost", "root", "root");
mysql_select_db("foobar");
$q = mysql_query("SELECT message FROM msg");

$trans = array_flip(array_merge(get_html_translation_table(HTML_ENTITIES), array(" " => "&nbsp;")));
$i=0;

while ($har = mysql_fetch_assoc($q)) {
    $zeilen = preg_split("/\. |! |\? |:-[)}\|(]/",
                    join(" ", 
                        split("\n", 
                            preg_replace(
                                array(  "/.*schrieb am.*/",
                                        "/^(>[ ]?)+/m",
                                        "/  /m"),
                                array(  "",
                                        "",
                                        ""),
                                strip_tags(strtr($har["message"], $trans))))));
    foreach($zeilen as $schwafel) {
        if (strlen($schwafel) > 200 || strlen($schwafel) < 3) continue;
        if (preg_match("/(gr(ue|ü|u)(ss|ß)(e)?)|cioa|mfg|cheers|cu/i", $schwafel)) continue;
        $muell[trim($schwafel)]=$i++; // Weia :-)
    }
}

foreach (array_flip($muell) as $weia) {
    echo "$weia.\n";
}
