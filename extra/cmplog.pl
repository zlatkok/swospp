# Compare two SWOS++ log files and see if any frames differ in network game.
# Zlatko Karakas, 19.11.2013.

use List::Util qw[min max];

$logFile1 = "d:\\games\\swos\\swospp.log";
$logFile2 = "d:\\games\\swos3\\swospp.log";

@frames1 = parseLogFile($logFile1);
@frames2 = parseLogFile($logFile2);

compareFrames(\@frames1, \@frames2);

sub parseLogFile
{
    my @frames, $frameNo;
    open FILE, "<${_[0]}" or die "Can't open ${_[0]} for reading.\n";
    while (<FILE>) {
        chomp;
        $frameNo = $1 if (/currentFrameNo = (\d+)/);
        $frames[$frameNo]{'r'} = $2 if (/currentReceivedControls = (0x)?(\d+)/);
        $frames[$frameNo]{'s'} = $2 if (/Sending controls = (0x)?(\d+)/);
    }
    return @frames;
}

sub compareFrames
{
    my $diff = 0;
    my ($l, $r);
    for ($i = 0; $i < max($#{$_[0]}, $#{$_[1]}); $i++) {
        if (defined ${$_[0]}[$i]{'s'}) {
            $l = ${$_[0]}[$i]{'s'};
            $r = ${$_[1]}[$i]{'r'};
            $mismatch = $l != $r;
        } else {
            $l = ${$_[0]}[$i]{'r'};
            $r = ${$_[1]}[$i]{'s'};
            $mismatch = $l != $r;
        }
        if ($mismatch) {
            print "Mismatch at frame $i. ($l vs $r)\n";
            $diff = 1;
        }
    }
    if (!$diff) {
        print "Log files seem equivalent.\n";
    }
}