#!/usr/bin/perl -w
use strict;

# reads filenames, from stdin or from files provided on the command ilne,
# writes a cabinet file with those filenames in it to stdout

sub u1($) { pack 'C', $_[0] }
sub u2($) { pack 'v', $_[0] }
sub u4($) { pack 'V', $_[0] }

# read filenames, one per line, from stdin or provided filename
my @files;
while (<>) {
  chomp;
  next if /^$/ or /^#/;
  push @files, $_;
}

die "no filenames" if @files == 0;
die "too many filenames" if @files > 65535;

my $header
  = 'MSCF'              # 00 signature
  . u4(0)               # 04
  . u4(0)               # 08 cabinet size (fixup later)
  . u4(0)               # 0C
  . u4(0)               # 10 files offset (fixup later)
  . u4(0)               # 14
  . u1(3)               # 18 cabinet file format minor version
  . u1(1)               # 19 cabinet file format major version
  . u2(1)               # 1A number of folders
  . u2(scalar @files)   # 1C number of files
  . u2(0)               # 1E flags
  . u2(1234)            # 20 cabinet set id
  . u2(0)               # 22 cabinet index
;

my $folders
  = u4(0)               # 00 data offset (fixup later)
  . u2(0)               # 04 number of blocks
  . u2(0)               # 06 compression method
;

my $files = '';
for (@files) {
  my $attribs = /[\x80-\xFF]/ ? 0xA0 : 0x20;
  $files
  .=u4(0)               # 00 uncompressed size
  . u4(0)               # 04 folder offset
  . u2(0)               # 08 folder index
  . u2(0x226C)          # 0A time
  . u2(0x59BA)          # 0C date
  . u2($attribs)        # 0E attribs
  . "$_\0"              # 10 file name
;
}

my $datablock
  = u4(0)               # 00 checksum
  . u2(0)               # 04 compressed size
  . u2(0)               # 06 uncompressed size
;

my $cablen
  = length($header)
  + length($folders)
  + length($files)
  + length($datablock);
;

# fixup cabinet size
substr($header, 0x08, 4, u4($cablen));

# fixup files offset
substr($header, 0x10, 4, u4(length($header) +
                            length($folders)));

# fixup datablock offsets
substr($folders, 0x00, 4, u4(length($header)  +
                             length($folders) +
                             length($files)));

# print cab file to stdout
print $header . $folders . $files . $datablock;
