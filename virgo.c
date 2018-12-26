#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#define sb_free(a) ((a) ? HeapFree(GetProcessHeap(), 0, stb__sbraw(a)), 0 : 0)
#define sb_push(a, v) (stb__sbmaybegrow(a, 1), (a)[stb__sbn(a)++] = (v))
#define sb_count(a) ((a) ? stb__sbn(a) : 0)

#define stb__sbraw(a) ((int *)(a)-2)
#define stb__sbm(a) stb__sbraw(a)[0]
#define stb__sbn(a) stb__sbraw(a)[1]

#define stb__sbneedgrow(a, n) ((a) == 0 || stb__sbn(a) + (n) >= stb__sbm(a))
#define stb__sbmaybegrow(a, n) (stb__sbneedgrow(a, (n)) ? stb__sbgrow(a, n) : 0)
#define stb__sbgrow(a, n) ((a) = stb__sbgrowf((a), (n), sizeof(*(a))))

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

#ifndef MOD_WIN
#define MOD_WIN 0x0008
#endif

#define NUM_DESKTOPS 4

typedef struct {
	HWND *windows;
	unsigned count;
} Desktop;

typedef struct {
	NOTIFYICONDATA nid;
	HBITMAP hBitmap;
	HFONT hFont;
	HWND window;
	HDC mdc;
	unsigned bitmapWidth;
} Trayicon;

typedef struct {
	unsigned current;
    unsigned length;
	Desktop desktops[NUM_DESKTOPS];
    HWND foregrounds[NUM_DESKTOPS];
	Trayicon trayicon;
} Virgo;

static void *stb__sbgrowf(void *arr, unsigned increment, unsigned itemsize)
{
	unsigned dbl_cur = arr ? 2 * stb__sbm(arr) : 0;
	unsigned min_needed = sb_count(arr) + increment;
	unsigned m = dbl_cur > min_needed ? dbl_cur : min_needed;
	unsigned *p;
	if (arr) {
		p = HeapReAlloc(GetProcessHeap(), 0, stb__sbraw(arr),
						itemsize * m + sizeof(unsigned) * 2);
	} else {
		p = HeapAlloc(GetProcessHeap(), 0, itemsize * m + sizeof(unsigned) * 2);
	}
	if (p) {
		if (!arr) {
			p[1] = 0;
		}
		p[0] = m;
		return p + 2;
	} else {
		ExitProcess(1);
		return (void *)(2 * sizeof(unsigned));
	}
}

static HICON trayicon_draw(Trayicon *trayicon, char *text, unsigned len)
{
	ICONINFO iconInfo;
	HBITMAP hOldBitmap;
	HFONT hOldFont;
	hOldBitmap = (HBITMAP)SelectObject(trayicon->mdc, trayicon->hBitmap);
	hOldFont = (HFONT)SelectObject(trayicon->mdc, trayicon->hFont);
	TextOut(trayicon->mdc, trayicon->bitmapWidth / 4, 0, text, len);
	SelectObject(trayicon->mdc, hOldBitmap);
	SelectObject(trayicon->mdc, hOldFont);
	iconInfo.fIcon = TRUE;
	iconInfo.xHotspot = iconInfo.yHotspot = 0;
	iconInfo.hbmMask = iconInfo.hbmColor = trayicon->hBitmap;
	return CreateIconIndirect(&iconInfo);
}

static void trayicon_init(Trayicon *trayicon)
{
	HDC hdc;
	trayicon->window =
		CreateWindowA("STATIC", "virgo", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
	trayicon->bitmapWidth = GetSystemMetrics(SM_CXSMICON);
	trayicon->nid.cbSize = sizeof(trayicon->nid);
	trayicon->nid.hWnd = trayicon->window;
	trayicon->nid.uID = 100;
	trayicon->nid.uFlags = NIF_ICON;
	hdc = GetDC(trayicon->window);
	trayicon->hBitmap = CreateCompatibleBitmap(hdc, trayicon->bitmapWidth, trayicon->bitmapWidth);
	trayicon->mdc = CreateCompatibleDC(hdc);
	ReleaseDC(trayicon->window, hdc);
	SetBkColor(trayicon->mdc, RGB(0x00, 0x00, 0x00));
	SetTextColor(trayicon->mdc, RGB(0x00, 0xFF, 0x00));
	trayicon->hFont = CreateFont(-MulDiv(11, GetDeviceCaps(trayicon->mdc, LOGPIXELSY), 72), 0,
						  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, TEXT("Arial"));
	trayicon->nid.hIcon = trayicon_draw(trayicon, "1", 1);
	Shell_NotifyIcon(NIM_ADD, &trayicon->nid);
}

static void trayicon_set(Trayicon *trayicon, unsigned number)
{
	char snumber[2];
	if (number > 9) {
		return;
	}
	snumber[0] = number + '0';
	snumber[1] = 0;
	DestroyIcon(trayicon->nid.hIcon);
	trayicon->nid.hIcon = trayicon_draw(trayicon, snumber, 1);
	Shell_NotifyIcon(NIM_MODIFY, &trayicon->nid);
}

static void trayicon_deinit(Trayicon *trayicon)
{
	Shell_NotifyIcon(NIM_DELETE, &trayicon->nid);
	DestroyIcon(trayicon->nid.hIcon);
	DeleteObject(trayicon->hBitmap);
	DeleteObject(trayicon->hFont);
	DeleteDC(trayicon->mdc);
	DestroyWindow(trayicon->window);
}

static void desktop_mod(Desktop *desktops, unsigned state)
{
	unsigned i;
	for (i = 0; i < desktops->count; i++) {
		ShowWindow(desktops->windows[i], state);
	}
}

static void desktop_show(Desktop *desktops) { desktop_mod(desktops, SW_SHOW); }

static void desktop_hide(Desktop *desktops) { desktop_mod(desktops, SW_HIDE); }

static void desktop_add(Desktop *desktops, HWND window)
{
	if (desktops->count >= sb_count(desktops->windows)) {
		sb_push(desktops->windows, window);
	} else {
		desktops->windows[desktops->count] = window;
	}
	desktops->count++;
}

static void desktop_del(Desktop *desktops, HWND window)
{
	unsigned i, e;
	for (i = 0; i < desktops->count; i++) {
		if (desktops->windows[i] != window) {
			continue;
		}
		if (i != desktops->count - 1) {
			for (e = i; e < desktops->count - 1; e++) {
				desktops->windows[e] = desktops->windows[e + 1];
			}
		}
		desktops->count--;
		break;
	}
}

static unsigned is_valid_window(HWND window)
{
	WINDOWINFO wi;
	wi.cbSize = sizeof(wi);
	GetWindowInfo(window, &wi);
	return (wi.dwStyle & WS_VISIBLE) && !(wi.dwExStyle & WS_EX_TOOLWINDOW);
}

static void register_hotkey(unsigned id, unsigned mod, unsigned vk)
{
	if (!RegisterHotKey(NULL, id, mod, vk)) {
		MessageBox(NULL, "could not register hotkey", "error",
				   MB_ICONEXCLAMATION);
		ExitProcess(1);
	}
}

static BOOL enum_func(HWND window, LPARAM lParam)
{
	unsigned i, e;
	Virgo *virgo;
	Desktop *desktop;
	virgo = (Virgo *)lParam;
	if (!is_valid_window(window)) {
		return 1;
	}
	for (i = 0; i < NUM_DESKTOPS; i++) {
		desktop = &(virgo->desktops[i]);
		for (e = 0; e < desktop->count; e++) {
			if (desktop->windows[e] == window) {
				return 1;
			}
		}
	}
	desktop_add(&(virgo->desktops[virgo->current]), window);
	return 1;
}

static void virgo_update(Virgo *virgo)
{
	unsigned i, e;
	Desktop *desktop;
	HWND window;
	for (i = 0; i < NUM_DESKTOPS; i++) {
		desktop = &(virgo->desktops[i]);
		for (e = 0; e < desktop->count; e++) {
			window = desktop->windows[e];
			if (!GetWindowThreadProcessId(desktop->windows[e], NULL)) {
				desktop_del(desktop, window);
			}
		}
	}
	desktop = &virgo->desktops[virgo->current];
	for (i = 0; i < desktop->count; i++) {
		window = desktop->windows[i];
		if (!IsWindowVisible(window)) {
			desktop_del(desktop, window);
		}
	}
	EnumWindows((WNDENUMPROC)&enum_func, (LPARAM)virgo);
}

static void virgo_init(Virgo *virgo)
{
    virgo->length = 2;
	register_hotkey(0, MOD_CONTROL | MOD_WIN | MOD_NOREPEAT, 'D');
	register_hotkey(1, MOD_CONTROL | MOD_WIN | MOD_NOREPEAT, VK_F4);
	register_hotkey(2, MOD_CONTROL | MOD_WIN | MOD_NOREPEAT, VK_LEFT);
	register_hotkey(3, MOD_CONTROL | MOD_WIN | MOD_NOREPEAT, VK_RIGHT);
	trayicon_init(&virgo->trayicon);
}

static void virgo_deinit(Virgo *virgo)
{
	unsigned i;
	for (i = 0; i < NUM_DESKTOPS; i++) {
		desktop_show(&virgo->desktops[i]);
		sb_free(virgo->desktops[i].windows);
	}
	trayicon_deinit(&virgo->trayicon);
}

static void virgo_go_to_desktop(Virgo *virgo, int target)
{
	if (virgo->current == target || target < 0 || target == virgo->length) {
		return;
	}

	virgo_update(virgo);
    virgo->foregrounds[virgo->current] = GetForegroundWindow();
	desktop_hide(&virgo->desktops[virgo->current]);
	desktop_show(&virgo->desktops[target]);
    SetForegroundWindow(virgo->foregrounds[target]);
	virgo->current = target;
	trayicon_set(&virgo->trayicon, virgo->current + 1);
}

static void virgo_move_window_to_desktop(Virgo *virgo, HWND window, int target)
{
    if (virgo->current == target || target < 0 || target == virgo->length) {
        return;
    }
    if (!window || !is_valid_window(window)) {
        return;
    }
    virgo_update(virgo);
    desktop_del(&virgo->desktops[virgo->current], window);
    desktop_add(&virgo->desktops[target], window);
    ShowWindow(window, SW_HIDE);
}

static void virgo_stretch(Virgo *virgo)
{
    if (virgo->length == NUM_DESKTOPS) {
        return;
    }

    ++virgo->length;
    virgo_go_to_desktop(virgo, virgo->length - 1);
}

static void virgo_shrink(Virgo *virgo)
{
    unsigned i;
    Desktop desktop;
    HWND window;
    if (virgo->length == 1 || virgo->current == 0) {
        return;
    }

    virgo_update(virgo);
    window = GetForegroundWindow();
    if (window) {
        virgo->foregrounds[virgo->current - 1] = window;
    }

    desktop = virgo->desktops[virgo->current];
    for (i = 0; i < desktop.count; ++i) {
        virgo_move_window_to_desktop(virgo, desktop.windows[i], virgo->current - 1);
    }
    virgo_go_to_desktop(virgo, virgo->current - 1);
    desktop = virgo->desktops[virgo->current + 1];
    for (i = virgo->current + 2; i < virgo->length; ++i) {
        virgo->desktops[i - 1] = virgo->desktops[i];
    }
    virgo->desktops[virgo->length - 1] = desktop;
    --virgo->length;
}

void __main(void) __asm__("__main");
void __main(void)
{
	Virgo virgo = {0};
	MSG msg;
	virgo_init(&virgo);
	while (GetMessage(&msg, NULL, 0, 0)) {
		if (msg.message != WM_HOTKEY) {
			continue;
		}
		switch (msg.wParam) {
			case 0:
                virgo_stretch(&virgo);
				break;
			case 1:
                virgo_shrink(&virgo);
				break;
			case 2:
				/* left */
				virgo_go_to_desktop(&virgo, virgo.current - 1);
				break;
			case 3:
				/* right */
				virgo_go_to_desktop(&virgo, virgo.current + 1);
				break;
		}
	}
	virgo_deinit(&virgo);
	ExitProcess(0);
}
