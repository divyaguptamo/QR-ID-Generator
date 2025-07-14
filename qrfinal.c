#include <windows.h>
#include <gdiplus.h>
#include <stdio.h>
#include <string.h>
#include "qrcodegen.h"
#include <commdlg.h> // For GetSaveFileNameA
#pragma comment(lib, "gdiplus.lib")

#define PADDING 20
#define MAX_QR_SIZE 177

bool qr[MAX_QR_SIZE][MAX_QR_SIZE]; // Supports up to version 40
int qrSize = 0;

#define IDC_NAME    101
#define IDC_PHONE   102
#define IDC_EMAIL   103
#define IDC_GEN     104
#define IDC_SAVE    105

static void init_matrix(void) {
    for (int y = 0; y < MAX_QR_SIZE; y++)
        for (int x = 0; x < MAX_QR_SIZE; x++)
            qr[y][x] = 0;
}

static void generate_qr(const char *text) {
    uint8_t qrcode[qrcodegen_BUFFER_LEN_FOR_VERSION(qrcodegen_VERSION_MAX)];
    uint8_t tempBuffer[qrcodegen_BUFFER_LEN_FOR_VERSION(qrcodegen_VERSION_MAX)];

    bool ok = qrcodegen_encodeText(
        text, tempBuffer, qrcode,
        qrcodegen_Ecc_LOW,
        qrcodegen_VERSION_MIN,
        qrcodegen_VERSION_MAX,
        qrcodegen_Mask_AUTO,
        true
    );

    if (!ok) {
        MessageBoxA(NULL, "QR generation failed!", "Error", MB_ICONERROR);
        return;
    }

    qrSize = qrcodegen_getSize(qrcode);
    init_matrix();

    for (int y = 0; y < qrSize; y++) {
        for (int x = 0; x < qrSize; x++) {
            qr[y][x] = qrcodegen_getModule(qrcode, x, y);
        }
    }
}

static int get_encoder_clsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0, size = 0;
    GdipGetImageEncodersSize(&num, &size);
    if (size == 0) return -1;

    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)malloc(size);
    if (!pImageCodecInfo) return -1;

    GdipGetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return 0;
        }
    }
    free(pImageCodecInfo);
    return -1;
}

static void save_qr_image(HWND hwnd) {
    char filename[MAX_PATH] = "qr_output.png";

    OPENFILENAMEA ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "PNG Files (.png)\0.png\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = "png";

    if (!GetSaveFileNameA(&ofn)) return;

    GdiplusStartupInput gdiStartupInput;
    ULONG_PTR gdiToken;

    gdiStartupInput.GdiplusVersion = 1;
    gdiStartupInput.DebugEventCallback = NULL;
    gdiStartupInput.SuppressBackgroundThread = FALSE;
    gdiStartupInput.SuppressExternalCodecs = FALSE;

    if (GdiplusStartup(&gdiToken, &gdiStartupInput, NULL) != Ok) {
        MessageBoxA(hwnd, "Failed to initialize GDI+.", "Error", MB_ICONERROR);
        return;
    }

    int cellSize = 20;
    int margin = 4;
    int fullSize = qrSize + 2 * margin;
    int imgSize = fullSize * cellSize;

    HDC hdc = GetDC(hwnd);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP hBmp = CreateCompatibleBitmap(hdc, imgSize, imgSize);
    SelectObject(memDC, hBmp);

    HBRUSH white = (HBRUSH)GetStockObject(WHITE_BRUSH);
    HBRUSH black = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RECT fullRect = { 0, 0, imgSize, imgSize };
    FillRect(memDC, &fullRect, white);

    for (int y = 0; y < qrSize; y++) {
        for (int x = 0; x < qrSize; x++) {
            if (qr[y][x]) {
                RECT r = {
                    (x + margin) * cellSize,
                    (y + margin) * cellSize,
                    (x + margin + 1) * cellSize,
                    (y + margin + 1) * cellSize
                };
                FillRect(memDC, &r, black);
            }
        }
    }

    GpBitmap* bitmap = NULL;
    GdipCreateBitmapFromHBITMAP(hBmp, NULL, &bitmap);

    const WCHAR* mimeType = L"image/png";
    CLSID encoderClsid;
    if (get_encoder_clsid(mimeType, &encoderClsid) == 0) {
        WCHAR widePath[MAX_PATH];
        MultiByteToWideChar(CP_ACP, 0, filename, -1, widePath, MAX_PATH);
        GdipSaveImageToFile(bitmap, widePath, &encoderClsid, NULL);
        MessageBoxA(hwnd, "QR code saved as PNG successfully.", "Saved", MB_OK);
    } else {
        MessageBoxA(hwnd, "Failed to get PNG encoder.", "Error", MB_ICONERROR);
    }

    GdipDisposeImage(bitmap);
    DeleteObject(hBmp);
    DeleteDC(memDC);
    ReleaseDC(hwnd, hdc);
    GdiplusShutdown(gdiToken);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HWND hName, hPhone, hEmail, hGenBtn, hSaveBtn;
    switch (msg) {
    case WM_CREATE:
        CreateWindowA("STATIC", "Name:", WS_CHILD | WS_VISIBLE,
            10, 10, 100, 15, hwnd, NULL, NULL, NULL);
        hName = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
            10, 30, 370, 20, hwnd, (HMENU)IDC_NAME, NULL, NULL);

        CreateWindowA("STATIC", "Phone:", WS_CHILD | WS_VISIBLE,
            10, 60, 100, 15, hwnd, NULL, NULL, NULL);
        hPhone = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
            10, 80, 370, 20, hwnd, (HMENU)IDC_PHONE, NULL, NULL);

        CreateWindowA("STATIC", "Email:", WS_CHILD | WS_VISIBLE,
            10, 110, 100, 15, hwnd, NULL, NULL, NULL);
        hEmail = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
            10, 130, 370, 20, hwnd, (HMENU)IDC_EMAIL, NULL, NULL);

        hGenBtn = CreateWindowA("BUTTON", "Generate", WS_CHILD | WS_VISIBLE,
            10, 165, 100, 25, hwnd, (HMENU)IDC_GEN, NULL, NULL);

        hSaveBtn = CreateWindowA("BUTTON", "Save", WS_CHILD | WS_VISIBLE,
            120, 165, 100, 25, hwnd, (HMENU)IDC_SAVE, NULL, NULL);
        break;

    case WM_COMMAND:
        if (LOWORD(wp) == IDC_GEN) {
            char name[100], phone[50], email[100], combined[300];
            GetWindowTextA(hName, name, sizeof(name));
            GetWindowTextA(hPhone, phone, sizeof(phone));
            GetWindowTextA(hEmail, email, sizeof(email));

            snprintf(combined, sizeof(combined),
                "Name: %s\nPhone: %s\nEmail: %s", name, phone, email);

            generate_qr(combined);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        else if (LOWORD(wp) == IDC_SAVE) {
            save_qr_image(hwnd);
        }
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT client;
        GetClientRect(hwnd, &client);
        FillRect(dc, &client, (HBRUSH)(COLOR_WINDOW + 1));

        int availW = client.right;
        int availH = client.bottom - 200;
        int qrArea = (availW < availH ? availW : availH) - 2 * PADDING;
        int cell = qrSize > 0 ? qrArea / qrSize : 1;
        int offsetX = (availW - cell * qrSize) / 2;
        int offsetY = 200 + PADDING;

        for (int y = 0; y < qrSize; y++) {
            for (int x = 0; x < qrSize; x++) {
                RECT r = {
                    offsetX + x * cell, offsetY + y * cell,
                    offsetX + (x + 1) * cell, offsetY + (y + 1) * cell
                };
                FillRect(dc, &r,
                    (HBRUSH)(qr[y][x] ? GetStockObject(BLACK_BRUSH) : GetStockObject(WHITE_BRUSH)));
            }
        }
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, PSTR cmd, int nShow) {
    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "QRGen";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowA(
        "QRGen", "QR Code Generator",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        420, 600, NULL, NULL, hInst, NULL);

    if (!hwnd) {
        MessageBoxA(NULL, "Window creation failed", "Error", MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}