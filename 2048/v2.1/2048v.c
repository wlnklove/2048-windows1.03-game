/* 2048.c - Windows 1.03 Edition v2.2                         */
/* 格子缓存 + IM1024增量更新 + 垂直同步                       */
/* 夏媛媛的可爱优化版 ^_^ */
/* 一点也不嘻嘻！im1024显卡的闪烁我怎么也修不好！我头好痛！*/
/* 本来想添加按c键切换颜色的！毕竟原始设计就是支持256 color的但是闪烁我的256色显卡闪烁！没心情了我默认256色棋盘！提供一个16色背景的版本！没兴趣了啊啊啊啊*/

#include <windows.h>
#include <conio.h>    /* inp() for WaitVBlank */
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

#define BOARD_SIZE    4
#define MARGIN        8        /* 窗口边距 */
#define TITLE_HEIGHT  36       /* 标题区高度 */

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

/* ====== IM1024 模式（V键切换，无自动检测） ====== */
int  g_bIM1024 = FALSE;

/* GDI对象缓存——只创建一次，WM_DESTROY 统一清理 */
HBRUSH hBrushes[13];        /* 数字格子颜色（0=空白, 1=2, 2=4, ...） */
HPEN   hPenBlack, hPenWhite;
HBRUSH hbrBg = NULL;        /* 背景画刷 */
HBRUSH hbrBoard = NULL;     /* 棋盘底板画刷 */
HFONT  hFont = NULL;        /* 动态缩放数字字体 */

/* ====== 格子缓存（IM1024 模式用） ====== */
HDC     hdcCellMem = NULL;               /* 格子内存 DC */
HBITMAP hbmCells[BOARD_SIZE][BOARD_SIZE]; /* 16 个格子位图 */
int     oldBoard[BOARD_SIZE][BOARD_SIZE]; /* 格子位图当前内容（检测变化） */

/* 布局变量 */
static int cxClient, cyClient;
static int cellSize;
static int gapSize;
static int boardPixels;
static int boardX, boardY;

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
void WaitVBlank();
void InitCellCache();
void FreeCellCache();
void RenderAllCells();
void DrawBoardWithCells();
void DrawTitleBar();
long FAR PASCAL WndProc();
BOOL FAR PASCAL AboutDlg();
BOOL FAR PASCAL HowToPlayDlg();

/* ============================================================
   垂直同步——等待垂直消隐开始
   ============================================================ */
void WaitVBlank()
{
    /* 等当前消隐结束 */
    while (inp(0x3DA) & 0x08)
        ;
    /* 等下一个消隐开始 */
    while (!(inp(0x3DA) & 0x08))
        ;
}

/* ============================================================
   格子缓存——预渲染 16 个格子位图，移动时只更新变化的格子
   ============================================================ */

/* 初始化格子缓存（进入 IM1024 模式或窗口尺寸变化时调用） */
void InitCellCache(hDC)
HDC hDC;
{
    int i, j;
    hdcCellMem = CreateCompatibleDC(hDC);
    if (!hdcCellMem) return;
    for (i = 0; i < BOARD_SIZE; i++) {
        for (j = 0; j < BOARD_SIZE; j++) {
            hbmCells[i][j] = CreateCompatibleBitmap(hDC, cellSize, cellSize);
            oldBoard[i][j] = -1;  /* 强制全部重渲染 */
        }
    }
}

/* 释放格子缓存（退出 IM1024 模式或 WM_DESTROY 时调用） */
void FreeCellCache()
{
    int i, j;
    if (hdcCellMem) {
        for (i = 0; i < BOARD_SIZE; i++) {
            for (j = 0; j < BOARD_SIZE; j++) {
                if (hbmCells[i][j]) {
                    DeleteObject(hbmCells[i][j]);
                    hbmCells[i][j] = NULL;
                }
            }
        }
        DeleteDC(hdcCellMem);
        hdcCellMem = NULL;
    }
}

/* 重新渲染所有格子位图（全量渲染，用于首次绘制或布局变化后） */
void RenderAllCells()
{
    int i, j;
    HBITMAP hbmOld;
    if (!hdcCellMem) return;
    for (i = 0; i < BOARD_SIZE; i++) {
        for (j = 0; j < BOARD_SIZE; j++) {
            if (!hbmCells[i][j]) continue;
            hbmOld = SelectObject(hdcCellMem, hbmCells[i][j]);
            DrawCell3D(hdcCellMem, board[i][j], 0, 0, cellSize, cellSize);
            SelectObject(hdcCellMem, hbmOld);
            oldBoard[i][j] = board[i][j];
        }
    }
}

/* 用格子缓存绘制整个棋盘到屏幕 DC */
void DrawBoardWithCells(hDC)
HDC hDC;
{
    int i, j, x, y;
    HBITMAP hbmOld;
    for (i = 0; i < BOARD_SIZE; i++) {
        for (j = 0; j < BOARD_SIZE; j++) {
            if (!hbmCells[i][j]) continue;
            x = boardX + gapSize + j * (cellSize + gapSize);
            y = boardY + gapSize + i * (cellSize + gapSize);
            hbmOld = SelectObject(hdcCellMem, hbmCells[i][j]);
            BitBlt(hDC, x, y, cellSize, cellSize, hdcCellMem, 0, 0, SRCCOPY);
            SelectObject(hdcCellMem, hbmOld);
        }
    }
}

/* 绘制标题栏（分数 + 操作提示） */
void DrawTitleBar(hDC)
HDC hDC;
{
    char buf[80];
    RECT rcTitle;
    int  len;

    /* 清除标题区背景 */
    rcTitle.left = 0; rcTitle.top = 0;
    rcTitle.right = cxClient; rcTitle.bottom = TITLE_HEIGHT;
    FillRect(hDC, &rcTitle, hbrBg);

    SetBkMode(hDC, TRANSPARENT);
    SetTextColor(hDC, RGB(0, 0, 0));

    /* 分数行 */
    if (g_bIM1024)
        sprintf(buf, "2048  Score: %d  [IM1024]", score);
    else
        sprintf(buf, "2048  Score: %d", score);
    len = strlen(buf);
    TextOut(hDC, (cxClient - len * 8) / 2, 4, buf, len);

    /* 操作提示行 */
    if (game_over) {
        sprintf(buf, "GAME OVER - Press N for New Game");
    } else {
        sprintf(buf, "Arrows: Move  N: New  V: IM1024");
    }
    len = strlen(buf);
    TextOut(hDC, (cxClient - len * 8) / 2, 20, buf, len);
}

/* ============================================================
   布局计算——动态棋盘尺寸（随分辨率和窗口大小自适应）
   ============================================================ */
void CalcLayout(cx, cy)
int cx, cy;
{
    int availW, availH, maxBoard;
    int cxScreen, cyScreen, screenMax;

    cxClient = cx;
    cyClient = cy;

    cxScreen = GetSystemMetrics(SM_CXSCREEN);
    cyScreen = GetSystemMetrics(SM_CYSCREEN);

    /* 动态最大棋盘尺寸：屏幕较小维度的 40%
     * 钳位 [180, 260]——比走四子的 280 略小
     * 640x480 → 180px, 800x600 → 240px, 1024x768+ → 260px */
    screenMax = (cxScreen < cyScreen) ? cxScreen : cyScreen;
    screenMax = screenMax * 2 / 5;
    if (screenMax > 260) screenMax = 260;
    if (screenMax < 180) screenMax = 180;

    availW = cx - 2 * MARGIN;
    availH = cy - TITLE_HEIGHT - MARGIN;

    maxBoard = (availW < availH) ? availW : availH;
    if (maxBoard > screenMax) maxBoard = screenMax;

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
   3D 边框绘制
   ============================================================ */

/* 凹陷边框——游戏区域"凹下去" */
void DrawSunkenRect(hDC, x, y, w, h)
HDC hDC;
int x, y, w, h;
{
    HPEN hOld;

    hOld = SelectObject(hDC, hPenBlack);
    MoveTo(hDC, x, y);           LineTo(hDC, x + w, y);
    MoveTo(hDC, x, y);           LineTo(hDC, x, y + h);
    SelectObject(hDC, hPenWhite);
    MoveTo(hDC, x, y + h - 1);   LineTo(hDC, x + w, y + h - 1);
    MoveTo(hDC, x + w - 1, y);   LineTo(hDC, x + w - 1, y + h);

    SelectObject(hDC, hPenWhite);
    MoveTo(hDC, x + 1, y + 1);       LineTo(hDC, x + w - 1, y + 1);
    MoveTo(hDC, x + 1, y + 1);       LineTo(hDC, x + 1, y + h - 1);
    SelectObject(hDC, hPenBlack);
    MoveTo(hDC, x + 1, y + h - 2);   LineTo(hDC, x + w - 1, y + h - 2);
    MoveTo(hDC, x + w - 2, y + 1);   LineTo(hDC, x + w - 2, y + h - 1);

    SelectObject(hDC, hOld);
}

/* 凸起边框——格子"浮起来" */
void DrawRaisedRect(hDC, x, y, w, h)
HDC hDC;
int x, y, w, h;
{
    HPEN hOld;

    hOld = SelectObject(hDC, hPenWhite);
    MoveTo(hDC, x, y);           LineTo(hDC, x + w, y);
    MoveTo(hDC, x, y);           LineTo(hDC, x, y + h);
    SelectObject(hDC, hPenBlack);
    MoveTo(hDC, x, y + h - 1);   LineTo(hDC, x + w, y + h - 1);
    MoveTo(hDC, x + w - 1, y);   LineTo(hDC, x + w - 1, y + h);

    SelectObject(hDC, hPenBlack);
    MoveTo(hDC, x + 1, y + 1);       LineTo(hDC, x + w - 1, y + 1);
    MoveTo(hDC, x + 1, y + 1);       LineTo(hDC, x + 1, y + h - 1);
    SelectObject(hDC, hPenWhite);
    MoveTo(hDC, x + 1, y + h - 2);   LineTo(hDC, x + w - 1, y + h - 2);
    MoveTo(hDC, x + w - 2, y + 1);   LineTo(hDC, x + w - 2, y + h - 1);

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

    /* 1. 底板填充 */
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

    rc.left = x; rc.top = y;
    rc.right = x + w; rc.bottom = y + h;
    FillRect(hDC, &rc, hBrush);

    DrawRaisedRect(hDC, x, y, w, h);

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
    char        buf[80];
    RECT        rcBoard, rcMargin;
    HDC         hdcMem;
    HBITMAP     hbmMem, hbmOld;
    DWORD       oldOrg;
    int         ci, cj, cx2, cy2;
    HBITMAP     hbmOldCell;
    RECT        rcTitle;

    switch (message) {

    case WM_CREATE:
        hWndMain = hWnd;
        InitGame();
        InitGDI();
        {
            RECT cr;
            GetClientRect(hWnd, &cr);
            CalcLayout(cr.right, cr.bottom);
        }
        break;

    case WM_SIZE:
        CalcLayout(LOWORD(lParam), HIWORD(lParam));
        /* IM1024 模式下格子尺寸变了，重建格子缓存 */
        if (g_bIM1024 && hdcCellMem) {
            FreeCellCache();
            hDC = GetDC(hWnd);
            if (hDC) {
                InitCellCache(hDC);
                ReleaseDC(hWnd, hDC);
            }
        }
        InvalidateRect(hWnd, NULL, FALSE);
        break;

    case WM_ERASEBKGND:
        return 1L;

    /* ========================================================
       WM_PAINT - 挖洞双缓冲（普通模式）/ 格子缓存（IM1024 模式）
       ======================================================== */
    case WM_PAINT:
        hDC = BeginPaint(hWnd, &ps);
        GetClientRect(hWnd, &rect);

        rcBoard.left = boardX;
        rcBoard.top = boardY;
        rcBoard.right = boardX + boardPixels;
        rcBoard.bottom = boardY + boardPixels;

        /* 1. 背景挖洞——只画游戏区域以外的边距 */
        if (rcBoard.top > 0) {
            rcMargin.left = 0; rcMargin.top = 0;
            rcMargin.right = rect.right; rcMargin.bottom = rcBoard.top;
            FillRect(hDC, &rcMargin, hbrBg);
        }
        if (rcBoard.bottom < rect.bottom) {
            rcMargin.left = 0; rcMargin.top = rcBoard.bottom;
            rcMargin.right = rect.right; rcMargin.bottom = rect.bottom;
            FillRect(hDC, &rcMargin, hbrBg);
        }
        if (rcBoard.left > 0) {
            rcMargin.left = 0; rcMargin.top = rcBoard.top;
            rcMargin.right = rcBoard.left; rcMargin.bottom = rcBoard.bottom;
            FillRect(hDC, &rcMargin, hbrBg);
        }
        if (rcBoard.right < rect.right) {
            rcMargin.left = rcBoard.right; rcMargin.top = rcBoard.top;
            rcMargin.right = rect.right; rcMargin.bottom = rcBoard.bottom;
            FillRect(hDC, &rcMargin, hbrBg);
        }

        /* 2. 标题文字 */
        DrawTitleBar(hDC);

        /* 3. 游戏区域 */
        if (g_bIM1024 && hdcCellMem) {
            /* === IM1024 模式：格子缓存 === */

            /* 3a. 棋盘底板直接画屏（纯色 FillRect + 线框，不闪） */
            rcBoard.left = boardX; rcBoard.top = boardY;
            rcBoard.right = boardX + boardPixels;
            rcBoard.bottom = boardY + boardPixels;
            FillRect(hDC, &rcBoard, hbrBoard);
            DrawSunkenRect(hDC, boardX, boardY, boardPixels, boardPixels);

            /* 3b. 更新变化的格子位图 */
            for (ci = 0; ci < BOARD_SIZE; ci++) {
                for (cj = 0; cj < BOARD_SIZE; cj++) {
                    if (board[ci][cj] != oldBoard[ci][cj]) {
                        if (!hbmCells[ci][cj]) continue;
                        hbmOldCell = SelectObject(hdcCellMem, hbmCells[ci][cj]);
                        DrawCell3D(hdcCellMem, board[ci][cj], 0, 0,
                                   cellSize, cellSize);
                        SelectObject(hdcCellMem, hbmOldCell);
                        oldBoard[ci][cj] = board[ci][cj];
                    }
                }
            }

            /* 3c. 垂直同步后 BitBlt 所有格子 */
            WaitVBlank();
            DrawBoardWithCells(hDC);

        } else {
            /* === 普通模式：整板双缓冲 === */
            hdcMem = CreateCompatibleDC(hDC);
            if (hdcMem != NULL) {
                hbmMem = CreateCompatibleBitmap(hDC, boardPixels, boardPixels);
                if (hbmMem != NULL) {
                    hbmOld = SelectObject(hdcMem, hbmMem);
                    DrawGameArea(hdcMem);
                    BitBlt(hDC, boardX, boardY, boardPixels, boardPixels,
                           hdcMem, 0, 0, SRCCOPY);
                    SelectObject(hdcMem, hbmOld);
                    DeleteObject(hbmMem);
                } else {
                    oldOrg = SetViewportOrg(hDC, boardX, boardY);
                    DrawGameArea(hDC);
                    SetViewportOrg(hDC, LOWORD(oldOrg), HIWORD(oldOrg));
                }
                DeleteDC(hdcMem);
            } else {
                oldOrg = SetViewportOrg(hDC, boardX, boardY);
                DrawGameArea(hDC);
                SetViewportOrg(hDC, LOWORD(oldOrg), HIWORD(oldOrg));
            }
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
            /* IM1024 模式下标记全部格子需要重渲染 */
            if (g_bIM1024 && hdcCellMem) {
                for (ci = 0; ci < BOARD_SIZE; ci++)
                    for (cj = 0; cj < BOARD_SIZE; cj++)
                        oldBoard[ci][cj] = -1;
            }
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        case 'V': case 'v':
            /* IM1024 模式切换——无弹窗，无检测 */
            g_bIM1024 = !g_bIM1024;
            if (g_bIM1024) {
                /* 进入 IM1024 模式：创建格子缓存 */
                hDC = GetDC(hWnd);
                if (hDC) {
                    InitCellCache(hDC);
                    ReleaseDC(hWnd, hDC);
                }
            } else {
                /* 退出 IM1024 模式：释放格子缓存 */
                FreeCellCache();
            }
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
            /* 游戏结束：对话框遮过窗口，需要全量重绘 */
            InvalidateRect(hWnd, NULL, FALSE);
        } else if (g_bIM1024 && hdcCellMem) {
            /* === IM1024 增量更新：只更新变化的格子 === */
            hDC = GetDC(hWnd);
            if (hDC) {
                /* 更新标题栏（分数变化） */
                DrawTitleBar(hDC);

                /* 更新变化的格子位图并直接 BitBlt 到屏幕 */
                WaitVBlank();
                for (ci = 0; ci < BOARD_SIZE; ci++) {
                    for (cj = 0; cj < BOARD_SIZE; cj++) {
                        if (board[ci][cj] != oldBoard[ci][cj]) {
                            if (!hbmCells[ci][cj]) continue;
                            cx2 = boardX + gapSize + cj * (cellSize + gapSize);
                            cy2 = boardY + gapSize + ci * (cellSize + gapSize);
                            hbmOldCell = SelectObject(hdcCellMem, hbmCells[ci][cj]);
                            DrawCell3D(hdcCellMem, board[ci][cj], 0, 0,
                                       cellSize, cellSize);
                            BitBlt(hDC, cx2, cy2, cellSize, cellSize,
                                   hdcCellMem, 0, 0, SRCCOPY);
                            SelectObject(hdcCellMem, hbmOldCell);
                            oldBoard[ci][cj] = board[ci][cj];
                        }
                    }
                }
                ReleaseDC(hWnd, hDC);
            }
            /* 不调 InvalidateRect——只有变化的格子被更新 */
        } else {
            /* 普通模式：全量重绘 */
            InvalidateRect(hWnd, NULL, FALSE);
        }
        break;

    case WM_COMMAND:
        switch (wParam) {
        case IDM_NEWGAME:
            remove("2048.sav");
            g_bPlayerMoved = 0;
            InitGame();
            g_bDirty = 1;
            if (g_bIM1024 && hdcCellMem) {
                for (ci = 0; ci < BOARD_SIZE; ci++)
                    for (cj = 0; cj < BOARD_SIZE; cj++)
                        oldBoard[ci][cj] = -1;
            }
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
        FreeCellCache();
        if (hbrBg) {
            DeleteObject(hbrBg);
            hbrBg = NULL;
        }
        if (hbrBoard) {
            DeleteObject(hbrBoard);
            hbrBoard = NULL;
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

    /* 初始窗口大小——基于屏幕分辨率动态计算 */
    {
        int cxScreen = GetSystemMetrics(SM_CXSCREEN);
        int cyScreen = GetSystemMetrics(SM_CYSCREEN);
        int screenMax = (cxScreen < cyScreen) ? cxScreen : cyScreen;
        int winW, winH;

        screenMax = screenMax * 2 / 5;
        if (screenMax > 260) screenMax = 260;
        if (screenMax < 180) screenMax = 180;

        winW = screenMax + 2 * MARGIN;
        winH = screenMax + TITLE_HEIGHT + MARGIN;
        if (winW > cxScreen * 3 / 4) winW = cxScreen * 3 / 4;
        if (winH > cyScreen * 3 / 4) winH = cyScreen * 3 / 4;

        {
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
    }

    if (!hWnd) return FALSE;

    if (LoadGame()) {
        InvalidateRect(hWnd, NULL, FALSE);
        g_bDirty = 0;
        g_bPlayerMoved = 0;
    } else {
        InitGame();
        g_bDirty = 1;
        g_bPlayerMoved = 0;
    }

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
   GDI 初始化/清理——柔和灰配色
   ============================================================ */

void InitGDI()
{
    /* 格子颜色——保留经典 2048 暖色调 */
    hBrushes[0]  = CreateSolidBrush(RGB(160, 160, 160));  /* 空白格 */
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

    /* 柔和灰配色 */
    hbrBg    = CreateSolidBrush(RGB(192, 192, 192));
    hbrBoard = CreateSolidBrush(RGB(128, 128, 128));
}

void CleanupGDI()
{
    int i;
    if (hFont) {
        DeleteObject(hFont);
        hFont = NULL;
    }
    for (i = 0; i < 13; i++) {
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
   对话框过程
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
