/* 2048.c - Windows 1.03 Edition v2.0                         */
/* 界面大升级：3D边框 + 挖洞双缓冲 + 棋盘尺寸限制             */
/* 游戏逻辑完全保留 v1.1 不变                                 */

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>    /* sprintf */
#include <string.h>   /* strlen */
#include "2048.h"

/* 存档/分数相关声明 */
void SaveGame();
int  LoadGame();

typedef struct {
    int highscore;
    int recent[5];
    int count;
} ScoreHistory;

BOOL GetScoreHistory();
void SetScoreHistory();
void UpdateScoreHistory();
BOOL FAR PASCAL ScoreboardDlg();

#define BOARD_SIZE       4
#define MARGIN           8        /* 窗口边距 */
#define MAX_BOARD_PIXELS 280      /* 棋盘最大边长——到达后停止放大 */
#define TITLE_HEIGHT     36       /* 标题区高度 */

/* 全局变量 */
int  board[BOARD_SIZE][BOARD_SIZE];
int  score;
int  game_over;
int  g_bDirty = 0;
int  g_bPlayerMoved = 0;
HWND hWndMain;
HANDLE hInst;
FARPROC lpfnAboutDlg;
FARPROC lpfnHowToPlayDlg;
FARPROC lpfnScoreboardDlg;

/* GDI对象缓存——只创建一次，WM_DESTROY 统一清理 */
HBRUSH hBrushes[13];        /* 数字格子颜色（0=空白, 1=2, 2=4, ...） */
HPEN   hPenBlack, hPenWhite;
HBRUSH hbrBg = NULL;        /* 蓝色背景画刷（CreateSolidBrush，需删除） */
HBRUSH hbrBoard;            /* 黑色游戏底板（stock object，不需删除） */
HFONT  hFont = NULL;        /* 动态缩放数字字体 */

/* 布局变量 */
static int cxClient, cyClient;
static int cellSize;        /* 格子边长 */
static int gapSize;         /* 格子间间隙 */
static int boardPixels;     /* 游戏区域总边长 */
static int boardX, boardY;  /* 游戏区域左上角屏幕坐标 */

/* 函数前置声明 */
void InitGame();
void AddRandomTile();
int  MoveLeft();
int  MoveRight();
int  MoveUp();
int  MoveDown();
int  CanMove();
void CalcLayout();
void DrawGameArea();
void DrawCell3D();
void DrawSunkenRect();
void DrawRaisedRect();
void InitGDI();
void CleanupGDI();
int  ValueToIndex();
long FAR PASCAL WndProc();
BOOL FAR PASCAL AboutDlg();
BOOL FAR PASCAL HowToPlayDlg();

/* ============================================================
   布局计算——棋盘到达最佳大小后停止放大
   ============================================================ */
void CalcLayout(cx, cy)
int cx, cy;
{
    int availW, availH, maxBoard;

    cxClient = cx;
    cyClient = cy;

    availW = cx - 2 * MARGIN;
    availH = cy - TITLE_HEIGHT - MARGIN;

    maxBoard = (availW < availH) ? availW : availH;
    if (maxBoard > MAX_BOARD_PIXELS) maxBoard = MAX_BOARD_PIXELS;

    gapSize = maxBoard / 24;
    if (gapSize < 3) gapSize = 3;

    cellSize = (maxBoard - 5 * gapSize) / 4;
    if (cellSize < 20) cellSize = 20;

    boardPixels = 4 * cellSize + 5 * gapSize;

    boardX = (cx - boardPixels) / 2;
    boardY = TITLE_HEIGHT + (cy - TITLE_HEIGHT - boardPixels) / 2;
    if (boardY < TITLE_HEIGHT) boardY = TITLE_HEIGHT;

    /* 动态字体——数字跟着格子变大 */
    if (hFont) DeleteObject(hFont);
    hFont = CreateFont(cellSize / 2, 0, 0, 0,
                       FW_BOLD, FALSE, FALSE, FALSE,
                       OEM_CHARSET,
                       OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS,
                       PROOF_QUALITY,
                       DEFAULT_PITCH,
                       NULL);
}

/* ============================================================
   3D 边框绘制（两层，标准 EDGE_SUNKEN / EDGE_RAISED 效果）
   ============================================================ */

/* 凹陷边框——模拟游戏区域"凹下去"
 * 外层：上/左黑色（阴影），下/右白色（高光）
 * 内层：上/左白色（高光），下/右黑色（阴影）
 * 在蓝色背景 + 黑色底板上都能看到明暗对比 */
void DrawSunkenRect(hDC, x, y, w, h)
HDC hDC;
int x, y, w, h;
{
    HPEN hOld;

    /* 外层 */
    hOld = SelectObject(hDC, hPenBlack);
    MoveTo(hDC, x, y);           LineTo(hDC, x + w, y);           /* 上 */
    MoveTo(hDC, x, y);           LineTo(hDC, x, y + h);           /* 左 */
    SelectObject(hDC, hPenWhite);
    MoveTo(hDC, x, y + h - 1);   LineTo(hDC, x + w, y + h - 1);   /* 下 */
    MoveTo(hDC, x + w - 1, y);   LineTo(hDC, x + w - 1, y + h);   /* 右 */

    /* 内层 */
    SelectObject(hDC, hPenWhite);
    MoveTo(hDC, x + 1, y + 1);       LineTo(hDC, x + w - 1, y + 1);       /* 上 */
    MoveTo(hDC, x + 1, y + 1);       LineTo(hDC, x + 1, y + h - 1);       /* 左 */
    SelectObject(hDC, hPenBlack);
    MoveTo(hDC, x + 1, y + h - 2);   LineTo(hDC, x + w - 1, y + h - 2);   /* 下 */
    MoveTo(hDC, x + w - 2, y + 1);   LineTo(hDC, x + w - 2, y + h - 1);   /* 右 */

    SelectObject(hDC, hOld);
}

/* 凸起边框——模拟格子"浮起来"
 * 外层：上/左白色（高光），下/右黑色（阴影）
 * 内层：上/左黑色（阴影），下/右白色（高光） */
void DrawRaisedRect(hDC, x, y, w, h)
HDC hDC;
int x, y, w, h;
{
    HPEN hOld;

    /* 外层 */
    hOld = SelectObject(hDC, hPenWhite);
    MoveTo(hDC, x, y);           LineTo(hDC, x + w, y);           /* 上 */
    MoveTo(hDC, x, y);           LineTo(hDC, x, y + h);           /* 左 */
    SelectObject(hDC, hPenBlack);
    MoveTo(hDC, x, y + h - 1);   LineTo(hDC, x + w, y + h - 1);   /* 下 */
    MoveTo(hDC, x + w - 1, y);   LineTo(hDC, x + w - 1, y + h);   /* 右 */

    /* 内层 */
    SelectObject(hDC, hPenBlack);
    MoveTo(hDC, x + 1, y + 1);       LineTo(hDC, x + w - 1, y + 1);       /* 上 */
    MoveTo(hDC, x + 1, y + 1);       LineTo(hDC, x + 1, y + h - 1);       /* 左 */
    SelectObject(hDC, hPenWhite);
    MoveTo(hDC, x + 1, y + h - 2);   LineTo(hDC, x + w - 1, y + h - 2);   /* 下 */
    MoveTo(hDC, x + w - 2, y + 1);   LineTo(hDC, x + w - 2, y + h - 1);   /* 右 */

    SelectObject(hDC, hOld);
}

/* ============================================================
   游戏区域绘制（在内存 DC 上画，坐标从 (0,0) 开始）
   ============================================================ */
void DrawGameArea(hDC)
HDC hDC;
{
    int i, j, x, y;
    RECT rc;

    /* 1. 底板填充（黑色） */
    rc.left = 0; rc.top = 0;
    rc.right = boardPixels; rc.bottom = boardPixels;
    FillRect(hDC, &rc, hbrBoard);

    /* 2. 3D 凹陷外框 */
    DrawSunkenRect(hDC, 0, 0, boardPixels, boardPixels);

    /* 3. 画 4x4 格子 */
    for (i = 0; i < BOARD_SIZE; i++) {
        for (j = 0; j < BOARD_SIZE; j++) {
            x = gapSize + j * (cellSize + gapSize);
            y = gapSize + i * (cellSize + gapSize);
            DrawCell3D(hDC, board[i][j], x, y, cellSize, cellSize);
        }
    }
}

/* 格子绘制——填充 + 3D凸起边框 + 数字 */
void DrawCell3D(hDC, value, x, y, w, h)
HDC hDC;
int value, x, y, w, h;
{
    char buf[16];
    int  len, idx;
    HBRUSH hBrush, hOldBrush;
    HFONT  hOldFont;
    LONG   extent;
    int    textW, textH;
    RECT   rc;

    idx = ValueToIndex(value);
    hBrush = hBrushes[idx];

    /* 填充格子背景 */
    rc.left = x; rc.top = y;
    rc.right = x + w; rc.bottom = y + h;
    FillRect(hDC, &rc, hBrush);

    /* 3D 凸起边框 */
    DrawRaisedRect(hDC, x, y, w, h);

    /* 数字 */
    if (value != 0) {
        SetBkMode(hDC, TRANSPARENT);
        SetTextColor(hDC, (value >= 8 && value < 128) ?
                          RGB(255,255,255) : RGB(0,0,0));

        hOldFont = SelectObject(hDC, hFont);
        sprintf(buf, "%d", value);
        len = strlen(buf);
        extent = GetTextExtent(hDC, buf, len);
        textW = LOWORD(extent);
        textH = HIWORD(extent);
        TextOut(hDC, x + (w - textW) / 2, y + (h - textH) / 2, buf, len);
        SelectObject(hDC, hOldFont);
    }
}

/* ============================================================
   窗口过程
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
    RECT        rcBoard, rcMargin;
    HDC         hdcMem;
    HBITMAP     hbmMem, hbmOld;
    DWORD       oldOrg;

    switch (message) {

    case WM_CREATE:
        hWndMain = hWnd;
        InitGame();
        InitGDI();
        /* 创建缓存画刷——只创建一次 */
        hbrBg = CreateSolidBrush(RGB(0, 0, 255));
        hbrBoard = GetStockObject(BLACK_BRUSH);
        /* 初始化布局 */
        {
            RECT cr;
            GetClientRect(hWnd, &cr);
            CalcLayout(cr.right, cr.bottom);
        }
        break;

    case WM_SIZE:
        CalcLayout(LOWORD(lParam), HIWORD(lParam));
        InvalidateRect(hWnd, NULL, FALSE);
        break;

    /* 不擦除背景——WM_PAINT 全权处理
     * 返回 1 跳过系统 WM_ERASEBKGND，避免闪烁 */
    case WM_ERASEBKGND:
        return 1L;

    /* ========================================================
       WM_PAINT - 挖洞双缓冲绘制
       背景只画四周边距（游戏区域挖洞保持旧帧）
       游戏区域用棋盘大小的内存 DC 画好后 BitBlt 原子替换
       ======================================================== */
    case WM_PAINT:
        hDC = BeginPaint(hWnd, &ps);
        GetClientRect(hWnd, &rect);

        /* 游戏区域矩形（屏幕坐标） */
        rcBoard.left = boardX;
        rcBoard.top = boardY;
        rcBoard.right = boardX + boardPixels;
        rcBoard.bottom = boardY + boardPixels;

        /* 1. 背景挖洞——只画游戏区域以外的边距
         *    游戏区域完全不碰，旧帧保持到 BitBlt 原子替换 */
        /* 上边距 */
        if (rcBoard.top > 0) {
            rcMargin.left = 0; rcMargin.top = 0;
            rcMargin.right = rect.right; rcMargin.bottom = rcBoard.top;
            FillRect(hDC, &rcMargin, hbrBg);
        }
        /* 下边距 */
        if (rcBoard.bottom < rect.bottom) {
            rcMargin.left = 0; rcMargin.top = rcBoard.bottom;
            rcMargin.right = rect.right; rcMargin.bottom = rect.bottom;
            FillRect(hDC, &rcMargin, hbrBg);
        }
        /* 左边距 */
        if (rcBoard.left > 0) {
            rcMargin.left = 0; rcMargin.top = rcBoard.top;
            rcMargin.right = rcBoard.left; rcMargin.bottom = rcBoard.bottom;
            FillRect(hDC, &rcMargin, hbrBg);
        }
        /* 右边距 */
        if (rcBoard.right < rect.right) {
            rcMargin.left = rcBoard.right; rcMargin.top = rcBoard.top;
            rcMargin.right = rect.right; rcMargin.bottom = rcBoard.bottom;
            FillRect(hDC, &rcMargin, hbrBg);
        }

        /* 2. 标题文字直接画屏（在背景上，不闪） */
        SetBkMode(hDC, TRANSPARENT);
        SetTextColor(hDC, RGB(210, 210, 210));
        sprintf(buf, "2048  Score: %d", score);
        TextOut(hDC,
                (cxClient - (int)strlen(buf) * 8) / 2,
                4,
                buf, strlen(buf));
        if (game_over) {
            TextOut(hDC,
                    (cxClient - 32 * 8) / 2,
                    20,
                    "GAME OVER - Press N for New Game", 32);
        } else {
            TextOut(hDC,
                    (cxClient - 26 * 8) / 2,
                    20,
                    "Arrows: Move   N: New Game", 26);
        }

        /* 3. 游戏区域双缓冲——棋盘大小位图（约 280x280 4bpp ≈ 38KB，安全） */
        hdcMem = CreateCompatibleDC(hDC);
        if (hdcMem != NULL) {
            hbmMem = CreateCompatibleBitmap(hDC, boardPixels, boardPixels);
            if (hbmMem != NULL) {
                hbmOld = SelectObject(hdcMem, hbmMem);

                /* 在内存 DC 上画游戏区域（坐标从 0,0 开始） */
                DrawGameArea(hdcMem);

                /* BitBlt 到屏幕——原子替换，零闪烁 */
                BitBlt(hDC, boardX, boardY, boardPixels, boardPixels,
                       hdcMem, 0, 0, SRCCOPY);

                SelectObject(hdcMem, hbmOld);
                DeleteObject(hbmMem);
            } else {
                /* 位图创建失败（极小概率），直接画到屏幕
                 * 用 SetViewportOrg 偏移坐标 */
                oldOrg = SetViewportOrg(hDC, boardX, boardY);
                DrawGameArea(hDC);
                SetViewportOrg(hDC, LOWORD(oldOrg), HIWORD(oldOrg));
            }
            DeleteDC(hdcMem);
        } else {
            /* DC 创建失败，直接画 */
            oldOrg = SetViewportOrg(hDC, boardX, boardY);
            DrawGameArea(hDC);
            SetViewportOrg(hDC, LOWORD(oldOrg), HIWORD(oldOrg));
        }

        EndPaint(hWnd, &ps);
        break;

    case WM_KEYDOWN:
        if (game_over && wParam != 'N' && wParam != 'n')
            break;

        switch (wParam) {
        case VK_LEFT:   if (MoveLeft())   goto MOVED; break;
        case VK_RIGHT:  if (MoveRight())  goto MOVED; break;
        case VK_UP:     if (MoveUp())     goto MOVED; break;
        case VK_DOWN:   if (MoveDown())   goto MOVED; break;
        case 'N': case 'n':
            remove("2048.sav");
            g_bPlayerMoved = 0;
            InitGame();
            g_bDirty = 1;
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        }
        break;

MOVED:
        if (!g_bPlayerMoved) {
            remove("2048.sav");
            g_bPlayerMoved = 1;
        }
        AddRandomTile();
        g_bDirty = 1;
        if (!CanMove()) {
            game_over = 1;
            UpdateScoreHistory(score);
            DialogBox(hInst, MAKEINTRESOURCE(DLG_SCOREBOARD),
                      hWnd, lpfnScoreboardDlg);
        }
        InvalidateRect(hWnd, NULL, FALSE);
        break;

    case WM_COMMAND:
        switch (wParam) {
        case IDM_NEWGAME:
            remove("2048.sav");
            g_bPlayerMoved = 0;
            InitGame();
            g_bDirty = 1;
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        case IDM_SAVE:
            SaveGame();
            g_bDirty = 0;
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        case IDM_SCOREBOARD:
            DialogBox(hInst, MAKEINTRESOURCE(DLG_SCOREBOARD),
                      hWnd, lpfnScoreboardDlg);
            break;
        case IDM_HOWTOPLAY:
            DialogBox(hInst, MAKEINTRESOURCE(DLG_HOWTOPLAY),
                      hWnd, lpfnHowToPlayDlg);
            break;
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(DLG_ABOUT),
                      hWnd, lpfnAboutDlg);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;

    case WM_SYSCOMMAND:
        if (wParam == IDSYS_ABOUT) {
            DialogBox(hInst, MAKEINTRESOURCE(DLG_ABOUT),
                      hWnd, lpfnAboutDlg);
            return 0;
        }
        return DefWindowProc(hWnd, message, wParam, lParam);

    case WM_DESTROY:
        if (!game_over && g_bDirty && g_bPlayerMoved) {
            if (MessageBox(hWnd,
                "Ooh! Meow ^v^ You haven't saved your game!",
                "Save Game?",
                MB_YESNO | MB_ICONQUESTION) == IDYES) {
                SaveGame();
                g_bDirty = 0;
            }
        }
        if (hbrBg) {
            DeleteObject(hbrBg);
            hbrBg = NULL;
        }
        CleanupGDI();
        FreeProcInstance(lpfnAboutDlg);
        FreeProcInstance(lpfnHowToPlayDlg);
        FreeProcInstance(lpfnScoreboardDlg);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0L;
}

/* ============================================================
   WinMain – 主入口
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
    HMENU      hMenu, hSysMenu;

    srand((unsigned)GetCurrentTime());

    if (!hPrevInstance) {
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = WndProc;
        wc.cbClsExtra    = 0;
        wc.cbWndExtra    = 0;
        wc.hInstance     = hInstance;
        wc.hIcon         = LoadIcon(hInstance, (LPSTR)"AppIcon");
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = GetStockObject(WHITE_BRUSH);
        wc.lpszMenuName  = NULL;
        wc.lpszClassName = "2048W103";

        if (!RegisterClass(&wc))
            return FALSE;
    }

    hInst = hInstance;

    lpfnAboutDlg     = MakeProcInstance((FARPROC)AboutDlg, hInstance);
    lpfnHowToPlayDlg = MakeProcInstance((FARPROC)HowToPlayDlg, hInstance);
    lpfnScoreboardDlg= MakeProcInstance((FARPROC)ScoreboardDlg, hInstance);

    hMenu = LoadMenu(hInstance, (LPSTR)"GAME_MENU");
    if (!hMenu) {
        MessageBox(NULL, "Menu load failed!", NULL, MB_OK);
        return FALSE;
    }

    /* 初始窗口大小——棋盘最大 280 + 标题 36 + 边距，约 360x420 足够 */
    {
        int cxScreen = GetSystemMetrics(SM_CXSCREEN);
        int cyScreen = GetSystemMetrics(SM_CYSCREEN);
        int winW = min(360, cxScreen * 3 / 4);
        int winH = min(440, cyScreen * 3 / 4);
        int xPos = (cxScreen - winW) / 2;
        int yPos = (cyScreen - winH) / 2;

        hWnd = CreateWindow("2048W103",
                    "2048 - Windows 1.03",
                    WS_TILEDWINDOW,
                    xPos, yPos, winW, winH,
                    NULL,
                    hMenu,
                    hInstance,
                    NULL);
    }

    if (!hWnd) return FALSE;

    /* 启动时检测存档 */
    if (LoadGame()) {
        InvalidateRect(hWnd, NULL, FALSE);
        g_bDirty = 0;
        g_bPlayerMoved = 0;
    } else {
        InitGame();
        g_bDirty = 1;
        g_bPlayerMoved = 0;
    }

    /* 系统菜单添加 About */
    hSysMenu = GetSystemMenu(hWnd, FALSE);
    if (hSysMenu) {
        ChangeMenu(hSysMenu, 0, (LPSTR)NULL, 0, MF_APPEND | MF_SEPARATOR);
        ChangeMenu(hSysMenu, 0, (LPSTR)"About...",
                   IDSYS_ABOUT, MF_APPEND | MF_STRING);
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}

/* ============================================================
   游戏逻辑（完全不变）
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
    int count, pos, i, j;
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
    board[i][j] = (rand() % 10 == 0) ? 4 : 2;
}

int MoveLeft()
{
    int moved = 0, i, j, k;

    for (i = 0; i < BOARD_SIZE; i++) {
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
        for (j = 0; j < BOARD_SIZE - 1; j++) {
            if (board[i][j] != 0 && board[i][j] == board[i][j+1]) {
                board[i][j] *= 2;
                score += board[i][j];
                board[i][j+1] = 0;
                moved = 1;
            }
        }
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

int MoveRight()
{
    int moved, i, j, tmp;
    for (i = 0; i < BOARD_SIZE; i++)
        for (j = 0; j < BOARD_SIZE / 2; j++) {
            tmp = board[i][j];
            board[i][j] = board[i][BOARD_SIZE - 1 - j];
            board[i][BOARD_SIZE - 1 - j] = tmp;
        }
    moved = MoveLeft();
    for (i = 0; i < BOARD_SIZE; i++)
        for (j = 0; j < BOARD_SIZE / 2; j++) {
            tmp = board[i][j];
            board[i][j] = board[i][BOARD_SIZE - 1 - j];
            board[i][BOARD_SIZE - 1 - j] = tmp;
        }
    return moved;
}

int MoveUp()
{
    int moved, i, j, tmp;
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

int MoveDown()
{
    int moved, i, j, tmp;
    for (i = 0; i < BOARD_SIZE; i++)
        for (j = i + 1; j < BOARD_SIZE; j++) {
            tmp = board[i][j];
            board[i][j] = board[j][i];
            board[j][i] = tmp;
        }
    for (i = 0; i < BOARD_SIZE; i++)
        for (j = 0; j < BOARD_SIZE / 2; j++) {
            tmp = board[i][j];
            board[i][j] = board[i][BOARD_SIZE - 1 - j];
            board[i][BOARD_SIZE - 1 - j] = tmp;
        }
    moved = MoveLeft();
    for (i = 0; i < BOARD_SIZE; i++)
        for (j = 0; j < BOARD_SIZE / 2; j++) {
            tmp = board[i][j];
            board[i][j] = board[i][BOARD_SIZE - 1 - j];
            board[i][BOARD_SIZE - 1 - j] = tmp;
        }
    for (i = 0; i < BOARD_SIZE; i++)
        for (j = i + 1; j < BOARD_SIZE; j++) {
            tmp = board[i][j];
            board[i][j] = board[j][i];
            board[j][i] = tmp;
        }
    return moved;
}

int CanMove()
{
    int i, j;
    for (i = 0; i < BOARD_SIZE; i++)
        for (j = 0; j < BOARD_SIZE; j++)
            if (board[i][j] == 0) return 1;
    for (i = 0; i < BOARD_SIZE; i++)
        for (j = 0; j < BOARD_SIZE - 1; j++)
            if (board[i][j] == board[i][j+1]) return 1;
    for (i = 0; i < BOARD_SIZE - 1; i++)
        for (j = 0; j < BOARD_SIZE; j++)
            if (board[i][j] == board[i+1][j]) return 1;
    return 0;
}

/* ============================================================
   GDI 初始化/清理
   ============================================================ */

void InitGDI()
{
    hBrushes[0]  = GetStockObject(GRAY_BRUSH);            /* 空白格 */
    hBrushes[1]  = CreateSolidBrush(RGB(238, 228, 218));  /* 2 */
    hBrushes[2]  = CreateSolidBrush(RGB(237, 224, 200));  /* 4 */
    hBrushes[3]  = CreateSolidBrush(RGB(245, 149, 99));   /* 8 */
    hBrushes[4]  = CreateSolidBrush(RGB(245, 149, 99));   /* 16 */
    hBrushes[5]  = CreateSolidBrush(RGB(246, 124, 95));   /* 32 */
    hBrushes[6]  = CreateSolidBrush(RGB(246, 94, 59));    /* 64 */
    hBrushes[7]  = CreateSolidBrush(RGB(237, 207, 114));  /* 128 */
    hBrushes[8]  = CreateSolidBrush(RGB(237, 207, 114));  /* 256 */
    hBrushes[9]  = CreateSolidBrush(RGB(237, 207, 114));  /* 512 */
    hBrushes[10] = CreateSolidBrush(RGB(237, 207, 114));  /* 1024 */
    hBrushes[11] = CreateSolidBrush(RGB(237, 207, 114));  /* 2048 */
    hBrushes[12] = CreateSolidBrush(RGB(237, 207, 114));  /* 更大 */

    hPenBlack = GetStockObject(BLACK_PEN);
    hPenWhite = GetStockObject(WHITE_PEN);
}

void CleanupGDI()
{
    int i;
    if (hFont) {
        DeleteObject(hFont);
        hFont = NULL;
    }
    for (i = 1; i < 13; i++) {
        if (hBrushes[i])
            DeleteObject(hBrushes[i]);
    }
}

int ValueToIndex(value)
int value;
{
    switch (value) {
        case 0:    return 0;
        case 2:    return 1;
        case 4:    return 2;
        case 8:    return 3;
        case 16:   return 4;
        case 32:   return 5;
        case 64:   return 6;
        case 128:  return 7;
        case 256:  return 8;
        case 512:  return 9;
        case 1024: return 10;
        case 2048: return 11;
        default:   return 12;
    }
}

/* ============================================================
   对话框过程（完全不变）
   ============================================================ */

BOOL FAR PASCAL AboutDlg(hDlg, message, wParam, lParam)
HWND     hDlg;
unsigned message;
WORD     wParam;
LONG     lParam;
{
    if (message == WM_COMMAND && wParam == IDOK) {
        EndDialog(hDlg, TRUE);
        return TRUE;
    }
    if (message == WM_INITDIALOG) return TRUE;
    return FALSE;
}

BOOL FAR PASCAL HowToPlayDlg(hDlg, message, wParam, lParam)
HWND     hDlg;
unsigned message;
WORD     wParam;
LONG     lParam;
{
    if (message == WM_COMMAND && wParam == IDOK) {
        EndDialog(hDlg, TRUE);
        return TRUE;
    }
    if (message == WM_INITDIALOG) return TRUE;
    return FALSE;
}

/* ============================================================
   存档/分数（完全不变）
   ============================================================ */

void SaveGame()
{
    FILE *fp;
    int i, j;
    fp = fopen("2048.sav", "w");
    if (fp == NULL) return;
    fprintf(fp, "%d %d\n", score, game_over);
    for (i = 0; i < BOARD_SIZE; i++) {
        for (j = 0; j < BOARD_SIZE; j++)
            fprintf(fp, "%d ", board[i][j]);
        fprintf(fp, "\n");
    }
    fclose(fp);
}

int LoadGame()
{
    FILE *fp;
    int i, j;
    fp = fopen("2048.sav", "r");
    if (fp == NULL) return 0;
    if (fscanf(fp, "%d %d", &score, &game_over) != 2) {
        fclose(fp); return 0;
    }
    for (i = 0; i < BOARD_SIZE; i++)
        for (j = 0; j < BOARD_SIZE; j++) {
            if (fscanf(fp, "%d", &board[i][j]) != 1) {
                fclose(fp); return 0;
            }
        }
    fclose(fp);
    return 1;
}

BOOL GetScoreHistory(pHist)
ScoreHistory *pHist;
{
    FILE *fp;
    fp = fopen("2048.his", "rb");
    if (fp == NULL) return FALSE;
    if (fread(pHist, sizeof(ScoreHistory), 1, fp) != 1) {
        fclose(fp); return FALSE;
    }
    fclose(fp);
    return TRUE;
}

void SetScoreHistory(pHist)
ScoreHistory *pHist;
{
    FILE *fp;
    fp = fopen("2048.his", "wb");
    if (fp == NULL) return;
    fwrite(pHist, sizeof(ScoreHistory), 1, fp);
    fclose(fp);
}

void UpdateScoreHistory(newScore)
int newScore;
{
    ScoreHistory hist;
    int i;
    if (!GetScoreHistory(&hist)) {
        hist.highscore = 0;
        hist.count = 0;
        for (i = 0; i < 5; i++) hist.recent[i] = 0;
    }
    if (newScore > hist.highscore)
        hist.highscore = newScore;
    for (i = 4; i > 0; i--)
        hist.recent[i] = hist.recent[i - 1];
    hist.recent[0] = newScore;
    if (hist.count < 5) hist.count++;
    SetScoreHistory(&hist);
}

BOOL FAR PASCAL ScoreboardDlg(hDlg, message, wParam, lParam)
HWND     hDlg;
unsigned message;
WORD     wParam;
LONG     lParam;
{
    ScoreHistory hist;
    char buf[512], line[64];
    int i;

    switch (message) {
    case WM_INITDIALOG:
        if (!GetScoreHistory(&hist)) {
            hist.highscore = 0;
            hist.count = 0;
        }
        buf[0] = '\0';
        sprintf(buf + strlen(buf), "Best Score: %d\r\n", hist.highscore);
        sprintf(buf + strlen(buf), "------------------------\r\n");
        sprintf(buf + strlen(buf), "Recent Scores:\r\n");
        for (i = 0; i < hist.count && i < 5; i++) {
            sprintf(line, "%d. %d\r\n", i + 1, hist.recent[i]);
            strcat(buf, line);
        }
        sprintf(buf + strlen(buf), "------------------------\r\n");
        sprintf(buf + strlen(buf), "Current Score: %d", score);
        SetDlgItemText(hDlg, IDC_SCOREBOARD_EDIT, buf);
        SetFocus(GetDlgItem(hDlg, IDOK));
        return FALSE;

    case WM_COMMAND:
        if (wParam == IDOK) {
            EndDialog(hDlg, TRUE);
            return TRUE;
        }
        break;
    }
    return FALSE;
}
