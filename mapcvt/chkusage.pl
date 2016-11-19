# Check usage of symbols - find possibly unused ones.
# Keep in sync with filter.txt changes.

use File::Find::Rule;

# read in filter.txt first
open FILT, "<filter.txt" or die "Couldn't open filter file `filter.txt'.\n";

# form a list of allowed symbols
while (<FILT>) {
    next if /^\s*#/; # skip comments
    if (/^^\s*(\w+)(\s*,\s*prefix\s*(\w+))?/) {
        die "Duplicate symbol `$1' found.\n" if $symbols{$1};
        $symbols{$3 . $1} = 0;
    }
}
close FILT;

for my $file (File::Find::Rule->in('../main')) {
    next if $file !~ /\.asm$|\.inc$|\.c$|\.cpp|\.h$/;
    # skip generated files
    next if index($file, 'swos.asm') != -1 || index($file, 'swos.inc') != -1 || index($file, 'swossym.h') != -1;
    #print "processing $file...\n";
    open FILE, "<../main/$file" or die "Couldn't open file `$file'.\n";
    while (<FILE>) {
        chomp;
        my $line = $_;
        foreach $word (split /[^\w]/, $line) {
            $symbols{$word}++ if exists($symbols{$word});
        }
    }
    close FILE;
}

while (($key, $value) = each %symbols) {
    # don't complain about SWOS libc functions
    if ($value < 1 && index($key, 'swos_libc_') != 0) {
        print "Possible unused symbol: $key\n";
        $unused++;
    }
}

#print "$_: $symbols{$_}\n" for sort { $symbols{$a} <=> $symbols{$b} || $a <=> $b} keys %symbols;
print $unused ? "$unused possibly unused symbol(s) found.\n" : "It seems all symbols are being used.\n";
