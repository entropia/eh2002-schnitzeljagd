#!/usr/local/bin/perl -w

use News::NNTPClient;

$c = new News::NNTPClient("localhost", 1119);
@header = ("Newsgroups: de.org.blorg", "Subject: test", "From: tester");
@body   = ("This is the body of the article");

$c->post(@header, "", @body);

#($first, $last) = ($c->group("de.org.blorg"));
#
#for (; $first <= $last; $first++) {
#    print $c->article($first);
#    
#}

