sub showUsage
{
    print "h2inc - C header file conversion utility\n";
    print "usage: h2inc.pl <header-file> [options]\n";
    print "Type h2inc.pl -h for help.\n";
    exit 1;
}

sub showHelp
{
    print "In progress.\n";
}

scalar @ARGV or showUsage;

open FILE, "<$ARGV[0]" or die "Couldn't open $ARGV[0] for reading.\n";
while (<FILE>) {
    #TODO: - pravi lexer
    #      - kompletno uklanjanje komentara (ili cak konverzija)
    #      - da ide rekurzivno i otvara includes, u slucaju da se koriste neke konstante,
    #        ali samo da izbacuje kod za trenutni modul
    #        (rekurzivna f-ja sa parametrom don't emmit)
    #      - kompletna podrska za izraze ukljucujuci i pokazivace
    #      - izracunavanje kompleksnih izraza
    #      - podrska za kompletni preprocesor
    #      - definisanje simbola iz komandne linije
    if ($in_comment && //) {
        ;
    }
    if /\s*#\s*define\s+(\w+)\s+(\w+)/ {
        ;
    }
    push @lines, $_;
}
close FILE;

print "@lines\n";