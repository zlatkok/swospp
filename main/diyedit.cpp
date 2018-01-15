extern dword edit_DIY_Menu asm("edit_DIY_Menu");
extern dword teamListMenu asm("teamListMenu");

#define MAX_DIY_FILES 45   /* keep in byte */

struct DIY_File {  /* info about diy file found in game directory    */
    char fileName[9];
    char longName[23];
};

/* we'll use famous pitch.dat buffer to save bss memory,
   keep it the only reference in initialization to avoid GCC emitting additional sections */
static DIY_File *m_diyFiles = (DIY_File *)g_pitchDatBuffer;
static byte *m_sortedFiles = (byte *)g_pitchDatBuffer + MAX_DIY_FILES * sizeof(DIY_File);
static byte *m_sortedTeams = (byte *)g_pitchDatBuffer + (MAX_DIY_FILES + 1) * sizeof(DIY_File);
static int m_totalFiles;
static byte m_readFiles;
static byte m_lineOffset, m_showEntries;
static byte m_fileLoaded;
static const char kDiyExt[] = "*.DIY";


/** GetDIYFilenames

    Reads DIY file names from game directory and fills diyFiles array of
    structures. Sets variable totalFiles to total number of files read, and
    readFiles to actual number of files read. Does simple DIY file validity
    test by checking if name of competition in file is ASCII.
*/
void GetDIYFilenames()
{
    DTA dta;
    int i = 0;

    m_totalFiles = m_readFiles = 0;

    SetDTA(&dta);
    if (!FindFirstFile(kDiyExt))
        return;

    do {
        dword handle;
        char *p, *q;

        m_totalFiles++;
        p = dta.fileName;
        q = m_diyFiles[i].fileName;
        do {
            if (*p == '.') {
                *q = '\0';
                break;
            }
            *q++ = *p++;
        } while (*p);

        /* check file */
        handle = OpenFile(F_READ_ONLY, dta.fileName);

        do {
            char *p;

            /* check if file was opened successfully */
            if (!handle)
                break;
            /* seek to offset 4 - name of DIY competition */
            if (SeekFile(handle, SEEK_START, 0, 4) < 0)
                break;
            if (ReadFile(handle, m_diyFiles[i].longName, 23) != 23)
                break;
            /* check for presence of non-ASCII characters */
            m_diyFiles[i].longName[22] = '\0';
            for (p = m_diyFiles[i].longName; *p; p++)
                if (!isalpha(*p) && *p != ' ')
                    break;
            if (*p)
                break;

            /* file seems to be OK, increase index */
            i++;
        } while (false);

        if (handle)
            CloseFile(handle);
    } while (i < MAX_DIY_FILES && FindNextFile());

    while (FindNextFile())
        m_totalFiles++;

    m_readFiles = i;
}

extern "C" void EditDIYFile()
{
    A0 = (dword)"EDIT/LOAD DIY FILE";
    calla(CheckContinueAbortPrompt);
    if (D0) {
        calla(SetExitMenuFlag);
        return;
    }

    GetDIYFilenames();

    /* init index list */
    for (int i = 0; i < m_readFiles; i++)
        m_sortedFiles[i] = i;

    /* sort filenames */
    bool sortFlag;
    do {
        sortFlag = false;
        for (int i = 0; i < m_readFiles - 1; i++) {
            if (stricmp(m_diyFiles[m_sortedFiles[i]].fileName, m_diyFiles[m_sortedFiles[i + 1]].fileName) > 0) {
                int tmp = m_sortedFiles[i];
                m_sortedFiles[i] = m_sortedFiles[i + 1];
                m_sortedFiles[i + 1] = tmp;
                sortFlag = true;
            }
        }
    } while (sortFlag);

    m_showEntries = min(m_readFiles, 15);

    if (m_totalFiles > MAX_DIY_FILES) {
        char buf[128];
        A1 = (dword)buf;
        D0 = MAX_DIY_FILES;
        A0 = (dword)"MORE THAN %0 DIY FILES FOUND";
        calla(PrimitivePrintf);
        A0 = (dword)buf;
        calla(ShowErrorMenu);
        return;
    }

    m_lineOffset = 0;
    A6 = (dword)&edit_DIY_Menu;
    calla(ShowMenu);
}

extern "C" void DIYUpdateList()
{
    int i, y;
    MenuEntry *m;

    D0 = 4 + m_lineOffset;
    calla(CalcMenuEntryAddress);
    m = (MenuEntry *)A0 + m_showEntries;

    /* make unneeded entries invisible */
    for (i = m_showEntries + m_lineOffset; i < MAX_DIY_FILES; i++, m++) {
        m->invisible = true;
        (m + MAX_DIY_FILES)->invisible = true;
    }

    /* fill entries with filenames and set next entries */
    for (i = m_lineOffset, m = (MenuEntry *)A0, y = 42; i < m_showEntries + m_lineOffset; i++, m++, y += 9) {
        m->u2.string = m_diyFiles[m_sortedFiles[i]].fileName;
        (m + MAX_DIY_FILES)->u2.string = m_diyFiles[m_sortedFiles[i]].longName;
        m->invisible = false;
        (m + MAX_DIY_FILES)->invisible = false;
        m->rightEntry = -1;
        m->leftEntry = 3 - (i > (m_showEntries + m_lineOffset) / 2);
        m->upEntry = 3 + i;
        m->downEntry = 5 + i;
        m->y = y;
        (m + MAX_DIY_FILES)->y = y;
    }

    m = (MenuEntry *)A0;
    m->upEntry = 3;
    /* last visible entry */
    (m + m_showEntries - 1)->downEntry = 1;
    (m + m_showEntries - 1)->rightEntry = 1;
    (m + m_showEntries - 1)->leftEntry = 2;
    /* exit */
    (m - m_lineOffset - 3)->leftEntry = 4 + m_showEntries + m_lineOffset - 1;
    (m - m_lineOffset - 3)->upEntry = 4 + m_showEntries + m_lineOffset - 1;
    /* up arrow */
    (m - m_lineOffset - 1)->rightEntry = 4 + m_lineOffset;
    /* down arrow */
    (m - m_lineOffset - 2)->rightEntry = 4 + m_showEntries + m_lineOffset - 1;

    /* center entries, if less than 15 */
    if (m_readFiles < 15) {
        y = 42 + 9 * (15 - m_readFiles) / 2 - 10;
        for (i = 0; i < m_readFiles; i++, m++) {
            m->y = y;
            (m + MAX_DIY_FILES)->y = y;
            y += 9;
        }
    }
}

extern "C" void DIYFileSelected()
{
    MenuEntry *m = (MenuEntry *)A5;
    int ord = m->ordinal - 4;
    char fname[13];

    strcpy(strcpy(fname, m_diyFiles[m_sortedFiles[ord]].fileName), kDiyExt + 1);
    A0 = (dword)fname;
    calla(LoadDIYFile);

    if (D1 < 5189 + 2 + sizeof(TeamFile) * g_numSelectedTeams || g_gameType != 1) {
        char buf[128];
        A1 = (dword)buf;
        A2 = (dword)fname;
        A0 = (dword)"INVALID DIY FILE '%a'";
        calla(PrimitivePrintf);
        A0 = (dword)buf;
        calla(ShowErrorMenu);
        g_gameType = 0;
        return;
    }

    for (int i = 0; i < g_numSelectedTeams; i++)   /* init sorted teams list */
        m_sortedTeams[i] = i;

    bool sortFlag;
    do {                                           /* sort teams, bubble sort */
        sortFlag = false;
        for (int i = 0; i < g_numSelectedTeams - 1; i++) {
            if (stricmp(g_selectedTeams[m_sortedTeams[i]].name, g_selectedTeams[m_sortedTeams[i + 1]].name) > 0) {
                int tmp = m_sortedTeams[i];
                m_sortedTeams[i] = m_sortedTeams[i + 1];
                m_sortedTeams[i + 1] = tmp;
                sortFlag = true;
            }
        }
    } while (sortFlag);

    m_fileLoaded = m_sortedFiles[ord];
    A6 = (dword)&teamListMenu;
    calla(ShowMenu);
}

extern "C" void DIYEditScrollUp()
{
    if (m_lineOffset > 0) {
        m_lineOffset--;
        DIYUpdateList();
        calla(DrawMenu);
    }
}

extern "C" void DIYEditScrollDown()
{
    if (m_lineOffset + 15 < m_readFiles) {
        m_lineOffset++;
        DIYUpdateList();
        calla(DrawMenu);
    }
}

extern "C" void DIYTeamsListInit()
{
    int columns, columnSize, width, height, entryHeight, entryWidth;
    int n, mod, x, y, yOrig, startY, endY, i, j, space, legendHeight;
    int teamIndex;
    byte modLimit, lastAdd;
    MenuEntry *m, *mExit;

    D0 = 0;                         /* get first entry                       */
    calla(CalcMenuEntryAddress);
    m = (MenuEntry *)A0;

    startY = m[67].height + m[67].y; /* title                                */
    endY = m[66].y;                 /* exit button                           */
    legendHeight = m[68].height;
    entryWidth = m->width;

    n = g_numSelectedTeams;

    for (i = 0; i < 66; i++)        /* all teams invisible in advance        */
        m[i].invisible = true;

    entryHeight = 10;               /* dynamically calculate entry height    */
    space = endY - startY - legendHeight;
    do {
        columns = (n * entryHeight + space - 1) / space;
        columnSize = n / columns;
        mod = n % columns;
        height = (columnSize + !!mod) * entryHeight - 1;
        width = columns * (entryWidth + 4) - 4;
        entryHeight--;
    } while (space < 0 || width >= WIDTH);

    space -= height;
    space /= 3;
    x = (WIDTH - width) / 2;
    yOrig = startY + space;

    for (i = 0; i < 6; i++)         /* reposition legend                     */
        m[68 + i].y = yOrig;

    yOrig += space + legendHeight;
    y = yOrig;

    teamIndex = 0;
    modLimit = (columns - mod) / 2;
    lastAdd = 0;
    mExit = m + 66;
    mExit->upEntry = -1;
    for (i = 0; i < columns; i++) {
        byte add = i >= modLimit && mod > 0;
        for (j = 0; j < columnSize + add; j++, m++) {
            m->x = x;
            m->y = y + j * (entryHeight + 1);
            m->height = entryHeight;
            m->downEntry = m->ordinal + 1;
            if (!j)
                m->upEntry = -1;
            else
                m->upEntry = m->ordinal - 1;
            if (!i)
                m->leftEntry = -1;
            else
                m->leftEntry = m->ordinal - columnSize - lastAdd;
            if (i >= columns - 1)
                m->rightEntry = -1;
            else
                m->rightEntry = m->ordinal + columnSize + add;
            m->invisible = false;
            m->type1 = 2;
            auto controls = g_selectedTeams[m_sortedTeams[teamIndex]].teamStatus;
            m->u1.entryColor = 9 + controls;
            m->u1.entryColor += controls == 3;
            /* we'll use upEntryDis to keep original controls, and downEntryDis to keep current controls */
            m->upEntryDis = m->downEntryDis = controls;
            m->u2.string = g_selectedTeams[m_sortedTeams[teamIndex++]].name;
            if (j >= columnSize) {
                mod--;
                if ((signed char)mExit->upEntry == -1)
                    mExit->upEntry = m->ordinal;
                if (!lastAdd)
                    m->leftEntry = -1;
                /* last column can never have extra entry */
                if (i == columns - 2)
                    m->rightEntry = -1;
            }
        }
        lastAdd = add;
        m[-1].downEntry = 66;
        x += entryWidth + 4;
        y = yOrig;
    }
    if ((signed char)mExit->upEntry == -1)
        mExit->upEntry = columnSize - 1;
}

extern "C" void DIYTeamsOnSelect()
{
    MenuEntry *m = (MenuEntry *)A5;
    auto *control = &g_selectedTeams[m_sortedTeams[m->ordinal]].teamStatus;

    *control = (*control + 1) % 4;
    *control += !*control;

    m->u1.entryColor = 9 + *control;
    m->u1.entryColor += *control == 3;
    m->downEntryDis = *control;
}

extern "C" void DIYTeamsExit()
{
    int i;
    MenuEntry *m;

    D0 = 0;
    calla(CalcMenuEntryAddress);
    m = (MenuEntry *)A0;

    for (i = 0; i < g_numSelectedTeams; i++, m++)
        if (m->downEntryDis != m->upEntryDis)
            break;

    /* important: make it look like everything is called from SWOS main menu */
#ifdef SWOS_16_17
    extern char newMainMenu[] asm ("newMainMenu");
    A6 = (dword)newMainMenu;
#else
    A6 = (dword)SWOS_MainMenu;
#endif
    calla(PrepareMenu);

    /* set fileName for save and title */
    strcpy(strcpy(saveFileName, m_diyFiles[m_fileLoaded].fileName), kDiyExt + 1);
    strcpy(saveFileTitle, m_diyFiles[m_fileLoaded].longName);

    if (i < g_numSelectedTeams)
        calla(SaveCareerSelected);
    calla(StartContest);
}
