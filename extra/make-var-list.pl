# Create variable list for DOSBox debugger. Place it directly to SWOS game directory.

$codeSWOS = 0x220000;
$dataSWOS = 0x2c1000;
$baseSWOSPP = 0x387000;

$SWOS = $ENV{'SWOS'} || 'd:\\games\\swos';
$SWOSPP = $ENV{'SWOSPP'} || 'f:\\swos';

# input
$mapSWOS = "$SWOS\\swos.map";
$mapSWOSPP = "$SWOSPP\\etc\\swospp_dbg.map";

# output
$varList = "$SWOS\\swospp.lst";

open MAP_S, "<$mapSWOS" or die "Can't open SWOS map file for reading.\n";
open MAP_SPP, "<$mapSWOSPP" or die "Can't open SWOS++ map file for reading.\n";
open VAR_LIST, ">$varList" or die "Can't open output file '$varList'.\n";

while (<MAP_S>) {
    if (/^\s+000(1|2):([a-fA-F0-9]{8})\s+([0-9a-zA-Z_]+)\s*$/) {
        # $1 - section, $2 - address, $3 - name of symbol
        printf VAR_LIST "%s %#08x\n", $3, hex($2) + ($1 eq '1' ? $codeSWOS : $dataSWOS);
    }
}

while (<MAP_SPP>) {
    if (/\s*(\w\w\w\w\w\w\w\w)\s+([a-zA-Z_][a-zA-Z_0-9]*)\s*\.\w+\s*(.*)/) {
        # $1 - address, $2 - name of symbol, $3 - origin object file
        if ($3 ne 'swos.obj') {
            $address = hex($1) - 0x401000 + $baseSWOSPP;
            printf VAR_LIST "%s %#08x\n", $2, $address;
        }
    }
}
