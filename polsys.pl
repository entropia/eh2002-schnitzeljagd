#!/usr/bin/perl -w

use strict;
use IO::Socket;
use POSIX qw(:sys_wait_h);

my $client;

sub REAPER {
   #until (-1 == waitpid(-1, WNOHANG));
   waitpid(-1, WNOHANG);
   $SIG{CHLD} = \&REAPER;                 # unless $] >= 5.002
}

sub cout {
   $client->send((shift));
}

sub gpf {
   cout <<EOT;

  POLIZEI.EXE caused a fatal exception
  0E at memory address: 645A:74C9.
  All unsaved work will be lost.

  The Crash Wizard will now guide you 
  through the rest of the rebooting process.

  -> Click here to return to the Classic
  Blue Screen Of Death error message.

EOT
}

sub drinmsg {
   cout <<EOT;

   MOTD:
   --------------------------------------
   Wir haben wie bereits gestern angekündigt
   auf das neue Polizeisystem umgestellt.
   Mit dem alten System ist nur noch beschränkter
   Zugriff auf Informationen möglich.
   --------------------------------------

EOT
}

sub saveentry ($) {
   # Igittigitt. Bitte wegschauen. Ich hatte keine Lust mehr :-)
   my @hinweise;
   cout("Gespeicherte Einträge\n");
   open(HINWEISE, "polsys.txt") or return;
   while (<HINWEISE>) { chomp($_); push @hinweise, $_; }
   close HINWEISE;

   # In hinweise eintragen
   chomp($_[1]);
   shift @hinweise if (@hinweise >= 20);
   push @hinweise, $_[1];
   
   # Na und wenn schon... solls halt racen
   open(HINWEISE, ">polsys.txt") or return;
   foreach my $eintrag (@hinweise) {
      print HINWEISE $eintrag."\n";
   }
   close HINWEISE;

   cout("\n");
   sleep(1);
   cout("'".$_[1]."' wurde gespeichert\n\n");
   
   $_[0]="main"; # XXX: wow. hübsch  
} 

sub readentries {
   my @hinweise;
   cout("Gespeicherte Einträge\n");
   open(HINWEISE, "polsys.txt") or return;
   while (<HINWEISE>) { chomp($_); push @hinweise, $_; }
   close HINWEISE;
   print "foo\n";
   
   foreach my $eintrag (@hinweise) {
      # You can effect a sleep of 250 milliseconds this way:
      select(undef, undef, undef, 0.1);
      cout(" - ".$eintrag."\n");
   }
}

sub handlepasswd ($) {
   cout("Überprüfe Passwort\n");
   sleep(1);
   if (length $_[1] > 10) {
      sleep(1);
      gpf;
      $_[0]="inside";
   } else {
      cout("\n");
      sleep(1);
      cout("Anmeldung fehlgeschlagen\n\n");
      sleep(1);
      $_[0]="main";
   }
}


my %config = (
   "start" => {
      "output" => { "text" => "Willkommen auf dem Polizeisystem [version 0.1]\n\n"},
      "next"   => "main"},
   "main" => {
      "output" => { "text" => "Hauptmenü\n".
                              "\n".
                              "1 - Hinweise Eintragen\n".
                              "2 - Hinweise lesen\n".
                              "3 - Login\n".
                              "4 - Beenden\n"},
      "input"  =>[{ "regex" => "1",  "next" =>  "input"},
                  { "regex" => "2",  "next" =>  "read"},
                  { "regex" => "3",  "next" =>  "login"},
                  { "regex" => "4",  "next" =>  "quit"},
                  { "regex" => ".*", "next" =>  "quit", "func"  =>  \&gpf }]},
   "input" =>  {
      "output" => { "text"  => "Hinweis eintragen:\n\nHinweis:\n\n"},
      "input"  =>[{ "regex" => ".*", "next" =>  "main",     "func"  =>  \&saveentry }]},
   "read" => {
      "output" => { "func"  => \&readentries },
      "next"   =>  "main" },
   "quit" => {
      "output" => { "text"  => "Vielen Dank für ihre Kooperation\n" },
      "next"   => "finished"},
   "login"=> {
      "output" => { "text"  => "Login:\n\nBitte Zugangspasswort eingeben:\n\n"},
      "input"  =>[{"regex"  => ".*", "func" =>  \&handlepasswd}]},
   "inside"=>{
      "output" => { "func"  => \&drinmsg },
      "next"   => "insidemenu"},
   "insidemenu" => {
      "output" => { "text" => "Informationsystem Hauptmenü\n".
                              "\n".
                              "1 - MOTD wiederholen\n".
                              "4 - Abmelden\n"},
      "input"  =>[{ "regex" => "1",  "next" =>  "inside"},
                  { "regex" => "4",  "next" =>  "main"},
                  { "regex" => ".*", "next" =>  "main", "func"  => \&gpf}]});
   
     

sub run {
   my $str;
   my $state = "start";
   while ($state ne "finished") {
      sleep(1);
      if (!defined($config{$state})) { die "upsa. $state undefined"; }

      my %cs = %{$config{$state}};
      if (defined($cs{"output"})) {
         cout($cs{"output"}{"text"}) if (defined($cs{"output"}{"text"}));
         $cs{"output"}{"func"}->()   if (defined($cs{"output"}{"func"}));
      }

      if (defined($cs{"input"})) {
         cout("> ");
         last unless defined($client->recv($str, 512)); 
         $str =~ s/\s+$//;
         foreach my $input (@{$cs{"input"}}) {
            if ($str =~ /^$input->{"regex"}$/) {
               $input->{"func"}->($state, $str)    if (defined($input->{"func"}));
               $state = $input->{"next"}           if (defined($input->{"next"}));
               last;
            }
         }
      }
      $state = $cs{"next"}  if (defined($cs{"next"}));
   }
   

}

$SIG{CHLD} = \&REAPER;

if (@ARGV != 2) {
    print "polizeisystem.pl <bindaddr> <port>\n";
    exit 1;
}

my $server = IO::Socket::INET->new(LocalAddr => $ARGV[0],
                                   LocalPort => $ARGV[1],
                                   Type      => SOCK_STREAM,
                                   Reuse     => 1,
                                   Listen    => 255)
or die "Couldn't be a tcp server: $@\n";

while ($client = $server->accept()) {
   my $pid;

   #die "fork: $!" unless defined $pid;     # failure

   next if $pid = fork;                    # parent
   next unless defined $pid;
   run;
   $client->close;
   exit;                                   # child leaves
} continue {
   $client->close;
}
