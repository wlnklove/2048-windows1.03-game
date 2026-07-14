/* 2048.c - Windows 1.03 Edition */
/* Microsoft C 5.0 + Windows SDK 1.03 */
/* K&R C style - 夏媛媛的师傅手敲 */

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>    /* sprintf */
#include <string.h>   /* strlen */
#include "2048.h"     /* 资源ID定义（IDM_NEWGAME, IDM_HOWTOPLAY, IDM_ABOUT, IDSYS_ABOUT, DLG_ABOUT, DLG_HOWTOPLAY） */

void SaveGame();
int  LoadGame();

/* ======= 分数历史记录（二进制文件，防玩家修改） ======= */
typedef struct {
    int highscore;          /* 历史最高分 */
    int recent[5];          /* 最近 5 局分数（下标 0 为最新） */
    int count;              /* 实际记录局数（<=5） */
} ScoreHistory;

/* 分数历史文件操作（K&R 旧式声明，只写参数名） */
BOOL GetScoreHistory();
void SetScoreHistory();
void UpdateScoreHistory();

/* 分数板对话框过程（K&R 旧式声明） */
BOOL FAR PASCAL ScoreboardDlg();

#define BOARD_SIZE 4
#define CELL_W     55
#define CELL_H     55
#define MARGIN     8

/* 全局数据段变量 */
int  board[BOARD_SIZE][BOARD_SIZE];
int  score;
int  game_over;
int g_bDirty = 0; /* 新增：脏标记，0=状态与存档一致，1=有未保存改动 */
int g_bPlayerMoved = 0;  /* 玩家是否进行过主动操作 */
HBRUSH hBrush, hOldBrush;
HWND hWndMain;
HANDLE hInst;                  /* 实例句柄 */
FARPROC lpfnAboutDlg;          /* About 对话框过程实例地址 */
FARPROC lpfnHowToPlayDlg;      /* HowToPlay 对话框过程实例地址 */
FARPROC lpfnScoreboardDlg;      /* 分数板对话框过程实例地址 */

/* GDI对象缓存 */
HBRUSH hBrushes[13];   /* 0=空白,1=2,2=4,3=8...12=2048+ */
HPEN   hPenBlack;
HPEN   hPenWhite;
RECT   rcGame;   /* 游戏区域 */

/* K&R 函数前置声明 */
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
    int         i, j;

    switch (message) {

	case WM_CREATE:
	    hWndMain = hWnd;
	    InitGame();
	    InitGDI();
	    rcGame.left   = 0;
	    rcGame.top    = 0;
	    rcGame.right  = 280;
	    rcGame.bottom = 320;
	    break;

	case WM_PAINT:
	    hDC = BeginPaint(hWnd, &ps);
	    GetClientRect(hWnd, &rect);

	    /* 刷白背景 */
	    hBrush = GetStockObject(WHITE_BRUSH);
	    hOldBrush = SelectObject(hDC, hBrush);
	    PatBlt(hDC, rect.left, rect.top,
		   rect.right - rect.left,
		   rect.bottom - rect.top, PATCOPY);
	    SelectObject(hDC, hOldBrush);

	    SetBkMode(hDC, TRANSPARENT);
	    SetTextColor(hDC, RGB(0, 0, 0));

	    /* 标题栏 */
	    sprintf(buf, "2048  Score: %d", score);
	    TextOut(hDC, MARGIN, 4, buf, strlen(buf));

	    if (game_over) {
		TextOut(hDC, MARGIN, 20,
			"GAME OVER - Press N for New Game", 32);
	    } else {
		TextOut(hDC, MARGIN, 20,
			"Arrows: Move   N: New Game", 26);
	    }

	    DrawBoard(hDC);
	    EndPaint(hWnd, &ps);
	    break;

	case WM_KEYDOWN:
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
			remove("2048.sav");
			g_bPlayerMoved = 0;
			InitGame();
					/* 修正：新游戏后，脏标记应为 1 */
		    g_bDirty = 1;
		    InvalidateRect(hWnd, &rcGame, FALSE);
		    break;
	    }
	    break;

MOVED:
	    /* 如果玩家是第一次操作，删除旧的存档文件 */
		if (!g_bPlayerMoved) {
			remove("2048.sav");    /* 旧存档已无效 */
			g_bPlayerMoved = 1;
		}
		AddRandomTile();
			/* 修正：任何移动成功并添加了随机块，都应视为状态改变 */
	    g_bDirty = 1;
		if (!CanMove()){
		game_over = 1;
		/* 游戏结束 → 更新分数历史并弹出分数板 */
		UpdateScoreHistory(score);
	    DialogBox(hInst, MAKEINTRESOURCE(DLG_SCOREBOARD),
	              hWnd, lpfnScoreboardDlg);
	    }
	    InvalidateRect(hWnd, &rcGame, FALSE);
	    break;

	/* 新增：处理菜单命令（Game、Help菜单） */
	case WM_COMMAND:
	    switch (wParam) {
		case IDM_NEWGAME:
			remove("2048.sav");
			g_bPlayerMoved = 0;
			InitGame();
			g_bDirty = 1;   /* 修正：新游戏后，脏标记应为 1 */
		    InvalidateRect(hWnd, &rcGame, FALSE);
		    break;
		case IDM_SAVE:
            SaveGame();
            g_bDirty = 0;
            InvalidateRect(hWnd, &rcGame, FALSE);
            break;
		case IDM_SCOREBOARD:
            /* 玩家随时查看分数板，当前分数来自全局变量 score */
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

	/* 新增：处理系统菜单（左上角 About…） */
	case WM_SYSCOMMAND:
	    if (wParam == IDSYS_ABOUT) {
		DialogBox(hInst, MAKEINTRESOURCE(DLG_ABOUT),
			  hWnd, lpfnAboutDlg);
		return 0;
	    }
	    /* 其他系统命令（最大化、最小化、移动、关闭等）必须交给默认处理 */
	    return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_DESTROY:
	    /* ============================================================
	       修正：退出时检查是否需要存档提醒
	       设计思路：
	       1. 如果游戏已经结束 (game_over == 1)，直接退出，不提醒。
	       2. 如果游戏未结束，但有未保存的改动 (g_bDirty == 1)，则触发提醒。
	       3. 如果游戏未结束且没有改动 (g_bDirty == 0)，说明用户已手动保存过，
		  直接退出，不打扰用户。
	    ============================================================ */
	    if (!game_over && g_bDirty && g_bPlayerMoved) {
		/* 弹出可爱的英文提示框 */
		if (MessageBox(hWnd,
            "Ooh! Meow ^v^ You haven't saved your game!",
            "Save Game?",
            MB_YESNO | MB_ICONQUESTION) == IDYES) {
		    /* 用户选择保存 */
		    SaveGame();
		    g_bDirty = 0;
		}
	    }
			/* 清理GDI对象和实例地址 */
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
    HMENU      hMenu;
    HMENU      hSysMenu;

    srand((unsigned)GetCurrentTime());

    if (!hPrevInstance) {
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = WndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = "2048W103";

	if (!RegisterClass(&wc))
	    return FALSE;
    }

    hInst = hInstance;

    /* 创建对话框过程的实例地址（必须在创建窗口前准备好） */
    lpfnAboutDlg = MakeProcInstance((FARPROC)AboutDlg, hInstance);
    lpfnHowToPlayDlg = MakeProcInstance((FARPROC)HowToPlayDlg, hInstance);
	lpfnScoreboardDlg = MakeProcInstance((FARPROC)ScoreboardDlg, hInstance);

    /* 加载菜单栏（资源名为 "GAME_MENU"，在 2048.rc 中定义） */
    hMenu = LoadMenu(hInstance, (LPSTR)"GAME_MENU");
    if (!hMenu) {
	MessageBox(NULL, "Menu load failed!", NULL, MB_OK);
	return FALSE;
    }

    /* Windows 1.03 兼容样式：WS_TILEDWINDOW（等同于 WS_TILED|WS_CAPTION|WS_SYSMENU|WS_SIZEBOX） */
    /* CreateWindow 只传 10 个参数，无最后的 lpParam */
    hWnd = CreateWindow("2048W103",
			"2048 - Windows 1.03",
			WS_TILEDWINDOW,
			10, 10, 280, 320,
			NULL,
			hMenu,
			hInstance,
			NULL);

    if (!hWnd)
	return FALSE;

    /* ============================================================
       修正：启动时检测并加载存档
       设计思路：
       1. 尝试加载存档。如果成功，游戏状态恢复，存档文件被立即删除。
       2. 如果加载失败（没有存档或文件损坏），则开始一个全新的游戏。
       3. 无论是加载成功还是开始新游戏，都设置脏标记 g_bDirty = 1，
	  因为有新状态产生，应视为未保存。
    ============================================================ */
    if (LoadGame()) {
	/* 成功加载存档，存档文件已被 LoadGame 内部删除 */
	InvalidateRect(hWnd, &rcGame, FALSE);
	g_bDirty = 0;   /* 恢复后的状态，视为未保存的新状态 */
	g_bPlayerMoved = 0;     /* 加载存档不算玩家操作 */
    } else {
	/* 没有存档，开始新游戏 */
	InitGame();
	g_bDirty = 1;   /* 新游戏视为未保存 */
	g_bPlayerMoved = 0;     /* 无存档启动，自动新游戏，视为玩家“开始了一局” */
    }
	
	/* 在系统菜单（左上角）添加分隔线后追加 About… */
    /* 先获取系统菜单副本 */
    hSysMenu = GetSystemMenu(hWnd, FALSE);
    if (hSysMenu) {
	/* 添加分隔线（横杠） */
	ChangeMenu(hSysMenu, 0, (LPSTR)NULL, 0,
		   MF_APPEND | MF_SEPARATOR);
	/* 添加 "About..." 命令 */
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
    board[i][j] = (rand() % 10 == 0) ? 4 : 2;
}

int MoveLeft()
{
    int moved;
    int i, j, k;

    moved = 0;

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
    int moved;
    int i, j;
    int tmp;

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
   GDI绘图
   ============================================================ */

void DrawCell(hDC, value, x, y, w, h)
HDC hDC;
int value, x, y, w, h;
{
    char buf[16];
    int  len;
    int  idx;
    HBRUSH hBrush, hOldBrush;
    HPEN   hPen, hOldPen;

    idx = ValueToIndex(value);
    hBrush = hBrushes[idx];

    if (value >= 8 && value < 128)
	hPen = hPenWhite;
    else
	hPen = hPenBlack;

    hOldBrush = SelectObject(hDC, hBrush);
    hOldPen   = SelectObject(hDC, hPen);

    Rectangle(hDC, x, y, x + w, y + h);

    SelectObject(hDC, hOldBrush);
    SelectObject(hDC, hOldPen);

    if (value != 0) {
	SetBkMode(hDC, TRANSPARENT);
	SetTextColor(hDC, (value >= 8 && value < 128) ?
			  RGB(255,255,255) : RGB(0,0,0));
	sprintf(buf, "%d", value);
	len = strlen(buf);
	TextOut(hDC, x + (w - len * 8) / 2, y + (h - 14) / 2, buf, len);
    }
}

void DrawBoard(hDC)
HDC hDC;
{
    int i, j;
    int x, y;
    int top = 45;

    for (i = 0; i < BOARD_SIZE; i++) {
	for (j = 0; j < BOARD_SIZE; j++) {
	    x = MARGIN + j * (CELL_W + MARGIN);
	    y = top + i * (CELL_H + MARGIN);
	    DrawCell(hDC, board[i][j], x, y, CELL_W, CELL_H);
	}
    }
}

/* 预创建所有GDI对象 */
void InitGDI()
{
    hBrushes[0]  = GetStockObject(LTGRAY_BRUSH);
    hBrushes[1]  = CreateSolidBrush(RGB(238, 228, 218));  /* 2 */
    hBrushes[2]  = CreateSolidBrush(RGB(237, 224, 200));  /* 4 */
    hBrushes[3]  = CreateSolidBrush(RGB(242, 177, 121));  /* 8 */
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

/* 程序退出时统一清理 */
void CleanupGDI()
{
    int i;
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
   对话框过程
   ============================================================ */

/* About 对话框过程 */
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
    if (message == WM_INITDIALOG)
	return TRUE;
    return FALSE;
}

/* HowToPlay 对话框过程（新增） */
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
    if (message == WM_INITDIALOG)
	return TRUE;
    return FALSE;
}

/* ============================================================
   新增：存档文件读写函数
   设计思路：
   1. SaveGame() 将当前棋盘状态写入文本文件 2048.sav。
      文本格式：简单可靠，第一行为分数和游戏状态，随后为棋盘矩阵。
   2. LoadGame() 从 2048.sav 读取状态。
      成功返回 1，失败返回 0。
      成功读取后，应立即删除存档文件，避免下次启动干扰。
   3. 存档仅保存最后一次手动保存的状态。
   4. 游戏启动时会检查并加载存档，加载后文件被删除。
      之后的游戏过程都不会再使用该文件，直到用户再次手动存档。
   ============================================================ */

void SaveGame()
{
    FILE *fp;
    int i, j;

    fp = fopen("2048.sav", "w");
    if (fp == NULL)
	return;

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
    if (fp == NULL)
	return 0;   /* 不存在存档 */

    if (fscanf(fp, "%d %d", &score, &game_over) != 2) {
	fclose(fp);
	return 0;
    }
    for (i = 0; i < BOARD_SIZE; i++)
	for (j = 0; j < BOARD_SIZE; j++) {
	    if (fscanf(fp, "%d", &board[i][j]) != 1) {
		fclose(fp);
		return 0;
	    }
	}
    fclose(fp);

    /* 修正：成功加载后，立即删除存档文件，避免下次启动干扰 */
    /* remove("2048.sav"); */
    return 1;
}

/* ============================================================
   分数历史记录读写函数（二进制文件，彻底避免玩家修改）
   ============================================================ */

/* 从文件读取历史，成功返回 TRUE，失败返回 FALSE */
BOOL GetScoreHistory(pHist)
ScoreHistory *pHist;
{
    FILE *fp;

    fp = fopen("2048.his", "rb");
    if (fp == NULL)
        return FALSE;

    if (fread(pHist, sizeof(ScoreHistory), 1, fp) != 1) {
        fclose(fp);
        return FALSE;
    }
    fclose(fp);
    return TRUE;
}

/* 将历史写入文件（若文件不存在则创建） */
void SetScoreHistory(pHist)
ScoreHistory *pHist;
{
    FILE *fp;

    fp = fopen("2048.his", "wb");
    if (fp == NULL)
        return;

    fwrite(pHist, sizeof(ScoreHistory), 1, fp);
    fclose(fp);
}

/* 游戏结束时调用，传入本局得分，自动更新历史文件 */
void UpdateScoreHistory(newScore)
int newScore;
{
    ScoreHistory hist;
    int i;

    /* 若文件不存在，则初始化空历史 */
    if (!GetScoreHistory(&hist)) {
        hist.highscore = 0;
        hist.count = 0;
        for (i = 0; i < 5; i++)
            hist.recent[i] = 0;
    }

    /* 更新最高分 */
    if (newScore > hist.highscore)
        hist.highscore = newScore;

    /* 将新分数插入 recent[0]，后移原有分数 */
    for (i = 4; i > 0; i--)
        hist.recent[i] = hist.recent[i - 1];
    hist.recent[0] = newScore;
    if (hist.count < 5)
        hist.count++;

    SetScoreHistory(&hist);
}

/* ============================================================
   分数板对话框过程
   ============================================================ */
BOOL FAR PASCAL ScoreboardDlg(hDlg, message, wParam, lParam)
HWND     hDlg;
unsigned message;
WORD     wParam;
LONG     lParam;
{
    ScoreHistory hist;
    char buf[512];
    char line[64];
    int i;

    switch (message) {

    case WM_INITDIALOG:
        /* 读取历史，若失败则显示空数据 */
        if (!GetScoreHistory(&hist)) {
            hist.highscore = 0;
            hist.count = 0;
        }

        /* 构建多行文本（注意换行用 \r\n） */
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

        /* 设置到编辑框（IDC_SCOREBOARD_EDIT） */
        SetDlgItemText(hDlg, IDC_SCOREBOARD_EDIT, buf);
		/* 设置完文本后，将焦点转移到 OK 按钮，避免编辑框文字被选中 */
        SetFocus(GetDlgItem(hDlg, IDOK));
        return FALSE;   /* 返回 FALSE 表示你主动设置了焦点，系统不再自动设置 */

    case WM_COMMAND:
        if (wParam == IDOK) {
            EndDialog(hDlg, TRUE);
            return TRUE;
        }
        break;
    }
    return FALSE;
}