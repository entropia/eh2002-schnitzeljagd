#!/usr/bin/perl -w

use strict;
use IO::Socket;
use POSIX qw(:sys_wait_h);
use MIME::Base64;

my $diary_port = 2323;
my $diary_exp = 100000000;
my $client;

# XXX
my ($client_authenticated,
    $client_challenge) = (1, int(rand $diary_exp));
my $i = 1;
my (@diary_titles, @diary_msgs);

while (open(MSG, "./config/spert/diary/entries/$i")) {
    if (defined(my $line = <MSG>)) { 
        chomp $line; push @diary_titles, $line; 
    }

    my $msg ="";
    $msg.=$_ while (<MSG>);

    push @diary_msgs, $msg; 
    
    close MSG;
    $i++;
}

sub REAPER {
   1 until (-1 == waitpid(-1, WNOHANG));
   $SIG{CHLD} = \&REAPER;                 # unless $] >= 5.002
}

sub ack {
   $client->send("ACK: ".(shift)."\n");
}

sub nack {
   $client->send("NACK: ".(shift)."\n");
}

sub diary_banner {
   ack("DIARY PROTOCOL SERVER (1.0) ready (try HELP)");
}

sub diary_help {
   ack("HELP CHALLENGE RESPONSE LIST GET QUIT AUTH");
}

sub diary_challenge {
   $client_challenge = int(rand $diary_exp) << 8;
   ack("CHALLENGE $client_challenge");
}

# perl -MMIME::Base64 -p -e 'chomp; print encode_base64($_ + 1)'
sub diary_response {
   my $resp = shift;
   if (!defined($resp)) {
      nack("RESPONSE response");
      return;
   }

   my $str = encode_base64($client_challenge + 1);
   chomp($str);
   if (!($str eq $resp)) {
      nack("RESPONSE $str");
   } else {
      ack("RESPONSE $str");
      $client_authenticated = 1;
   }
}

sub diary_list {
   if (!$client_authenticated) {
      nack("!AUTH");
   } else {
      my $i = 0;
      foreach my $msg (@diary_titles) {
         $i++;
         ack("LIST $i $msg");
      }
   }
}

sub diary_get {
   my ($num) = @_;

   if (!$client_authenticated) {
      nack("!AUTH");
   } elsif (!defined($num)) {
      nack("GET num");
   } elsif (($num <= $#diary_msgs + 1) &&
            ($num > 0)) {
      ack("GET $num\n".$diary_msgs[$num-1]);
   } else {
      ack("GET <> !");
   }
}

sub diary_entropia {
   ack("entr0pia (c) 2002 dividuum manuel");
}

sub diary_system {
   ack("HEILIGER SCHEDULER");
}

sub diary_gehsterben {
   ack("hi neingeist");
}

my %diary_handlers = (
   "HELP"       => \&diary_help,
   "CHALLENGE"  => \&diary_challenge,
   "RESPONSE"   => \&diary_response,
   "AUTH"       => \&diary_response,
   "LIST"       => \&diary_list,
   "GET"        => \&diary_get,
   "entr0pia"   => \&diary_entropia,
   "SYSTEM"     => \&diary_system,
   "GEHSTERBEN" => \&diary_gehsterben,
);

sub diary {
   my $str;

   diary_banner();

   while (defined($client->recv($str, 512))) {
      chomp($str);
      my ($cmd, @args) = split /\s+/, $str;
      
      next unless (defined($cmd));
      if ($cmd eq "QUIT") {
         return;
      }

      if (defined($diary_handlers{$cmd})) {
         $diary_handlers{$cmd}->(@args);
      } else {
         $client->send("NACK PROTOCOL MISMATCH\n");
      }
   }

   print "return\n";
}

$SIG{CHLD} = \&REAPER;

my $server = IO::Socket::INET->new(LocalPort => $diary_port,
                                   Type      => SOCK_STREAM,
                                   Reuse     => 1,
                                   Listen    => 255)
or die "Couldn't be a tcp server on port $diary_port : $@\n";

while ($client = $server->accept()) {
   my $pid;

   #die "fork: $!" unless defined $pid;     # failure

   next if $pid = fork;                    # parent
   next unless defined $pid;
   diary;
   $client->close;
   exit;                                   # child leaves
} continue {
   $client->close;
}
