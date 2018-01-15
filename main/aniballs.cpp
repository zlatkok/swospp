const int kMaxBalls = 8;

struct Ball {
    int x;
    int y;
    int height;
    int speed;
    int arg;
} static m_balls[kMaxBalls];

const char kBall1[] = {
    0, 3, 2, 0,
    2, 2, 2, 3,
    2, 3, 2, 1,
    0, 2, 3, 0
};

const char kBall2[] = {
    0, 2, 3, 0,
    2, 3, 2, 2,
    2, 2, 2, 3,
    0, 2, 1, 0
};

const char kBall3[] = {
    0, 2, 2, 0,
    3, 2, 3, 2,
    2, 2, 2, 1,
    0, 3, 1, 0
};

const char kBall4[] = {
    0, 2, 3, 0,
    2, 2, 2, 2,
    3, 2, 3, 1,
    0, 2, 1, 0
};

const char *kBallFrames[] = {kBall1, kBall2, kBall3, kBall4};


static void DrawBitmapClipped(int x, int y, int width, int height, const char *data);
static unsigned numBalls;
static unsigned repeatTimer;
static int counter;


void InitBalls()
{
    numBalls = kMaxBalls / 2 + 1 + rand() % (kMaxBalls / 2);
    for (size_t i = 0; i < numBalls; i++) {
        m_balls[i].x = m_balls[i].y = 0;
        m_balls[i].height = 34 + rand() % 32;
        m_balls[i].arg = rand() % m_balls[i].height;
        do {
            m_balls[i].speed = rand() % 3 + 4;
        } while (i && m_balls[i].speed == m_balls[i - 1].speed);
    }
}

extern "C" void InitCounter()
{
    counter = 0;
    srand(g_currentTick);
    InitBalls();
}

extern "C" void AnimateBalls()
{
    int sine, idle = true;

    if (counter++ < 100)
        return;

    for (size_t i = 0; i < numBalls; i++) {
        if (m_balls[i].x >= 320)
          continue;
        idle = false;
        repeatTimer = 0;
        m_balls[i].x += m_balls[i].speed;
        m_balls[i].x += 2;
        sine = m_balls[i].height * kSineCosineTable[m_balls[i].arg++ * m_balls[i].speed % 128];
        sine += ((sine & 0xffff) >= 0x1000) << 15;
        sine >>= 15;
        m_balls[i].y = 199 - sine - 4;
        DrawBitmapClipped(m_balls[i].x, m_balls[i].y, 4, 4, kBallFrames[m_balls[i].arg % 4]);
    }
    repeatTimer += idle;
    if (repeatTimer >= 40) {
        counter = 100;
        InitBalls();
    }
}

void DrawBitmapClipped(int x, int y, int width, int height, const char *data)
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
