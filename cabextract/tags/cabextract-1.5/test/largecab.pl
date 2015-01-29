#!/usr/bin/perl -w
use strict;

if ($#ARGV != 2 ||
    (int($ARGV[1]) <= 0) || (int($ARGV[1]) > 65535) ||
    (int($ARGV[2]) <= 0) || (int($ARGV[2]) > 65535))
{
  die <<EOF;
Usage: $0 <filename> <blocks1> <blocks2>
This generates a CAB file with two files in it, where the first file
is 32768*blocks1 bytes long and the second file is 32768*blocks2 bytes
long. Neither file can be more than 65535*32768 bytes long.
EOF
}

my $filename = $ARGV[0];
my $blocks1  = int($ARGV[1]);
my $blocks2  = int($ARGV[2]);

sub u1($) { pack 'C', $_[0] }
sub u2($) { pack 'v', $_[0] }
sub u4($) { pack 'V', $_[0] }

my $header
  = 'MSCF'              # 00 signature
  . u4(0)               # 04
  . u4(0)               # 08 cabinet size (fixup later)
  . u4(0)               # 0C
  . u4(0)               # 10 files offset (fixup later)
  . u4(0)               # 14
  . u1(3)               # 18 cabinet file format minor version
  . u1(1)               # 19 cabinet file format major version
  . u2(2)               # 1A number of folders
  . u2(2)               # 1C number of files
  . u2(0)               # 1E flags
  . u2(1234)            # 20 cabinet set id
  . u2(0)               # 22 cabinet index
;

my $folders
  = u4(0)               # 00 data offset (fixup later)
  . u2($blocks1)        # 04 number of blocks
  . u2(0)               # 06 compression method
  . u4(0)               # 08 data offset (fixup later)
  . u2($blocks2)        # 0C number of blocks
  . u2(0)               # 0E compression method
;

my $files
  = u4($blocks1*32768)  # 00 uncompressed size
  . u4(0)               # 04 folder offset
  . u2(0)               # 08 folder index
  . u2(0x226C)          # 0A time
  . u2(0x59BA)          # 0C date
  . u2(0x20)            # 0E attribs
  . "test1.bin\0"       # 10 file name

  . u4($blocks2*32768)  # uncompressed size
  . u4(0)               # folder offset
  . u2(1)               # folder index
  . u2(0x226C)          # time
  . u2(0x59BA)          # date
  . u2(0x20)            # attribs
  . "test2.bin\0"       # file name
;

my $datablock
  = u4(0)              # 00 checksum
  . u2(32768)          # 04 compressed size
  . u2(32768)          # 06 uncompressed size
  . ((pack 'C*', 0 .. 255) x 128)  # 08: actual data (interesting pattern)
;

my $cablen
  = length($header)
  + length($folders)
  + length($files)
  + (length($datablock) * $blocks1)
  + (length($datablock) * $blocks2)
;

die "Overall cabinet is too large ($cablen > 4294967295)\n"
  if $cablen > 4294967295;

# fixup cabinet size
substr($header, 0x08, 4, u4($cablen));

# fixup files offset
substr($header, 0x10, 4, u4(length($header) +
			    length($folders)));

# fixup folder data offsets
substr($folders, 0x00, 4, u4(length($header)  +
			     length($folders) +
			     length($files)));

substr($folders, 0x08, 4, u4(length($header)  +
			     length($folders) +
			     length($files)   +
			     (length($datablock) * $blocks1)));

if (open FH, ">$filename") {
  print FH $header . $folders . $files;
  for (1 .. $blocks1) {
    print FH $datablock;
  }
  for (1 .. $blocks2) {
    print FH $datablock;
  }
  close FH;
}
else {
  die "Can't write to $filename - $!";
}
