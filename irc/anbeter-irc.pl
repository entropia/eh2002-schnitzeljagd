#!/usr/bin/perl -w

use Net::IRC;
use strict;

my @names = qw(dividuum manuel The big time sink on this project Each
      Connection instance is a single connection to an IRC server The
      module itself contains methods for every);

sub anbeternick {
   return $names[rand $#names + 1];
}

sub anbeter {
   my $irc = Net::IRC->new;
   $irc->debug(1);

   print anbeternick."\n";
   my $conn = $irc->newconn(
         Nick   => "blorg",
         Server => '192.168.23.2');

   if (!defined($conn)) {
      return,
   }

   for ([nicknameinuse => sub {
            my $conn = shift;
            my $nick = $conn->nick;
            $nick = anbeternick;
            $conn->nick($nick);
         }],
         [public => sub {
            my ($conn, $event) = @_;
            my ($msg) = $event->args;
            print "$msg\n";
         }],
         [endofmotd => sub {
            my $conn = shift;
            $conn->join("#entr0pia");
         }],) {
      $conn->add_global_handler(@$_);
   }

   $irc->start;
}

for (1 .. 1) {
   my $pid; 

   next if $pid = fork;
   next unless defined $pid;

   anbeter;
   exit
}
