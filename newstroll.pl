#!/usr/local/bin/perl -w

use News::NNTPClient;
use strict;

my $c = new News::NNTPClient("localhost", 1119);

my @getrolle = ();
while (<STDIN>) {
    chomp $_;
    push @getrolle, $_;
}

#print $getrolle[int rand $#getrolle];


my ($first, $last) = ($c->group("de.org.troll"));

for (my $cur = $first; $cur <= $last; $cur++) {

    next if (rand() < 0.5);
    my $inbody = 0;
    my %headers;
    my @body = ();
    
    foreach ($c->article($cur)) {
        if (!$inbody) {
            if    (/^([^:]+): (.*)$/)   { $headers{lc $1}=$2; }
            elsif (/^$/)                { $inbody = 1; }
            else                        { print "huch? $_ passt nicht ins konzept\n"; }
        } else {
            push @body, $_;
        }
    }

    my @replyheader = ("Subject: Re: ".$headers{"subject"}, 
                       "From: Trollsepp <blorg\@this.is.invalid>",
                       "Newsgroups: ".$headers{"newsgroups"},
                       "References: ".$headers{"message-id"});
    my @replybody = ();
    my $hadtext = 0;
    my $madecomment = 0;
    foreach (@body) {
        chomp;
        if      (/^$/ && $hadtext)  {
            push @replybody, "";
            my $num = rand 10;
            my $muell="";
            for (my $i = 0; $i < $num; $i++) {
                $muell.=$getrolle[int rand $#getrolle]." ";
            }
            push @replybody, $muell;
            push @replybody, "";
            $hadtext = 0;
            $madecomment = 1;
        } elsif (/^> (.*)/) {
            # gequoteten schrott weglassen
        } else {
            # normalen text quoten
            push @replybody, "> $_";
            $hadtext = 1;
        }
    }
    if (!$madecomment) {
        push @replybody, "";
        push @replybody, $getrolle[int rand $#getrolle];
        push @replybody, $getrolle[int rand $#getrolle];
    }

    print "Trolle als Antwort auf ".$headers{"from"}."\n";
    $c->post(@replyheader, "", @replybody);
}




#@header = ("Newsgroups: de.org.blorg", "Subject: test", "From: tester");
#@body   = ("This is the body of the article");

#$c->post(@header, "", @body);

#($first, $last) = ($c->group("de.org.blorg"));
#
#for (; $first <= $last; $first++) {
#    print $c->article($first);
#    
#}

