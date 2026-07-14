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
HBRUSH hBrush, hOldBrush;
/*HMENU  hSysMenu;
char   aboutBuf[128];*/
HWND hWndMain;

/* GDI对象缓存：程序启动时造好，退出时销毁，中间绝不碰Create/Delete */
HBRUSH hBrushes[13];   /* 0=空白,1=2,2=4,3=8...12=2048+ */
HPEN   hPenBlack;
HPEN   hPenWhite;
RECT   rcGame;   /* 游戏区域：标题栏 + 格子，局部刷新用 */

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
void InitGDI();
void CleanupGDI();
int  ValueToIndex();

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
			InitGDI();    /* 新增：启动时造好所有画刷 */
			
			/* 给系统菜单加 About 项 */
            /*hSysMenu = GetSystemMenu(hWnd, FALSE);*/
            /* Windows 1.03 用 ChangeMenu 追加菜单项 */
            /* 参数：菜单句柄, 旧项ID(忽略), 新项文字, 新项ID, 标志 */
            /*ChangeMenu(hSysMenu, 0, "About 2048...", 100, 0x0100);*/
            /* 0x0100 = MF_APPEND, 0x0000 = MF_STRING */
			
			/* 定义需要重绘的区域：整个游戏画面（标题+格子） */
            rcGame.left   = 0;
            rcGame.top    = 0;
            rcGame.right  = 280;
            rcGame.bottom = 320;
            break;

        case WM_PAINT:
            hDC = BeginPaint(hWnd, &ps);
            GetClientRect(hWnd, &rect);

            /* 先刷白背景 */
            /* 罪状1修复：先选入白色画刷，再PatBlt */
            hBrush = GetStockObject(WHITE_BRUSH);
            hOldBrush = SelectObject(hDC, hBrush);
            PatBlt(hDC, rect.left, rect.top,
                   rect.right - rect.left,
                   rect.bottom - rect.top,
                   PATCOPY);
			SelectObject(hDC, hOldBrush);  /* 用完还回去 */
			
			/* 罪状2修复：设成透明模式，TextOut不再画黑矩形 */
            SetBkMode(hDC, TRANSPARENT);
            SetTextColor(hDC, RGB(0, 0, 0));  /* 纯黑文字，对比度拉满 */
            
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
                    InvalidateRect(hWnd, &rcGame, FALSE);  /* 同样改这里 */
                    break;
            }
            break;

MOVED:
            /* 有移动才生成新块，然后检查死活 */
            AddRandomTile();
            if (!CanMove())
                game_over = 1;
            InvalidateRect(hWnd, &rcGame, FALSE);  /* FALSE = 系统不擦背景，你自己画 */
            break;

        #if 0
		case WM_SYSCOMMAND:
            if (wParam == 100) {
                sprintf(aboutBuf, "2048 for Windows 1.03  Version bate1.0.2  Developer: yuanyuan xia (sha xin)");
                MessageBox(hWnd, aboutBuf, "About 2048", MB_OK);
                return 0L; /* 拦截，不让系统继续处理 */
            }
			/* 其他所有系统命令（Move/Size/Minimize/Maximize/Close） */
            /* 必须交给系统的默认管家 DefWindowProc */
            return DefWindowProc(hWnd, message, wParam, lParam);
            break;
		#endif
		
		case WM_DESTROY:
			CleanupGDI();  /* 新增：退出时统一销毁 */
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
	int  idx;
    HBRUSH hBrush, hOldBrush;
    HPEN   hPen, hOldPen;

    /* 查表取画刷，O(1) */
    idx = ValueToIndex(value);
    hBrush = hBrushes[idx];
	
	/* 选画笔：深色格用白笔，浅色格用黑笔 */
    if (value >= 8 && value < 128)
        hPen = hPenWhite;
    else
        hPen = hPenBlack;

    hOldBrush = SelectObject(hDC, hBrush);
    hOldPen   = SelectObject(hDC, hPen);

    /* Rectangle用当前画刷填充，用当前画笔描边 */
    Rectangle(hDC, x, y, x + w, y + h);

    SelectObject(hDC, hOldBrush);
    SelectObject(hDC, hOldPen);

    /* 画数字 */
    if (value != 0) {
		SetBkMode(hDC, TRANSPARENT);      /* 加这行！格子里的字也透明 */
		SetTextColor(hDC, (value >= 8 && value < 128) ? RGB(255,255,255) : RGB(0,0,0));
        sprintf(buf, "%d", value);
        len = strlen(buf);
        /* 简单居中：EGA字符约8x14 */
        TextOut(hDC, x + (w - len * 8) / 2, y + (h - 14) / 2, buf, len);
    }
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

/* ------------------------------------------------------------
   预创建所有GDI对象
   ------------------------------------------------------------ */
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
    hBrushes[10] = CreateSolidBrush(RGB(237, 207, 114)); /* 1024 */
    hBrushes[11] = CreateSolidBrush(RGB(237, 207, 114)); /* 2048 */
    hBrushes[12] = CreateSolidBrush(RGB(237, 207, 114)); /* 更大 */

    hPenBlack = GetStockObject(BLACK_PEN);
    hPenWhite = GetStockObject(WHITE_PEN);
}

/* ------------------------------------------------------------
   程序退出时统一清理
   ------------------------------------------------------------ */
void CleanupGDI()
{
    int i;
    /* 0号是Stock Object，系统会回收，不能删！从1开始 */
    for (i = 1; i < 13; i++) {
        if (hBrushes[i])
            DeleteObject(hBrushes[i]);
    }
    /* 画笔也是Stock Object，不用删 */
}

/* ------------------------------------------------------------
   数字值转数组索引
   ------------------------------------------------------------ */
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
