#include "swos.h"
#include "util.h"


static char ball1[] = {
    0, 3, 2, 0,
    2, 2, 2, 3,
    2, 3, 2, 1,
    0, 2, 3, 0
};

static char ball2[] = {
    0, 2, 3, 0,
    2, 3, 2, 2,
    2, 2, 2, 3,
    0, 2, 1, 0
};

static char ball3[] = {
    0, 2, 2, 0,
    3, 2, 3, 2,
    2, 2, 2, 1,
    0, 3, 1, 0
};

static char ball4[] = {
    0, 2, 3, 0,
    2, 2, 2, 2,
    3, 2, 3, 1,
    0, 2, 1, 0
};

static char *ballFrames[] = {ball1, ball2, ball3, ball4};

#define MAX_BALLS 8

struct Ball {
    int x;
    int y;
    int height;
    int speed;
    int arg;
} static balls[MAX_BALLS];

static void DrawBitmapClipped(int x, int y, int width, int height, char *data);
static unsigned numBalls;
static unsigned repeatTimer;
static int counter;

void InitBalls()
{
    numBalls = MAX_BALLS / 2 + 1 + rand() % (MAX_BALLS / 2);
    for (size_t i = 0; i < numBalls; i++) {
        balls[i].x = balls[i].y = 0;
        balls[i].height = 34 + rand() % 32;
        balls[i].arg = rand() % balls[i].height;
        do {
            balls[i].speed = rand() % 3 + 4;
        } while (i && balls[i].speed == balls[i - 1].speed);
    }
}

extern "C" void InitCounter()
{
    counter = 0;
    srand(currentTick);
    InitBalls();
}

extern "C" void AnimateBalls()
{
    int sine, idle = true;

    if (counter++ < 100)
        return;

    for (size_t i = 0; i < numBalls; i++) {
        if (balls[i].x >= 320)
          continue;
        idle = false;
        repeatTimer = 0;
        balls[i].x += balls[i].speed;
        balls[i].x += 2;
        sine = balls[i].height * sineCosineTable[balls[i].arg++ * balls[i].speed % 128];
        sine += ((sine & 0xffff) >= 0x1000) << 15;
        sine >>= 15;
        balls[i].y = 199 - sine - 4;
        DrawBitmapClipped(balls[i].x, balls[i].y, 4, 4, ballFrames[balls[i].arg % 4]);
    }
    repeatTimer += idle;
    if (repeatTimer >= 40) {
        counter = 100;
        InitBalls();
    }
}

void DrawBitmapClipped(int x, int y, int width, int height, char *data)
{
    if (x >= 320 || y >= 200)
        return;
    if (x + width < 0 || y + height < 0)
        return;
    if (x + width >= 320)
        width = 319 - x;
    if (x < 0) {
        width += x;
        x = 0;
    }
    if (y < 0) {
        height += y;
        y = 0;
    }
    if (y + height >= 200)
        height = 199 - y;
    DrawBitmap(x, y, width, height, data);
}