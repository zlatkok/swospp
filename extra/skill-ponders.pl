# Determine minimum and maximum value to multiply player skills with during price determination.

@priceSkillPonders = (
    0x00005020, 0x00024010, 0x00034000, 0x00024010, 0x00005020, 0x01003030, 0x01013020,
    0x00024010, 0x01013020, 0x01003030, 0x02002030, 0x02013010, 0x01013000, 0x02013010,
    0x02002030, 0x02010130, 0x02013000, 0x03011000, 0x02013000, 0x02010130, 0x02001230,
    0x03000220, 0x04100200, 0x03000220, 0x02001230, 0x03000240, 0x02210220, 0x03230312,
    0x02210220, 0x03000240, 0x03000330, 0x02200221, 0x01220124, 0x02200221, 0x03000330,
);

@skillValuePonders = (
    0, 1, 3, 6, 10, 15, 21, 28
);

$globalMin = ~0;
$globalMax = 0;
foreach (@priceSkillPonders) {
    my $min = 0;
    my $max = 0;
    foreach my $i (0..6) {
        $skillValuePonder = $skillValuePonders[$_ & 0x0f];
        $min += $skillValuePonder * 1;    # min and max values of player skills
        $max += $skillValuePonder * 8;
        $_ >>= 4;
    }
    $min < $globalMin && ($globalMin = $min);
    $max > $globalMax && ($globalMax = $max);
}
print "Minimum value: $globalMin, maximum value: $globalMax\n";