/** DrawSprite

    saveSprite = -1 not saving, anything else to save background

    Route it to SWOS' DrawSprite, skip over reading of sprite index.

    Important notice: DrawSprite16Pixels doesn't do proper clipping by x, it simply wraps it around. During
    the game screen has unused 64 pixels in each line, so it appears to clip properly - but that will not be
    true even in the game for sprites wider than 64 pixels.
*/
void DrawSprite(int x, int y, int width, int height, const char *spriteData, int saveSprite)
{
    D1 = x;
    D2 = y;
    D4 = width;
    D7 = width / 2;
    D5 = height;
    A0 = (dword)spriteData;

    int tmp;
    asm volatile (
        // later gcc is boo-boo, disallows force-register-to-ebp trick, so less efficient code it is...
        "push ebp\n"
        "mov  ebp, %[saveSprite]\n"
        "mov  %[tmp], offset SWOS_DrawSprite16Pixels + 0x88\n"
        "call %[tmp]\n"
        "pop  ebp\n"
        : [tmp] "=g" (tmp)
        : [saveSprite] "g" (saveSprite)
        : "cc", "memory"
    );
}

/** DrawSpriteInGame

    Just a helper routine, wrapper around DrawSprite. All the work is done by SWOS.
*/
static void DrawSpriteInGame(int x, int y, const SpriteGraphics *s)
{
    deltaColor = 0;
    DrawSprite(x, y, s->wquads * 16, s->nlines, s->data, 0);
}

static void DrawSpriteInMenus(int x, int y, const SpriteGraphics *s)
{
    deltaColor = 0;
    DrawSprite(x, y, s->wquads * 16, s->nlines, s->data, -1);
}

/** GetSmallNumberStringLength

    num         -> null-terminated string of a number to measure, it's assumed it contains only ASCII digits
    digitWidths -> lengths of digits, assumed length 10 elements
    kerning     -  number of pixels to add between digits
    outWidths   -> array that will receive width of each digit when printed

    Return what would be the length of string in pixels when rendered.
*/
static int GetSmallNumberStringLength(const char *num, const byte *digitWidths, int kerning, int *outWidths)
{
    int len = 0;
    while (*num) {
        /* little hack, two ones in a row look ugly with standard kerning */
        int fixTwoOnes = kerning && *num == '1' && num[1] == '1';
        assert(*num >= '0' && *num <= '9');
        len += *outWidths++ = digitWidths[*num++ - '0'] + kerning - fixTwoOnes;
    }
    return len;
}

/** PrintSmallNumber

   num    - number to print
   x      - number x coordinate
   y      - number y coordinate
   inGame - do we need small number during the game or in the menus? (bool)

   Prints small number, like the one that marks players during game, or the ones in edit tactics menu.
*/
void PrintSmallNumber(int num, int x, int y, bool32 inGame)
{
    assert(inGame == 0 || inGame == 1);
    char *buf;
    int length;
    static const byte inGameLengths[10] = { 3, 2, 3, 3, 3, 3, 3, 3, 3, 3 };
    static const byte editTacticsLenghts[10] = { 4, 2, 4, 4, 4, 4, 4, 4, 4, 4 };
    static const byte *digitLengths[2] = { editTacticsLenghts, inGameLengths };
    const byte *currentDigitLengths = digitLengths[inGame];
    const int kerning = inGame;
    const int startSprite = inGame ? 1188 : 162;
    char *eightMiddlePixel = &spritesIndex[inGame ? 1195 : 169]->data[16];
    void (*drawFunc)(int, int, const SpriteGraphics *) = inGame ? DrawSpriteInGame : DrawSpriteInMenus;
    int widths[16], *currentDigitWidth = widths;

    if (num < 0)
        return;

    buf = int2str(num);
    assert(inGame == 0 || inGame == 1);
    /* find out the length of number when printed to center it */
    length = GetSmallNumberStringLength(buf, currentDigitLengths, kerning, widths);

    x -= length / 2;
    y -= inGame ? 2 : 5;

    while (*buf) {
        bool zeroFixed = false;
        assert(*buf >= '0' && *buf <= '9');
        if (*buf == '0') {
            /* we got no zero sprite, so masquerade 8 instead */
            *eightMiddlePixel &= 0xf0;
            *buf = '8';
            zeroFixed = true;
        }
        drawFunc(x, y, spritesIndex[startSprite + *buf++ - '0' - 1]);
        if (zeroFixed)
            *eightMiddlePixel |= 0x02;
        x += *currentDigitWidth++;
    }
}

/** Text2Sprite

    Renders text into a specified sprite. This is wrapper around SWOS call that preserves ebp.

    x            -  x offset into sprite
    y            -  y offset into sprite
    spriteIndex  -  index of sprite to which we're rendering
    text         -> text to render
    charsTable   -> pointer to SWOS chars table, could be big or small table

    Returns x offset in the sprite right after text was inserted, or -1 in case text couldn't fit.
    Nothing will be drawn unless entire text can fit into destination sprite.
*/
int Text2Sprite(int x, int y, int spriteIndex, const char *text, const void *charsTable)
{
    D1 = x;
    D2 = y;
    D4 = spriteIndex;
    A0 = (dword)text;
    A1 = (dword)charsTable;
    calla(SWOS_Text2Sprite);
    return D0 ? -1 : D1;
}

/** DrawBitmap

    Only meant to be called from menus (and not in the game). If game would need it,
    use screenWidth instead of WIDTH. Assumes no pitch in bitmap data or destination.
*/
void DrawBitmap(int x, int y, int width, int height, const char *data)
{
    char *dest = linAdr384k + WIDTH * y + x;
    int delta = WIDTH - width;
    assert(width >= 0 && height >= 0 && data);
    while (height--) {
        for (int i = width; i != 0; i--) {
            if (*data)
                *dest = *data;
            data++;
            dest++;
        }
        dest += delta;
    }
}
