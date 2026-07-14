/* 2048.c - Windows 1.03 Edition          */
/* 视口自适应 + 居中 + 灰色背景 + 动态字体 */
/* 夏媛媛的可爱优化版 ^_^                  */

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>    /* sprintf */
#include <string.h>   /* strlen */
#include "2048.h"     /* 资源ID定义 */

void SaveGame();
int  LoadGame();

/* 分数历史记录（二进制文件） */
typedef struct {
    int highscore;
    int recent[5];
    int count;
} ScoreHistory;

BOOL GetScoreHistory();
void SetScoreHistory();
void UpdateScoreHistory();
BOOL FAR PASCAL ScoreboardDlg();

#define BOARD_SIZE 4
#define MARGIN     8      /* 用于标题文字（在居中计算中不再用） */

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

/* GDI对象缓存 */
HBRUSH hBrushes[13];
HPEN   hPenBlack, hPenWhite;
RECT   rcGame;      /* 保留但不再使用（可删，为兼容不删） */

/* ========== [新增] 视口自适应相关全局变量 ========== */
static int cxClient, cyClient;       /* 当前客户区宽高 */
static int cellSize;                 /* 当前格子大小（像素） */
static int boardPixels;              /* 棋盘总边长 */
static int gapSize;                  /* 格子间间隙 */
static int xOffset, yOffset;         /* 棋盘左上角居中偏移 */
static HFONT hFont = NULL;          /* 动态缩放的数字字体 */

/* 函数前置声明 */
void InitGame();
void AddRandomTile();
int  MoveLeft();
int  MoveRight();
int  MoveUp();
int  MoveDown();
int  CanMove();
void DrawBoard();
void DrawCell();
void InitGDI();
void CleanupGDI();
int  ValueToIndex();
long FAR PASCAL WndProc();
BOOL FAR PASCAL AboutDlg();
BOOL FAR PASCAL HowToPlayDlg();

/* ============================================================
   窗口过程（视口自适应 + 居中 + 背景色）
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
        hWndMain = hWnd;
        InitGame();
        InitGDI();
		 /* ===== [新增] 手动初始化视口变量，保证第一次绘制格子正确 ===== */
    {
        RECT cr;
        GetClientRect(hWnd, &cr);
        /* 模拟 WM_SIZE 计算逻辑 */
        cxClient = cr.right;
        cyClient = cr.bottom;
        {
            int availW = cxClient - 2 * MARGIN;
            int availH = cyClient - 45 - MARGIN;
            int maxBoard = (availW < availH) ? availW : availH;
            int gap = maxBoard / 20;
            if (gap < 2) gap = 2;
            gapSize = gap;
            cellSize = (maxBoard - 3 * gap) / 4;
            if (cellSize < 10) cellSize = 10;
            boardPixels = 4 * cellSize + 3 * gap;
            xOffset = (cxClient - boardPixels) / 2;
            yOffset = 45 + (cyClient - 45 - boardPixels) / 2;
            if (yOffset < 45) yOffset = 45;
            /* 创建动态字体 */
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
    }
        /* rcGame 不再使用，保留旧赋值兼容 */
        rcGame.left = rcGame.top = 0;
        rcGame.right = 280;  rcGame.bottom = 320;
        break;

    /* [修改] 响应窗口尺寸变化，计算棋盘动态尺寸 */
    case WM_SIZE:
        cxClient = LOWORD(lParam);
        cyClient = HIWORD(lParam);
        {
            int availW = cxClient - 2 * MARGIN;
            int availH = cyClient - 45 - MARGIN;
            int maxBoard = (availW < availH) ? availW : availH;
            int gap;

            gap = maxBoard / 20;
            if (gap < 2) gap = 2;
            gapSize = gap;

            cellSize = (maxBoard - 3 * gap) / 4;
            if (cellSize < 10) cellSize = 10;           /* 最小保护 */
            boardPixels = 4 * cellSize + 3 * gap;

            xOffset = (cxClient - boardPixels) / 2;
            yOffset = 45 + (cyClient - 45 - boardPixels) / 2;
            if (yOffset < 45) yOffset = 45;             /* 防止遮住标题 */

            /* 动态创建等宽粗体字体，让数字跟着格子变大 */
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
        /* 触发全窗口重绘 */
        InvalidateRect(hWnd, NULL, TRUE);
        break;

    case WM_PAINT:
        hDC = BeginPaint(hWnd, &ps);
        GetClientRect(hWnd, &rect);

        /* 背景改为深蓝色（纯色，16色支持，护眼且区分空格子） */
		{
			HBRUSH hBlueBrush = CreateSolidBrush(RGB(0,0,255));  /* 深蓝 */
			HBRUSH hOldBrush = SelectObject(hDC, hBlueBrush);
			PatBlt(hDC, rect.left, rect.top,
				   rect.right - rect.left,
				   rect.bottom - rect.top, PATCOPY);
			SelectObject(hDC, hOldBrush);
			DeleteObject(hBlueBrush);   /* 自己创建的刷子要记得删除！ */
		}

        /* 设置透明背景，文字清晰 */
        SetBkMode(hDC, TRANSPARENT);
        SetTextColor(hDC, RGB(210, 210, 210));

        /* [修改] 标题文字水平居中 */
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

        DrawBoard(hDC);
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
            InvalidateRect(hWnd, NULL, TRUE);
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
        InvalidateRect(hWnd, NULL, TRUE);
        break;

    case WM_COMMAND:
        switch (wParam) {
        case IDM_NEWGAME:
            remove("2048.sav");
            g_bPlayerMoved = 0;
            InitGame();
            g_bDirty = 1;
            InvalidateRect(hWnd, NULL, TRUE);
            break;
        case IDM_SAVE:
            SaveGame();
            g_bDirty = 0;
            InvalidateRect(hWnd, NULL, TRUE);
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
        CleanupGDI();        /* 包含删除 hFont */
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
   WinMain – 主入口（根据屏幕分辨率设定初始窗口大小且居中）
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

    /* [修改] 根据屏幕分辨率计算初始窗口大小，让窗口在高分下更大 */
    {
        int cxScreen = GetSystemMetrics(SM_CXSCREEN);
        int cyScreen = GetSystemMetrics(SM_CYSCREEN);
        int winW = min(500, cxScreen * 3 / 4);
        int winH = min(600, cyScreen * 3 / 4);
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
        InvalidateRect(hWnd, NULL, TRUE);
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
        /* 压缩 */
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
        /* 合并 */
        for (j = 0; j < BOARD_SIZE - 1; j++) {
            if (board[i][j] != 0 && board[i][j] == board[i][j+1]) {
                board[i][j] *= 2;
                score += board[i][j];
                board[i][j+1] = 0;
                moved = 1;
            }
        }
        /* 再压缩 */
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
    /* 转置 */
    for (i = 0; i < BOARD_SIZE; i++)
        for (j = i + 1; j < BOARD_SIZE; j++) {
            tmp = board[i][j];
            board[i][j] = board[j][i];
            board[j][i] = tmp;
        }
    /* 水平翻转 */
    for (i = 0; i < BOARD_SIZE; i++)
        for (j = 0; j < BOARD_SIZE / 2; j++) {
            tmp = board[i][j];
            board[i][j] = board[i][BOARD_SIZE - 1 - j];
            board[i][BOARD_SIZE - 1 - j] = tmp;
        }
    moved = MoveLeft();
    /* 翻转回来 */
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
   GDI 绘图（全部改为动态坐标）
   ============================================================ */

/* [修改] 每个格子使用全局字体，文字居中采用 GetTextExtent */
void DrawCell(hDC, value, x, y, w, h)
HDC hDC;
int value, x, y, w, h;
{
    char buf[16];
    int  len, idx;
    HBRUSH hBrush, hOldBrush;
    HPEN   hPen, hOldPen;
    HFONT  hOldFont;
    LONG   extent;
    int    textW, textH;

    idx = ValueToIndex(value);
    hBrush = hBrushes[idx];
    hPen = (value >= 8 && value < 128) ? hPenWhite : hPenBlack;

    hOldBrush = SelectObject(hDC, hBrush);
    hOldPen   = SelectObject(hDC, hPen);
    Rectangle(hDC, x, y, x + w, y + h);
    SelectObject(hDC, hOldBrush);
    SelectObject(hDC, hOldPen);

    if (value != 0) {
        SetBkMode(hDC, TRANSPARENT);
        SetTextColor(hDC, (value >= 8 && value < 128) ?
                          RGB(255,255,255) : RGB(0,0,0));

        /* 选择动态字体并测量文字宽度 */
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

/* [修改] 使用全局偏移量、格子大小和间距 */
void DrawBoard(hDC)
HDC hDC;
{
    int i, j;
    int x, y;
    for (i = 0; i < BOARD_SIZE; i++) {
        for (j = 0; j < BOARD_SIZE; j++) {
            x = xOffset + j * (cellSize + gapSize);
            y = yOffset + i * (cellSize + gapSize);
            DrawCell(hDC, board[i][j], x, y, cellSize, cellSize);
        }
    }
}

/* [修改] 初始化 GDI：空白格用白色，数字8用与16相同的颜色 */
void InitGDI()
{
    hBrushes[0]  = GetStockObject(GRAY_BRUSH); /* 空白格白色 */
    hBrushes[1]  = CreateSolidBrush(RGB(238, 228, 218));  /* 2 */
    hBrushes[2]  = CreateSolidBrush(RGB(237, 224, 200));  /* 4 */
    hBrushes[3]  = CreateSolidBrush(RGB(245, 149, 99));   /* 8 → 与16同色，更易辨认 */
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

/* [修改] 清理GDI时加上删除 hFont */
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

/* 对话框过程（不变） */
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