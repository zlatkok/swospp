defined($ARGV[0]) or die "Usage $0 <bmp file> [transparent color] [generate header]\n";
$filename = shift;
$transparentColor = shift;
$generateHeader = shift;

open IN, "<$filename" or die "Can't open $filename for reading\n";
$filename =~ s/\.bmp$/\.bp/;
open OUT, ">$filename" or die "Can't open swos.asm for writing\n";
binmode IN;
binmode OUT;

seek IN, 10, 0;
read IN, $in, 40;
($ofs, $headerSize, $width, $height, $bitsPerPixel, $colorsUsed) =
    unpack "VVVVx2vx16V", $in;
$headerSize == 40 or die "Unsupported header type (size = $headerSize)!\n";
$bitsPerPixel == 8 or die "Only supporting 8-bit palette bitmaps.\n";
# the usual case with bitmaps from SWPE
!defined $transparentColor and $colorsUsed == 17 and $transparentColor = 16;
$padding = (4 - $width % 4) % 4;

seek IN, $ofs, 0;
# bitmaps are upside-down
for ($i = 0; $i < $height; $i++) {
    read IN, $line, $width;
    # convert transparent color to index 0
    if ($transparentColor != 0) {
        @bytes = map { $_ == $transparentColor and $_ = 0, 1 or !$_ and $_ = $transparentColor; $_ } unpack "C*", $line;
        $line = pack "C*", @bytes;
    }
    seek IN, $padding, 1;
    unshift @data, $line;
}

# header is just 2 bytes for width and height
if (defined $generateHeader) {
    $width < 256 and $height < 256 or die "Bitmap dimensions too large to fit in header values\n";
    syswrite OUT, pack("CC", $width, $height), 2;
}
for ($i = 0; $i < $height; $i++) {
    syswrite OUT, $data[$i], $width;
}

close IN;
close OUT;
print "RAW bitmap data extracted, offset: $ofs, dimensions: $width $height\n";