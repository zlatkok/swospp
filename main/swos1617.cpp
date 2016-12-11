/** swos1617.cpp

    New menu entry to celebrate SWOS 20 years anniversary.
*/

static int counter;
static int playersSettledCounter;

const int kAnimationStart = 25;
const int kPlayersSettledDelay = 38;
const int kNumberOfPlayers = 3;
const int kNumberOfImages = 24;
const int kPlayerSpeed = 14;
const int kSwosUnitedLogoX = 277;
const int kSwosUnitedLogoY = 173;

struct Position {
    short x;
    short y;
};

static const Position playerStartingLocations[kNumberOfPlayers] = { { -12, -15 }, { -12, 173 }, { 272, -15 } };
static const Position endPositions[kNumberOfPlayers] = {
    { kSwosUnitedLogoX, kSwosUnitedLogoY },
    { kSwosUnitedLogoX + 6, kSwosUnitedLogoY },
    { kSwosUnitedLogoX + 12, kSwosUnitedLogoY },
};

static SpriteGraphics *sprites[kNumberOfPlayers];
static word spriteOffsets[kNumberOfImages];

static Sprite players[kNumberOfPlayers];


static void printString(int x, int y, const char *text)
{
    D1 = x;
    D2 = y;
    D3 = 2;
    A0 = (dword)text;
    A1 = (dword)smallCharsTable;
    calla(DrawMenuText);
}

static void printCredits()
{
    const int kRowHeight = 7;
    const int kCreditsX = 6;
    const int kCreditsY = 18;

    static const char kCreditsText[] =
        "ENJOY THE GAME, AS MUCH AS YOU DID 20 YEARS AGO. IT IS NOT\0"
        "MEANT TO BE PERFECT. THE WORLD OF SOCCER WILL NEVER BE.\0"
        "IT SHALL CAPTURE A MOMENT IN TIME OF FOOTBALL HISTORY...\0"
        "\0"
        "YOURS,\0"
        "PLAYAVELI\0"
        "\0"
        "\0"
        "EDITORS:\0"
        "ANDIB, ARMANDO JIMENEZ, BB, BENNN54, BETICHUS, BOMB,\0"
        "COMBAT444, DIMETRODON, DJOWGER, GORZO, JANKO4, LEMONHEADIV,\0"
        "MAHARAJA, MARIN PARUSHEV, MICHAELS, PARANORMALJOE,\0"
        "PLAYAVELI, ROCK AND ROLL, SAINTGAV, SALVA, SIDA79, SKAARJ,\0"
        "SUPERHOOPS-1967, THEOLOGISTIC, WHITEULVER, XFLEA\0"
        "\0"
        "GRAPHICS:\0"
        "REDHAIR\0"
        "\0"
        "SPECIAL THANKS TO:\0"
        "SYNCHRONATED, ZLATKO KARAKAŠ, ROCK AND ROLL, REDHAIR\0"
        "ELMICHAJ, ARMANDO JIMENEZ, STARWINDZ, MOZG, SIDA79, ZIGEUNER\0"
        "\0"
        "VISIT:          WWW.SENSIBLESOCCER.DE\0"
        "                  WWW.FACEBOOK.COM/SWOSUNITED\0";

    int y = kCreditsY;
    const char *sentinel = kCreditsText + sizeof(kCreditsText);

    for (const char *p = kCreditsText; p < sentinel; p++) {
        printString(kCreditsX, y, p);
        while (*p)
            p++;
        y += kRowHeight;
    }
}

static char *loadSpriteFile()
{
    A0 = (dword)aTeam2_dat;
    A1 = (dword)teamFileBuffer;
    calla(LoadFile);

    return teamFileBuffer;
}

static int fixSprites(char *sprites, bool unchain)
{
    auto p = sprites;
    dword offset = 0;

    for (int i = 0; i < kNumberOfImages; i++) {
        auto spr = (SpriteGraphics *)p;
        spr->data = (char *)p + sizeof(SpriteGraphics);
        spriteOffsets[i] = offset;

        p += spr->size();
        offset += spr->size();

        if (unchain) {
            D0 = spr->wquads;
            D1 = spr->nlines;
            A0 = (dword)spr->data;
            calla(UnchainSpriteInMenus);
        }
    }

    return p - sprites;
}

static void copySprites(const char *templateSprites, int spritesSize)
{
    auto p = editTeamsSaveVarsArea;     /* poor old pitchDatBuffer wasn't enough */

    for (int i = 0; i < kNumberOfPlayers; i++) {
        sprites[i] = (SpriteGraphics *)p;
        memcpy(p, templateSprites, spritesSize);
        p += fixSprites(p, false);
    }
}

static void applyColorTable(SpriteGraphics *sprite, const short *colorTable)
{
    auto savedSprite = spritesIndex[0];
    spritesIndex[0] = sprite;

    D0 = 0;
    A0 = (dword)colorTable;
    calla(ConvertSpriteColors);

    spritesIndex[0] = savedSprite;
}

static void setSpriteColors(SpriteGraphics *sprites, int numSprites, TeamColor teamColor, PlayerFace playerFace)
{
    static short colorTable[16] = { 0, 1, 2, 3, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0 };

    colorTable[10] = teamColor;
    colorTable[11] = teamColor;
    colorTable[14] = 2;
    colorTable[15] = teamColor;

    D0 = playerFace;
    A1 = (dword)colorTable;
    calla(FillSkinColorConversionTable);

    for (int i = 0; i < numSprites; i++) {
        applyColorTable(sprites, colorTable);
        sprites = sprites->next();
    }
}

static void colorAllSprites()
{
    setSpriteColors(sprites[0], kNumberOfImages, MENU_TEAM_COLOR_RED, PL_FACE_BLACK);
    setSpriteColors(sprites[1], kNumberOfImages, MENU_TEAM_COLOR_GREEN, PL_FACE_GINGER);
    setSpriteColors(sprites[2], kNumberOfImages, MENU_TEAM_COLOR_BLUE, PL_FACE_WHITE);
}

static void initializeSprites()
{
    auto templateSprites = loadSpriteFile();
    auto spritesSize = fixSprites(templateSprites, true);
    copySprites(templateSprites, spritesSize);
    colorAllSprites();
}

static void drawSprite(SpriteGraphics *sprite, int x, int y)
{
    auto savedSprite = spritesIndex[0];
    spritesIndex[0] = sprite;

    D0 = 0;
    D1 = x;
    D2 = y;
    calla(SWOS_DrawSprite);

    spritesIndex[0] = savedSprite;
}

static void updatePlayerSpeed(Sprite& player)
{
    const int MAX_SPEED = 1280;
    player.speed = playerSpeedsGameInProgress[min(max(kPlayerSpeed / 2, 0), sizeofarray(playerSpeedsGameInProgress) - 1)];
    /* so the slower players would get greater delay between frames */
    player.frameDelay = max(0, MAX_SPEED - player.speed) / 128 + 6;
}

static void initializePlayers()
{
    for (int i = 0; i < kNumberOfPlayers; i++) {
        memset(&players[i], 0, sizeof(Sprite));

        players[i].teamNumber = 1;
        players[i].beenDrawn = true;

        players[i].x = playerStartingLocations[i].x;
        players[i].y = playerStartingLocations[i].y;

        players[i].destX = endPositions[i].x;
        players[i].destY = endPositions[i].y;

        players[i].direction = -1;

        updatePlayerSpeed(players[i]);
    }
}

static bool playersSettled()
{
    if (playersSettledCounter >= kPlayersSettledDelay)
        return true;

    playersSettledCounter++;
    return false;
}

static void updatePlayerDirection(Sprite& player, int direction)
{
    auto oldDirection = player.direction;
    player.fullDirection = direction;
    player.direction = ((direction + 16) & 0xff) >> 5;

    if (oldDirection != player.direction) {
        A0 = (dword)playerRunningAnimTable;
        A1 = (dword)&player;
        calla(SetAnimationTable);
    }
}

static bool playerArrived(const Sprite& player)
{
    return player.x.whole() == player.destX && player.y.whole() == player.destY;
}

static bool movePlayers()
{
    bool movement = false;

    for (int i = 0; i < kNumberOfPlayers; i++) {
        D0 = players[i].speed;
        D1 = players[i].destX;
        D2 = players[i].destY;
        D3 = players[i].x.whole();
        D4 = players[i].y.whole();
        calla(CalculateDeltaXAndY);
        players[i].deltaX = D1;
        players[i].deltaY = D2;

        if (playerArrived(players[i])) {
            players[i].direction = 4;

            A0 = (dword)playerNormalStandingAnimTable;
            A1 = (dword)&players[i];
            calla(SetAnimationTable);
        } else {
            movement = true;
            players[i].isMoving = true;
            players[i].beenDrawn = true;

            if ((int)D0 >= 0) {
                updatePlayerDirection(players[i], D0);

                A0 = (dword)&players[i];
                calla(MovePlayer);
            }
        }

        A0 = (dword)&players[i];
        calla(SetNextPlayerFrame);
    }

    return movement;
}

static void drawPlayers()
{
    /* keep middle player on top */
    static const int kOrder[kNumberOfPlayers] = { 0, 2, 1 };

    for (int pl = 0; pl < kNumberOfPlayers; pl++) {
        int i = kOrder[pl];
        auto spriteNo = players[i].pictureIndex - 341;
        assert(spriteNo >= 0 && spriteNo < kNumberOfImages);
        auto spriteOffset = spriteOffsets[spriteNo];

        auto spr = (SpriteGraphics *)((char *)sprites[i] + spriteOffset);
        drawSprite(spr, players[i].x, players[i].y);
    }
}

static void drawSwosUnitedLogo()
{
    const int kLogoWidth = 24;
    const int kLogoHeight = 14;
    extern byte swosUnitedLogo[kLogoWidth][kLogoHeight] asm("swosUnitedLogo");

    DrawBitmap(kSwosUnitedLogoX, kSwosUnitedLogoY, kLogoWidth, kLogoHeight, (char *)swosUnitedLogo);
}

extern "C" void SWOSAnniversaryMenuInit()
{
    counter = 0;
    playersSettledCounter = 0;

    goalScored = 0;
    gameStatePl = ST_GAME_IN_PROGRESS;

    initializeSprites();
    initializePlayers();
}

extern "C" void SWOSAnniversaryMenuOnDraw()
{
    printCredits();
    if (counter++ >= kAnimationStart) {
        if (!movePlayers() && playersSettled())
            drawSwosUnitedLogo();
        else
            drawPlayers();
    }

}
