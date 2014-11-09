defined $ARGV[0] && defined $ARGV[1] or die "Usage $0 <file1> <file2>\n";
open F1, "<$ARGV[0]" or die "Can't open $ARGV[0] for reading.\n";
open F2, "<$ARGV[1]" or die "Can't open $ARGV[1] for reading.\n";
binmode F1;
binmode F2;
seek F1, 0, 2;
$size = tell F1;
seek F1, 0, 0;
seek F2, 0, 2;
$size == tell F2 or die "Files must be of equal size!\n";
seek F2, 0, 0;
$ofs = -1;
while (defined($c1 = getc F1)) {
    $ofs++;
    $c2 = getc F2;
    next if ord $c1 == ord $c2;
    $startofs = $ofs;
    $len = 0;
    do {
        $len++;
        push @orig_data, ord $c1;
        push @patch_data, ord $c2;
        $c1 = getc F1;
        $c2 = getc F2;
        $ofs++;
    } while (defined $c1 && ord $c1 != ord $c2);
    push @offsets, $startofs;
    push @lengths, $len;
}

print "offsets:\n    dd ";
for ($i = 0, $printed = 7; $i < @offsets; $i++) {
    $comma = $printed + 6 > 67 ? '' : ',';
    if ($printed + 12 > 79) {
        print "\n    dd ";
        $printed = 7;
        $comma = ',';
    }
    printf "0x%08x%s", $offsets[$i], $i == $#offsets ? '' : $comma;
    print ' ' if $printed + 24 <= 79;
    $printed += 12;
}
print "\noffsets_end equ \$\n\nlengths:\n    db ";
for ($i = 0, $printed = 7; $i < @lengths; $i++) {
    $comma = $printed + 6 > 73 ? '' : ',';
    if ($printed + 6 > 79) {
        print "\n    db ";
        $printed = 7;
        $comma = ',';
    }
    printf "0x%02x%s", $lengths[$i], $i == $#lengths ? '' : $comma;
    print ' ' if $printed + 12 <= 79;
    $printed += 6;
}
print "\n\norig_data:\n    db ";
for ($i = 0, $printed = 7; $i < @orig_data; $i++) {
    $comma = $printed + 6 > 73 ? '' : ',';
    if ($printed + 6 > 79) {
        print "\n    db ";
        $printed = 7;
        $comma = ',';
    }
    printf "0x%02x%s", $orig_data[$i], $i == $#orig_data ? '' : $comma;
    print ' ' if $printed + 12 <= 79 && $i != $#orig_data;
    $printed += 6;
}
print "\n\npatch_data:\n    db ";
for ($i = 0, $printed = 7; $i < @patch_data; $i++) {
    $comma = $printed + 6 > 73 ? '' : ',';
    if ($printed + 6 > 79) {
        print "\n    db ";
        $printed = 7;
        $comma = ',';
    }
    printf "0x%02x%s", $patch_data[$i], $i == $#patch_data ? '' : $comma;
    print ' ' if $printed + 12 <= 79 && $i != $#patch_data;
    $printed += 6;
}