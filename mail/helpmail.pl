#!/usr/bin/perl -w

use Mail::Internet;
use Net::SMTP;
use strict;

my @hilfemails = ();

while (my $file = shift @ARGV) {
   print "reading from $file\n";
   open (HILFEMAIL, "<$file") || next;

   my @mail = ();

   while (<HILFEMAIL>) {
      push @mail, $_;
   }
   push @hilfemails, \@mail;
   close HILFEMAIL;
}

#foreach my $mail (@hilfemails) {
#   print "mail:\n";
#   print join("", @{$mail})."\n";
#}

while (<>) {
   chomp;
   my $ip = $_;

   my $mail = new Mail::Internet($hilfemails[rand @hilfemails]);

   print "sending to $ip\n";
   $mail->print && print "\n";

   $mail->smtpsend(( Host => "$ip", 
                     To   => "postmaster\@localhost"));
}

