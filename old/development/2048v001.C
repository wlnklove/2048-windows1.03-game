/* 2048.c - Windows 1.03 Edition */
/* Microsoft C 5.0 + Windows SDK 1.03 */
/* K&R C style - 夏媛媛的师傅手敲 */

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>    /* sprintf */
#include <string.h>   /* strlen */

#define BOARD_SIZE 4
#define CELL_W     55
#define CELL_H     55
#define MARGIN     8

/* 全局数据段变量。Win16里全局变量放在DS段，别太大就行 */
int  board[BOARD_SIZE][BOARD_SIZE];
int  score;
int  game_over;
HWND hWndMain;

/* K&R风格的函数前置声明——不需要参数类型，只写名字 */
void InitGame();
void AddRandomTile();
int  MoveLeft();
int  MoveRight();
int  MoveUp();
int  MoveDown();
int  CanMove();
void DrawBoard();
void DrawCell();

/* ============================================================
   窗口过程 - Windows程序的心脏
   每一个消息都会被送到这里
   ============================================================ */
long FAR PASCAL WndProc(hWnd, message, wParam, lParam)
HWND     hWnd;
unsigned message;
WORD     wParam;
LONG     lParam;
{
    PAINTSTRUCT ps;
    HDC         hDC;
    RECT        rect;
    char        buf[64];
    int         i, j;

    switch (message) {

        case WM_CREATE:
            /* 窗口刚诞生，初始化一局 */
            hWndMain = hWnd;
            InitGame();
            break;

        case WM_PAINT:
            hDC = BeginPaint(hWnd, &ps);
            GetClientRect(hWnd, &rect);

            /* 先刷白背景 */
            SetBkColor(hDC, GetSysColor(COLOR_WINDOW));
            SetTextColor(hDC, GetSysColor(COLOR_WINDOWTEXT));
            PatBlt(hDC, rect.left, rect.top,
                   rect.right - rect.left,
                   rect.bottom - rect.top,
                   PATCOPY);

            /* 标题栏文字 */
            sprintf(buf, "2048  Score: %d", score);
            TextOut(hDC, MARGIN, 4, buf, strlen(buf));

            if (game_over) {
                TextOut(hDC, MARGIN, 20,
                        "GAME OVER - Press N for New Game", 32);
            } else {
                TextOut(hDC, MARGIN, 20,
                        "Arrows: Move   N: New Game", 26);
            }

            /* 画游戏板 */
            DrawBoard(hDC);

            EndPaint(hWnd, &ps);
            break;

        case WM_KEYDOWN:
            /* 游戏结束后只认N键 */
            if (game_over && wParam != 'N' && wParam != 'n')
                break;

            switch (wParam) {
                case VK_LEFT:
                    if (MoveLeft()) goto MOVED;
                    break;
                case VK_RIGHT:
                    if (MoveRight()) goto MOVED;
                    break;
                case VK_UP:
                    if (MoveUp()) goto MOVED;
                    break;
                case VK_DOWN:
                    if (MoveDown()) goto MOVED;
                    break;
                case 'N':
                case 'n':
                    InitGame();
                    InvalidateRect(hWnd, NULL, TRUE);
                    break;
            }
            break;

MOVED:
            /* 有移动才生成新块，然后检查死活 */
            AddRandomTile();
            if (!CanMove())
                game_over = 1;
            InvalidateRect(hWnd, NULL, TRUE);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0L;
}

/* ============================================================
   WinMain - 程序入口
   ============================================================ */
int PASCAL WinMain(hInstance, hPrevInstance, lpCmdLine, nCmdShow)
HANDLE hInstance;
HANDLE hPrevInstance;
LPSTR  lpCmdLine;
int    nCmdShow;
{
    MSG        msg;
    WNDCLASS   wc;
    HWND       hWnd;

    /* 用系统启动时间做随机种子 */
    srand((unsigned)GetCurrentTime());

    /* 注册窗口类。Windows 1.03里每个程序都要干这事 */
    if (!hPrevInstance) {
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = WndProc;
        wc.cbClsExtra    = 0;
        wc.cbWndExtra    = 0;
        wc.hInstance     = hInstance;
        wc.hIcon         = NULL;           /* 1.03图标很简单 */
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = GetStockObject(WHITE_BRUSH);
        wc.lpszMenuName  = NULL;
        wc.lpszClassName = "2048W103";

        if (!RegisterClass(&wc))
            return FALSE;
    }

    /* 创建平铺窗口。Windows 1.03的窗口不能重叠，都是贴瓷砖一样排 */
    hWnd = CreateWindow(
        "2048W103",
        "2048 - Windows 1.03",
        WS_TILED | WS_CAPTION | WS_SYSMENU | WS_BORDER,
        10, 10,
        280, 320,
        NULL, NULL, hInstance, NULL
    );

    if (!hWnd)
        return FALSE;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    /* 消息循环 - 这就是Windows和DOS的本质区别 */
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return msg.wParam;
}

/* ============================================================
   游戏逻辑
   ============================================================ */

void InitGame()
{
    int i, j;
    for (i = 0; i < BOARD_SIZE; i++)
        for (j = 0; j < BOARD_SIZE; j++)
            board[i][j] = 0;

    score = 0;
    game_over = 0;

    AddRandomTile();
    AddRandomTile();
}

void AddRandomTile()
{
    int empty[BOARD_SIZE * BOARD_SIZE][2];
    int count, pos;
    int i, j;

    count = 0;
    for (i = 0; i < BOARD_SIZE; i++)
        for (j = 0; j < BOARD_SIZE; j++)
            if (board[i][j] == 0) {
                empty[count][0] = i;
                empty[count][1] = j;
                count++;
            }

    if (count == 0) return;

    pos = rand() % count;
    i = empty[pos][0];
    j = empty[pos][1];
    board[i][j] = (rand() % 10 == 0) ? 4 : 2;   /* 10%出4，90%出2 */
}

/* ------------------------------------------------------------
   向左移动 - 核心算法
   师傅教我：所有方向都靠这个函数，这叫"归一化"
   ------------------------------------------------------------ */
int MoveLeft()
{
    int moved;
    int i, j, k;

    moved = 0;

    for (i = 0; i < BOARD_SIZE; i++) {
        /* 第一步：压缩。把非零数字全部挤到左边 */
        for (j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == 0) {
                for (k = j + 1; k < BOARD_SIZE; k++) {
                    if (board[i][k] != 0) {
                        board[i][j] = board[i][k];
                        board[i][k] = 0;
                        moved = 1;
                        break;
                    }
                }
            }
        }

        /* 第二步：合并。相邻相等就相加 */
        for (j = 0; j < BOARD_SIZE - 1; j++) {
            if (board[i][j] != 0 && board[i][j] == board[i][j+1]) {
                board[i][j] *= 2;
                score += board[i][j];
                board[i][j+1] = 0;
                moved = 1;
            }
        }

        /* 第三步：再压缩。合并后中间可能漏出空位 */
        for (j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == 0) {
                for (k = j + 1; k < BOARD_SIZE; k++) {
                    if (board[i][k] != 0) {
                        board[i][j] = board[i][k];
                        board[i][k] = 0;
                        moved = 1;
                        break;
                    }
                }
            }
        }
    }

    return moved;
}

/* ------------------------------------------------------------
   向右：水平翻转 → 左移 → 翻回来
   媛媛的师傅说的诀窍：把陌生的方向变成熟悉的方向
   ------------------------------------------------------------ */
int MoveRight()
{
    int moved;
    int i, j;
    int tmp;

    /* 翻转每一行 */
    for (i = 0; i < BOARD_SIZE; i++)
        for (j = 0; j < BOARD_SIZE / 2; j++) {
            tmp = board[i][j];
            board[i][j] = board[i][BOARD_SIZE - 1 - j];
            board[i][BOARD_SIZE - 1 - j] = tmp;
        }

    moved = MoveLeft();

    /* 翻回来 */
    for (i = 0; i < BOARD_SIZE; i++)
        for (j = 0; j < BOARD_SIZE / 2; j++) {
            tmp = board[i][j];
            board[i][j] = board[i][BOARD_SIZE - 1 - j];
            board[i][BOARD_SIZE - 1 - j] = tmp;
        }

    return moved;
}

/* ------------------------------------------------------------
   向上：转置 → 左移 → 转置回来
   矩阵转置就是行列互换
   ------------------------------------------------------------ */
int MoveUp()
{
    int moved;
    int i, j;
    int tmp;

    for (i = 0; i < BOARD_SIZE; i++)
        for (j = i + 1; j < BOARD_SIZE; j++) {
            tmp = board[i][j];
            board[i][j] = board[j][i];
            board[j][i] = tmp;
        }

    moved = MoveLeft();

    for (i = 0; i < BOARD_SIZE; i++)
        for (j = i + 1; j < BOARD_SIZE; j++) {
            tmp = board[i][j];
            board[i][j] = board[j][i];
            board[j][i] = tmp;
        }

    return moved;
}

/* ------------------------------------------------------------
   向下：转置 → 翻转 → 左移 → 翻 → 转置
   ------------------------------------------------------------ */
int MoveDown()
{
    int moved;
    int i, j;
    int tmp;

    /* 转置 */
    for (i = 0; i < BOARD_SIZE; i++)
        for (j = i + 1; j < BOARD_SIZE; j++) {
            tmp = board[i][j];
            board[i][j] = board[j][i];
            board[j][i] = tmp;
        }

    /* 水平翻转（让底部朝上） */
    for (i = 0; i < BOARD_SIZE; i++)
        for (j = 0; j < BOARD_SIZE / 2; j++) {
            tmp = board[i][j];
            board[i][j] = board[i][BOARD_SIZE - 1 - j];
            board[i][BOARD_SIZE - 1 - j] = tmp;
        }

    moved = MoveLeft();

    /* 翻回来 */
    for (i = 0; i < BOARD_SIZE; i++)
        for (j = 0; j < BOARD_SIZE / 2; j++) {
            tmp = board[i][j];
            board[i][j] = board[i][BOARD_SIZE - 1 - j];
            board[i][BOARD_SIZE - 1 - j] = tmp;
        }

    /* 转置回来 */
    for (i = 0; i < BOARD_SIZE; i++)
        for (j = i + 1; j < BOARD_SIZE; j++) {
            tmp = board[i][j];
            board[i][j] = board[j][i];
            board[j][i] = tmp;
        }

    return moved;
}

/* 检查是否还能动 */
int CanMove()
{
    int i, j;

    for (i = 0; i < BOARD_SIZE; i++)
        for (j = 0; j < BOARD_SIZE; j++)
            if (board[i][j] == 0)
                return 1;

    for (i = 0; i < BOARD_SIZE; i++)
        for (j = 0; j < BOARD_SIZE - 1; j++)
            if (board[i][j] == board[i][j+1])
                return 1;

    for (i = 0; i < BOARD_SIZE - 1; i++)
        for (j = 0; j < BOARD_SIZE; j++)
            if (board[i][j] == board[i+1][j])
                return 1;

    return 0;
}

/* ============================================================
   GDI绘图 - 师傅教我用Windows的画笔
   ============================================================ */

void DrawCell(hDC, value, x, y, w, h)
HDC hDC;
int value, x, y, w, h;
{
    char buf[16];
    int  len;
    HBRUSH hBrush, hOldBrush;
    HPEN   hPen, hOldPen;

    /* 选颜色。EGA有16色，咱们奢侈一点 */
    if (value == 0) {
        hBrush = GetStockObject(LTGRAY_BRUSH);
        hPen   = GetStockObject(BLACK_PEN);
    } else if (value == 2) {
        hBrush = CreateSolidBrush(RGB(238, 228, 218));
        hPen   = GetStockObject(BLACK_PEN);
    } else if (value == 4) {
        hBrush = CreateSolidBrush(RGB(237, 224, 200));
        hPen   = GetStockObject(BLACK_PEN);
    } else if (value == 8) {
        hBrush = CreateSolidBrush(RGB(242, 177, 121));
        hPen   = GetStockObject(WHITE_PEN);
    } else if (value == 16) {
        hBrush = CreateSolidBrush(RGB(245, 149, 99));
        hPen   = GetStockObject(WHITE_PEN);
    } else if (value == 32) {
        hBrush = CreateSolidBrush(RGB(246, 124, 95));
        hPen   = GetStockObject(WHITE_PEN);
    } else if (value == 64) {
        hBrush = CreateSolidBrush(RGB(246, 94, 59));
        hPen   = GetStockObject(WHITE_PEN);
    } else {
        hBrush = CreateSolidBrush(RGB(237, 207, 114));
        hPen   = GetStockObject(BLACK_PEN);
    }

    hOldBrush = SelectObject(hDC, hBrush);
    hOldPen   = SelectObject(hDC, hPen);

    /* Rectangle用当前画刷填充，用当前画笔描边 */
    Rectangle(hDC, x, y, x + w, y + h);

    SelectObject(hDC, hOldBrush);
    SelectObject(hDC, hOldPen);

    /* 画数字 */
    if (value != 0) {
        sprintf(buf, "%d", value);
        len = strlen(buf);
        /* 简单居中：EGA字符约8x14 */
        TextOut(hDC, x + (w - len * 8) / 2, y + (h - 14) / 2, buf, len);
    }

    /* 清理动态创建的画刷， stock object不能删 */
    if (value != 0 && value != 2 && value != 4)
        DeleteObject(hBrush);
    if (value >= 8)
        DeleteObject(hPen);
}

void DrawBoard(hDC)
HDC hDC;
{
    int i, j;
    int x, y;
    int top = 45;   /* 留给标题的空间 */

    for (i = 0; i < BOARD_SIZE; i++) {
        for (j = 0; j < BOARD_SIZE; j++) {
            x = MARGIN + j * (CELL_W + MARGIN);
            y = top + i * (CELL_H + MARGIN);
            DrawCell(hDC, board[i][j], x, y, CELL_W, CELL_H);
        }
    }
}
