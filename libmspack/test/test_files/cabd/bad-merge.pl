#!/usr/bin/perl -w
use strict;

sub write_cab {
    my ($fname, $idx, $prev, $next, $folders, $blocks, $files) = @_;
    my $flags = $next ? ($prev ? 3 : 2) : ($prev ? 1 : 0);
    my $hdr = pack 'A4V5C2v5',
       'MSCF', 0, 0, 0,     # signature, 0, cabinet size (fixup), 0,
       0, 0, 3, 1,          # files offset (fixup), 0, format rev, format ver
       scalar(@$folders),   # number of folders
       scalar(@$files),     # number of files
       $flags, 12345, $idx; # flags, set id, set index
    $hdr .= pack 'Z*Z*', @$prev if $prev;
    $hdr .= pack 'Z*Z*', @$next if $next;

    # build all folders, files, datablocks, and fixup folders' block offset
    my $all_folders = '';
    my $all_files = join '', @$files;
    my $all_blocks = '';
    my $files_offset = length($hdr) + length(join '', @$folders);
    my $blocks_offset = $files_offset + length($all_files);
    for my $fi (0 .. $#$folders) {
        my $num_blocks = unpack 'v', substr $folders->[$fi], 0x04, 2;
        substr($folders->[$fi], 0x00, 4,
            pack 'V', $blocks_offset + length($all_blocks));
        $all_folders .= $folders->[$fi];
        for my $i (1 .. $num_blocks) {
            $all_blocks .= shift @$blocks;
        }
    }

    # fixup cabinet size and files offset
    substr($hdr, 0x08, 4, pack 'V', $blocks_offset + length($all_blocks));
    substr($hdr, 0x10, 4, pack 'V', $files_offset);

    if (open my $fh, '>', $fname) {
        print $fh $hdr . $all_folders . $all_files . $all_blocks;
        close $fh;
    }
}
sub folder {
    # data offset (fixup), number of blocks, compression method
    return pack 'Vvv', 0, $_[0], 0;
}
sub file {
    my ($len, $off, $fidx, $filename) = @_;
    # uncompressed size, folder offset, folder inde, time, date, attribs, filename
    return pack 'V2v4Z*', $len, $off, $fidx, 0x226C, 0x59BA, 0x20, $filename;
}
sub block {
    # checksum, compressed size, uncompressed size, data
    return pack "Vvva$_[0]", 0, $_[0], $_[0], '';
}
sub split_block {
    # checksum, compressed size, uncompressed size, data
    return pack "Vvva$_[0]", 0, $_[0], $_[1], '';
}

# writes two cabs to be merged, but the continued-to-next file has an invalid filename
write_cab('bad-merge1.cab', 0, undef, ["bad-merge2.cab", "BAD2"],
    [folder(1), folder(1)],
    [block(1), split_block(1,0)],
    [file(1, 0, 0, 'a'), file(2, 0, 0xFFFE, '')]); # second filename invalid (should be "b")
write_cab('bad-merge2.cab', 1, ["bad-merge1.cab", "BAD1"], undef,
    [folder(1), folder(1)],
    [split_block(1,2), block(1)],
    [file(2, 0, 0xFFFD, 'b'), file(1, 0, 1, 'c')]);

# writes two cabs to be merged, but the continued-from-prev file has an invalid filename
write_cab('bad-merge3.cab', 0, undef, ["bad-merge4.cab", "BAD4"],
    [folder(1), folder(1)],
    [block(1), split_block(1,0)],
    [file(1, 0, 0, 'a'), file(2, 0, 0xFFFE, 'b')]);
write_cab('bad-merge4.cab', 1, ["bad-merge3.cab", "BAD3"], undef,
    [folder(1), folder(1)],
    [split_block(1,2), block(1)],
    [file(2, 0, 0xFFFD, ''), file(1, 0, 1, 'c')]);  # first filename invalid (should be "b")

# file "a" continued-previous-and-next but cab has more than one folder
# (for continued-previous-and-next, cab can only have a single folder)
write_cab('bad_continue.cab', 0, undef, undef,
    [folder(1), folder(1)],
    [block(1), block(1)],
    [file(2, 0, 0xFFFF, 'a')]);
