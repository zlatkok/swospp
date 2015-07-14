# Compare two SWOS++ log files and see if any frames differ in network game.
#
# Zlatko Karakas, 19.11.2013.

use List::Util qw[min max];

$logFile1 = "d:\\games\\swos\\swospp.log";
$logFile2 = "d:\\games\\swos3\\swospp.log";

@frames1 = parseLogFile($logFile1, 1);
@frames2 = parseLogFile($logFile2, 2);

#print "frames1\n";
#for ($i = 0; $i <= $#frames1; $i++) {
#    print "$i r = '", $frames1[$i]{'r'}, "', s = '", $frames1[$i]{'s'}, "'\n";
#}
#print "frames2\n";
#for ($i = 0; $i <= $#frames1; $i++) {
#    print "$i r = '", $frames1[$i]{'r'}, "', s = '", $frames1[$i]{'s'}, "'\n";
#}
compareFrames(\@frames1, \@frames2);

sub parseLogFile
{
    my @frames, $frameNo;
    open FILE, "<${_[0]}" or die "Can't open ${_[0]} for reading.\n";
    while (<FILE>) {
        chomp;
        $frameNo = $1 if (/currentFrameNo = (\d+)/);
        $frames[$frameNo]{'r'} = $2 if (/currentReceivedControls = (0x)?(\d+)/);
        $frames[$1]{'s'} = $3 if (/Sending frame (\d+), controls = (0x)?(\d+)/);
        if (/^\[/ && $#currentTeamBytes + 1 > 0) {
            # remove fields: opponent team pointer and player number - that's always different,
            # is-a-player-coach, pointers to in-game structure and statistics
            splice @currentTeamBytes, 0, 18;
            $frames[$frameNo]{$currentTeam} = [@currentTeamBytes];
            @currentTeamBytes = ();
            $currentTeam = '';
        }
        if ($currentTeam ne '' && /^\s*\w+\s+((\w\w)\s+)+/) {
            @tokens = split ' ';
            shift @tokens;
            pop @tokens;
            push @currentTeamBytes, @tokens;
        }
        if (/Hex dump of (\w+) team/) {
            $currentTeam = $1;
        }
    }
    return @frames;
}

sub compareFrames
{
    my $diff = 0;
    my ($l, $r);
    for ($i = 0; $i < max($#{$_[0]}, $#{$_[1]}); $i++) {
        $mismatch = 0;
        if (defined ${$_[0]}[$i]{'s'}) {
            $l = ${$_[0]}[$i]{'s'};
            $r = ${$_[1]}[$i]{'r'};
            $mismatch = $l != $r;
        } else {
            $l = ${$_[0]}[$i]{'r'};
            $r = ${$_[1]}[$i]{'s'};
            $mismatch = $l != $r;
        }
        if ($_[0]->[$i]{'left'} && $_[1]->[$i]{'left'} &&
            $_[0]->[$i]{'right'} && $_[1]->[$i]{'right'}) {
            $orgMismatch = $mismatch;
            for ($j = 0; $j < scalar @{$_[0]->[$i]{'left'}}; $j++) {
                if ($_[0]->[$i]{'left'}->[$j] != $_[1]->[$i]{'left'}->[$j]) {
                    print "Difference in left team, at offset " . ($j + 18) . ".\n";
                    $mismatch++;
                }
                if ($_[0]->[$i]{'right'}->[$j] != $_[1]->[$i]{'right'}->[$j]) {
                    print "Difference in right team, at offset " . ($j + 18) . ".\n";
                    $mismatch++;
                }
            }
            if ($mismatch > $orgMismatch) {
                print "P1L: @{$_[0]->[$i]{'left'}}\n";
                print "P2L: @{$_[1]->[$i]{'left'}}\n";
                print "P1R: @{$_[0]->[$i]{'right'}}\n";
                print "P2R: @{$_[1]->[$i]{'right'}}\n";
            }
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