#!/usr/bin/perl -w
use strict;

# writes 2 file entries where they're expected to be (immediately after folders)
# but also writes 2 more file entries, pointed to by files offset

my $header = pack 'A4V5C2v5',
   'MSCF', 0, 0, 0,   # signature, 0, cabinet size (fixup), 0,
   0, 0, 3, 1,        # files offset (fixup), 0, format rev, format ver
   1,                 # number of folders
   2,                 # number of files
   0, 1234, 0;        # flags, set id, set index

my $folder = pack 'Vvv',
    0, # data offset (fixup)
    0, # number of data blocks
    0; # compression method

my @files = map {
    pack 'V2v4Z*',
        0,         # uncompressed size
        0,         # folder offset
        0,         # folder index
        0x226C,    # time
        0x59BA,    # date
        0x20,      # attribs
        $_; # filename
} qw(normal1.txt normal2.txt hidden1.txt hidden2.txt);

my $datablock = pack 'Vvv',
    0, # checksum
    0, # compressed size
    0; # uncompressed size

# fixup offsets
my $files_offset  = length($header) + length($folder);
my $hidden_offset = $files_offset + length(join '', @files[0..1]);
my $blocks_offset = $files_offset + length(join '', @files);
my $cab_length    = $blocks_offset  + length($datablock);
substr($header, 0x08, 4, pack 'V', $cab_length);
substr($header, 0x10, 4, pack 'V', $hidden_offset); # point at @files[2..3]
substr($folder, 0x00, 4, pack 'V', $blocks_offset);

# print cab file to stdout
print $header, $folder, @files, $datablock;
