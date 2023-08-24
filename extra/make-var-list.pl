# Create variable list for DOSBox debugger. Place it directly to SWOS game directory.

$codeSWOS = 0x220000;
$dataSWOS = 0x2c1000;
$baseSWOSPP = 0x387000;

$SWOS = $ENV{'SWOS_DIR'} || 'f:\\games\\swos';
$SWOSPP = $ENV{'SWOSPP'} || 'f:\\swos';

# input
$mapSWOS = "$SWOSPP\\mapcvt\\swos.map";
$mapSWOSPP = "$SWOSPP\\var\\swospp_dbg.map";

# output
$varList = "$SWOS\\swospp.lst";

open MAP_S, "<$mapSWOS" or die "Can't open SWOS map file for reading.\n";
open MAP_SPP, "<$mapSWOSPP" or die "Can't open SWOS++ map file for reading.\n";
open VAR_LIST, ">$varList" or die "Can't open output file '$varList'.\n";

my %symbols = ();

while (<MAP_S>) {
    # $1 - section, $2 - address, $3 - name of symbol
    if (/^\s+000(1|2):([a-fA-F0-9]{8})\s+([0-9a-zA-Z_]+)\s*$/) {
        printf VAR_LIST "%s %#08x\n", $3, hex($2) + ($1 eq '1' ? $codeSWOS : $dataSWOS);
        $symbols[$3] = 1;
    }
}

while (<MAP_SPP>) {
    # $1 - address, $2 - name of symbol, $3 - origin object file
    if (/\s*(\w\w\w\w\w\w\w\w)\s+([a-zA-Z_][a-zA-Z_0-9]*)\s*\.\w+\s*(.*)/) {
        # skip duplicate symbols, mostly those from swos object file
        if ($3 !~ /^swos\./ and not exists($symbols[$2])) {
            $address = hex($1) - 0x401000 + $baseSWOSPP;
            printf VAR_LIST "%s %#08x\n", $2, $address;
        }
    }
}
