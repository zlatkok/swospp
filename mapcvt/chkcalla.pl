# check for errors with calla, for functions called through a register that is
# expected to hold parameter

%funcParams = (
    "strlen"    => 1,
    "strcat"    => 2,
    "strcpy"    => 2,
    "strncpy"   => 3,
    "memcpy"    => 3,
    "memmove"   => 3,
    "strcmp"    => 2,
    "ctime"     => 1,
    "time"      => 1,
);

@regs = ('eax', 'edx', 'ebx', 'ecx', 'esi', 'edi');

opendir DIR, '../main' or die "Can't open directory.";
$errors = 0;
while (defined($file = readdir(DIR))) {
    next if $file !~ /\.asm$/;
    #print "processing $file...\n";
    open FILE, "<../main/$file" or die "Couldn't open file `$file'.\n";
    $lineNo = 0;
    while (<FILE>) {
        chomp;
        my $line = $_;
        $lineNo++;
        if ($line =~ /\s*calla\s+(\w+)(\s*,\s+(\w+))?/) {
            my $reg = $3 || 'eax';
            if (substr($1, 0, 13) eq "swos_libc_str") {
                my $fn = substr(substr($1, 10), 0, -1);
                my $numParams = $funcParams{$fn};
                if ($numParams) {
                    for (my $i = 0; $i < $numParams; $i++) {
                        if ($reg eq $regs[$i]) {
                            print "$file:$lineNo Invalid call to function $fn through $reg register!\n";
                            $errors++;
                            last;
                        }
                    }
                }
            }
        }
    }
    close FILE;
}
close DIR;
print "Everything seems all right.\n" if !$errors;