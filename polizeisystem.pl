#!/usr/bin/perl -w

use strict;
use IO::Socket;
use POSIX qw(:sys_wait_h);

my $client;

sub REAPER {
   1 until (-1 == waitpid(-1, WNOHANG));
   $SIG{CHLD} = \&REAPER;                 # unless $] >= 5.002
}

sub cprint {
   $client->send((shift)."\n");
}

sub nichtfertig {
   return 1;
}

@


sub mainmenu {
   my $str;
   my $nichtfertig = 1;
   while ($nichtfertig) {
      sleep(1);
      cprint("");
      cprint("Hauptmenü");
      cprint("1 - Hinweis eintragen");
      cprint("2 - Hinweise lesen");
      cprint("3 - Einloggen");
      cprint("");
      last unless (defined($client->recv($str, 512)));
      $str =~ s/\s+$//;
      if    ($str eq "1") { eintragen; }
      elsif ($str eq "2") { auflisten; }
      elsif ($str eq "3") { login;     }
      elsif ($str eq "3") { last;    }
      else  { cprint <<EOT;

   POLIZEI.EXE caused a fatal exception
   0E at memory address: 645A:74C9.
   All unsaved work will be lost.

   The Crash Wizard will now guide you 
   through the rest of the rebooting process.

   -> Click here to return to the Classic
   Blue Screen Of Death error message.
EOT
         last;
      }
   }
}

sub welcome {
   cprint("Willkommen auf dem Polizeisystem [version 0.1]");
   mainmenu;
   print "return\n";
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

#   next if $pid = fork;                    # parent
#   next unless defined $pid;
   welcome;
   $client->close;
   exit;                                   # child leaves
} continue {
   $client->close;
}
