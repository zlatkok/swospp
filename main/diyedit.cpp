#include "swos.h"
#include "dos.h"
#include "util.h"

extern dword edit_DIY_Menu asm("edit_DIY_Menu");
extern dword teamListMenu asm("teamListMenu");

#define MAX_DIY_FILES 45   /* keep in byte */

typedef struct DIY_File {  /* info about diy file found in game directory    */
    char fileName[9];
    char longName[23];
} DIY_File;

/* we'll use famous pitch.dat buffer to save bss memory,
   keep it the only reference in initialization to avoid GCC emitting additional sections */
static DIY_File *diyFiles = (DIY_File *)pitchDatBuffer;
static byte *sortedFiles = (byte *)pitchDatBuffer + MAX_DIY_FILES * sizeof(DIY_File);
static byte *sortedTeams = (byte *)pitchDatBuffer + MAX_DIY_FILES * (sizeof(DIY_File) + 1);
static int totalFiles;
static byte readFiles;
static byte lineOffset, showEntries;
static byte fileLoaded;
static const char diyExt[] = "*.DIY";

/** GetDIYFilenames

    Reads diy file names from game directory and fills diyFiles array of
    structures. Sets variable totalFiles to total number of files read, and
    readFiles to actual number of files read. Does simple diy file validity
    test by checking if name of competition in file is ASCII.
*/
void GetDIYFilenames()
{
    DTA dta;
    int i = 0;

    totalFiles = readFiles = 0;

    SetDTA(&dta);
    if (!FindFirstFile(diyExt))
        return;

    do {
        dword handle;
        char *p, *q;

        totalFiles++;
        p = dta.fileName;
        q = diyFiles[i].fileName;
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

            /* check if file was opened successfuly */
            if (!handle)
                break;
            /* seek to offset 4 - name of diy competition */
            if (SeekFile(handle, SEEK_START, 0, 4) < 0)
                break;
            if (ReadFile(handle, diyFiles[i].longName, 23) != 23)
                break;
            /* check for presence of non-ascii characters */
            diyFiles[i].longName[22] = '\0';
            for (p = diyFiles[i].longName; *p; p++)
                if (!isalpha(*p) && *p != ' ')
                    break;
            if (*p)
                break;

            /* file seems to be ok, increase index */
            i++;
        } while (false);

        if (handle)
            CloseFile(handle);
    } while (i < MAX_DIY_FILES && FindNextFile());

    while (FindNextFile())
        totalFiles++;

    readFiles = i;
}

extern "C" void EditDIYFile()
{
    int sortFlag, i, last;

    A0 = (dword)"EDIT/LOAD DIY FILE";
    calla(CheckContinueAbortPrompt);
    if (D0) {
        calla(SetExitMenuFlag);
        return;
    }

    GetDIYFilenames();

    /* init index list */
    for (i = 0; i < readFiles; i++)
        sortedFiles[i] = i;

    /* sort filenames */
    do {
        sortFlag = false;
        for (i = 0, last = readFiles - 1; i < last; i++) {
            if (stricmp(diyFiles[sortedFiles[i]].fileName, diyFiles[sortedFiles[i + 1]].fileName) > 0) {
                int tmp = sortedFiles[i];
                sortedFiles[i] = sortedFiles[i + 1];
                sortedFiles[i + 1] = tmp;
                sortFlag = true;
            }
        }
    } while (sortFlag);

    showEntries = min(readFiles, 15);

    if (totalFiles > MAX_DIY_FILES) {
        char buf[128];
        A1 = (dword)buf;
        D0 = MAX_DIY_FILES;
        A0 = (dword)"MORE THAN %0 DIY FILES FOUND";
        calla(PrimitivePrintf);
        A0 = (dword)buf;
        calla(ShowErrorMenu);
        return;
    }

    lineOffset = 0;
    A6 = (dword)&edit_DIY_Menu;
    calla(ShowMenu);
}

extern "C" void DIYUpdateList()
{
    int i, y;
    MenuEntry *m;

    D0 = 4 + lineOffset;
    calla(CalcMenuEntryAddress);
    m = (MenuEntry *)A0 + showEntries;

    /* make unneeded entries invisible */
    for (i = showEntries + lineOffset; i < MAX_DIY_FILES; i++, m++) {
        m->invisible = true;
        (m + MAX_DIY_FILES)->invisible = true;
    }
    /* fill entries with filenames and set next entries */
    for (i = lineOffset, m = (MenuEntry *)A0, y = 42; i < showEntries + lineOffset; i++, m++, y += 9) {
        m->u2.string = diyFiles[sortedFiles[i]].fileName;
        (m + MAX_DIY_FILES)->u2.string = diyFiles[sortedFiles[i]].longName;
        m->invisible = false;
        (m + MAX_DIY_FILES)->invisible = false;
        m->rightEntry = -1;
        m->leftEntry = 3 - (i > (showEntries + lineOffset) / 2);
        m->upEntry = 3 + i;
        m->downEntry = 5 + i;
        m->y = y;
        (m + MAX_DIY_FILES)->y = y;
    }
    m = (MenuEntry *)A0;
    m->upEntry = 3;
    /* last visible entry */
    (m + showEntries - 1)->downEntry = 1;
    (m + showEntries - 1)->rightEntry = 1;
    (m + showEntries - 1)->leftEntry = 2;
    /* exit */
    (m - lineOffset - 3)->leftEntry = 4 + showEntries + lineOffset - 1;
    (m - lineOffset - 3)->upEntry = 4 + showEntries + lineOffset - 1;
    /* up arrow */
    (m - lineOffset - 1)->rightEntry = 4 + lineOffset;
    /* down arrow */
    (m - lineOffset - 2)->rightEntry = 4 + showEntries + lineOffset - 1;

    /* center entries, if less than 15 */
    if (readFiles < 15) {
        y = 42 + 9 * (15 - readFiles) / 2 - 10;
        for (i = 0; i < readFiles; i++, m++) {
            m->y = y;
            (m + MAX_DIY_FILES)->y = y;
            y += 9;
        }
    }
}

extern "C" void DIYFileSelected()
{
    MenuEntry *m = (MenuEntry *)A5;
    int i, sortFlag, ord = m->ordinal - 4;
    char fname[13], *teams;

    strcpy(strcpy(fname, diyFiles[sortedFiles[ord]].fileName), diyExt + 1);
    A0 = (dword)fname;
    calla(LoadDIYFile);

    if ((int)D1 < 5189 + 2 + 684 * numSelectedTeams || gameType != 1) {
        char buf[128];
        A1 = (dword)buf;
        A2 = (dword)fname;
        A0 = (dword)"INVALID DIY FILE '%a'";
        calla(PrimitivePrintf);
        A0 = (dword)buf;
        calla(ShowErrorMenu);
        gameType = 0;
        return;
    }
    for (i = 0; i < numSelectedTeams; i++)   /* init sorted teams list */
        sortedTeams[i] = i;

    teams = (char*)selectedTeamsBuffer + 5;
    do {                                           /* sort teams, bubble sort */
        sortFlag = false;
        for (i = 0; i < numSelectedTeams - 1; i++) {
            if (stricmp(teams + 684 * sortedTeams[i], teams + 684 * sortedTeams[i + 1]) > 0) {
                int tmp = sortedTeams[i];
                sortedTeams[i] = sortedTeams[i + 1];
                sortedTeams[i + 1] = tmp;
                sortFlag = true;
            }
        }
    } while (sortFlag);

    fileLoaded = sortedFiles[ord];
    A6 = (dword)&teamListMenu;
    calla(ShowMenu);
}

extern "C" void DIYEditScrollUp()
{
    if (lineOffset > 0) {
        lineOffset--;
        DIYUpdateList();
        calla(DrawMenu);
    }
}

extern "C" void DIYEditScrollDown()
{
    if (lineOffset + 15 < readFiles) {
        lineOffset++;
        DIYUpdateList();
        calla(DrawMenu);
    }
}

extern "C" void DIYTeamsListInit()
{
    int columns, columnSize, width, height, entryHeight, entryWidth;
    int n, mod, x, y, yOrig, startY, endY, i, j, space, legendHeight;
    int teamIndex;
    byte *teams, modLimit, lastAdd;
    MenuEntry *m, *mExit;

    D0 = 0;                         /* get first entry                       */
    calla(CalcMenuEntryAddress);
    m = (MenuEntry *)A0;

    startY = m[67].height + m[67].y; /* title                                */
    endY = m[66].y;                 /* exit button                           */
    legendHeight = m[68].height;
    entryWidth = m->width;

    n = numSelectedTeams;

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

    teams = selectedTeamsBuffer + 5;
    teamIndex = 0;
    modLimit = (columns - mod) / 2;
    lastAdd = 0;
    mExit = m + 66;
    mExit->upEntry = -1;
    for (i = 0; i < columns; i++) {
        byte controls, add = i >= modLimit && mod > 0;
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
            controls = teams[684 * sortedTeams[teamIndex] - 1];
            m->u1.entryColor = 9 + controls;
            m->u1.entryColor += controls == 3;
            /* we'll use upEntryDis to keep original controls, and downEntryDis to keep current controls */
            m->upEntryDis = m->downEntryDis = controls;
            m->u2.string = (char *)teams + 684 * sortedTeams[teamIndex++];
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
    byte *control = &selectedTeamsBuffer[684 * sortedTeams[m->ordinal] + 4];

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

    for (i = 0; i < numSelectedTeams; i++, m++)
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
    strcpy(strcpy(saveFileName, diyFiles[fileLoaded].fileName), diyExt + 1);
    strcpy(saveFileTitle, diyFiles[fileLoaded].longName);

    if (i < numSelectedTeams)
        calla(SaveCareerSelected);
    calla(StartContest);
}