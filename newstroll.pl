#!/usr/bin/perl -w
use News::NNTPClient;
use Text::Format;
use Date::Parse;
use strict;

my $c = new News::NNTPClient("172.16.2.72", 1119);

open(TROLL, $ARGV[0]) or die("kann $ARGV[0] nicht finden");
my (@getrolle, @trolltopic);
while (<TROLL>) {
    chomp $_;
    push @getrolle, $_;
    push @trolltopic, $_ if (/^(\w+[. ]){2,4}$/);
}
close TROLL;


open(NAMEN, "namen") or die("ups. namen daten fehlt");
my (@vornamen, @nachnamen);
while (<NAMEN>) {
    if (/([a-zA-ZüöäüÜÖÄ]*) ([a-zA-ZüöäüÜÖÄ]*)/) {
        push @vornamen, $1;
        push @nachnamen, $2;
    }
}
close NAMEN;

sub funnyname {
    return $vornamen[rand @vornamen]." ".$nachnamen[rand @nachnamen]." <entropia\@harhar.invalid>";
}

sub muell {
    my $text = Text::Format->new({columns => 72, firstIndent=>0});
    my $num = rand 3;
    my $muell="";
    for (my $i = 0; $i < $num; $i++) {
        $muell.=$getrolle[rand @getrolle]." ";
    }
    return split /\n/, $text->format($muell); 
}

my ($first, $last) = ($c->group("de.org.troll"));


$c->post("Subject: ".$trolltopic[rand @trolltopic],
         "From: ".funnyname, 
         "Newsgroups: de.org.troll",
         "",
         muell);
print "Trolle mit neuer Nachricht\n";

for (my $cur = $first; $cur <= $last; $cur++) {

    my $inbody = 0;
    my %headers;
    my @body = ();
    
    foreach ($c->article($cur)) {
        if (!$inbody) {
            if    (/^([^:]+): (.*)$/)   { $headers{lc $1}=$2; }
            elsif (/^$/)                { $inbody = 1; }
            # leere Headerzeilen ignorieren
        } else {
            push @body, $_;
        }
    }

    next if (!defined($headers{"date"}));
    my $alter = time - str2time($headers{"date"});
    next if ($alter < 0 || $alter > 60*15); 
    next if (rand() < 0.5);

    my @replyheader = ("Subject: Re: ".$headers{"subject"}, 
                       "From: ".funnyname, 
                       "Newsgroups: ".$headers{"newsgroups"},
                       "References: ".$headers{"message-id"},
                       "X-MSMail-Priority: Normal",
                       "X-Newsreader: Microsoft Outlook Express 5.00.2919.6700",
                       "X-MimeOLE: Produced By Microsoft MimeOLE V5.00.2919.6700");

    my @replybody = ( $headers{"from"}." schrieb: ", "");
    my $hadtext = 0;
    my $madecomment = 0;
    foreach (@body) {
        chomp;
        if      (/^$/ && $hadtext)  {
            push @replybody, "";
            push @replybody, muell;
            push @replybody, "";
            $hadtext = 0;
            $madecomment = 1;
        } elsif (/^> (.*)/) {
            # gequoteten schrott weglassen
        } elsif (/^.*<.*> schrieb:/) {
            # einleitungszeile nicht kommentieren
        } else {
            # normalen text quoten, falls zeile länger als 2 zeichen
            if (length >= 2) {
                push @replybody, "> $_";
                $hadtext = 1;
            }
        }
    }
    if (!$madecomment) {
        push @replybody, "";
        push @replybody, muell;
    }

    print "Trolle als Antwort auf ".$headers{"from"}."\n";
    $c->post(@replyheader, "", @replybody);
#    print "------\n";
#    print join "\n", @replyheader, "", @replybody;
#    print "\n------\n";
}



