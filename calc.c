#include <windows.h>
#include <string.h>
#include <stdio.h>

// Control IDs used by Win32 to identify UI elements in WM_COMMAND/WM_DRAWITEM.
#define ID_DISPLAY 100
#define ID_BUTTON_BASE 1000

// Display geometry.
#define DISPLAY_X 14
#define DISPLAY_Y 18
#define DISPLAY_WIDTH 335
#define DISPLAY_HEIGHT 70

// Button grid geometry.
#define BUTTON_COLS 4
#define BUTTON_X 14
#define BUTTON_WIDTH 78
#define BUTTON_HEIGHT 58
#define BUTTON_GAP 8

// Memory button row geometry (sits above the main button grid).
#define MEM_BUTTON_COUNT 5
#define MEM_BUTTON_Y 108
#define MEM_BUTTON_HEIGHT 36
#define BUTTON_Y (MEM_BUTTON_Y + MEM_BUTTON_HEIGHT + BUTTON_GAP)

// Base control ID for memory buttons.
#define ID_MEMORY_BUTTON_BASE 2000

// Extra empty space at right/bottom edge of the client area.
#define CONTENT_RIGHT_PADDING 14
#define CONTENT_BOTTOM_PADDING 14

// Handles to controls and GDI resources used while painting.
HWND hDisplay;
HFONT hDisplayFont;
HFONT hButtonFont;
HBRUSH hDisplayBrush;

// Shared colors for the display area.
COLORREF displayBackColor = RGB(252, 254, 255);
COLORREF displayBorderColor = RGB(141, 166, 198);

// Calculator state.
double currentValue = 0, previousValue = 0;
double repeatValue = 0;
char operation = 0;
char repeatOperation = 0;
BOOL newNumber = TRUE;
BOOL hasRepeatOperation = FALSE;

// Memory state.
double memoryValue = 0;

// Button labels in visual order (left-to-right, top-to-bottom).
const char* buttons[] = {
    "C", "CE", "<-", "/",
    "7", "8", "9", "*",
    "4", "5", "6", "-",
    "1", "2", "3", "+",
    "0", ".", "="
};

const int buttonCount = sizeof(buttons) / sizeof(buttons[0]);

// Memory button labels.
const char* memoryButtons[] = {"MC", "MR", "M+", "M-", "MS"};

// Color palette used to paint one owner-drawn button.
typedef struct ButtonPalette {
    COLORREF border;
    COLORREF top;
    COLORREF bottom;
    COLORREF highlight;
    COLORREF text;
} ButtonPalette;

// Draw a simple top-to-bottom gradient by painting one horizontal line at a time.
void FillVerticalGradient(HDC hdc, RECT rect, COLORREF topColor, COLORREF bottomColor) {
    int height = rect.bottom - rect.top;

    if (height <= 0) {
        return;
    }

    for (int y = 0; y < height; y++) {
        int red = GetRValue(topColor) + (GetRValue(bottomColor) - GetRValue(topColor)) * y / height;
        int green = GetGValue(topColor) + (GetGValue(bottomColor) - GetGValue(topColor)) * y / height;
        int blue = GetBValue(topColor) + (GetBValue(bottomColor) - GetBValue(topColor)) * y / height;
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(red, green, blue));
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);

        MoveToEx(hdc, rect.left, rect.top + y, NULL);
        LineTo(hdc, rect.right, rect.top + y);

        SelectObject(hdc, oldPen);
        DeleteObject(pen);
    }
}

// Helper that shrinks a rectangle inward by dx/dy on all sides.
void InflateRectBy(RECT* rect, int dx, int dy) {
    rect->left += dx;
    rect->top += dy;
    rect->right -= dx;
    rect->bottom -= dy;
}

// Paint the overall calculator background with layered Vista-like blue gradients.
void PaintWindowBackground(HDC hdc, const RECT* clientRect) {
    RECT topGlow = *clientRect;
    RECT body = *clientRect;
    RECT bottomBand = *clientRect;

    topGlow.bottom = 126;
    body.top = topGlow.bottom;
    bottomBand.top = clientRect->bottom - 128;

    FillVerticalGradient(hdc, topGlow, RGB(235, 244, 253), RGB(197, 218, 241));
    FillVerticalGradient(hdc, body, RGB(214, 227, 242), RGB(171, 191, 214));
    FillVerticalGradient(hdc, bottomBand, RGB(160, 181, 206), RGB(138, 160, 187));

    HPEN linePen = CreatePen(PS_SOLID, 1, RGB(243, 248, 253));
    HPEN oldPen = (HPEN)SelectObject(hdc, linePen);
    MoveToEx(hdc, 0, 0, NULL);
    LineTo(hdc, clientRect->right, 0);
    SelectObject(hdc, oldPen);
    DeleteObject(linePen);
}

// Pick a button color theme based on function (equals, clear, operators, digits).
ButtonPalette GetButtonPalette(const char* text) {
    ButtonPalette palette;

    if (strcmp(text, "=") == 0) {
        palette.border = RGB(24, 73, 135);
        palette.top = RGB(76, 154, 235);
        palette.bottom = RGB(30, 106, 190);
        palette.highlight = RGB(10, 82, 255);
        palette.text = RGB(25, 25, 25);
        return palette;
    }

    if (strcmp(text, "MC") == 0 || strcmp(text, "MR") == 0 ||
        strcmp(text, "M+") == 0 || strcmp(text, "M-") == 0 || strcmp(text, "MS") == 0) {
        palette.border = RGB(140, 150, 170);
        palette.top = RGB(218, 224, 236);
        palette.bottom = RGB(188, 197, 213);
        palette.highlight = RGB(238, 242, 250);
        palette.text = RGB(28, 38, 60);
        return palette;
    }

    if (strcmp(text, "C") == 0 || strcmp(text, "CE") == 0 || strcmp(text, "<-") == 0) {
        palette.border = RGB(156, 92, 86);
        palette.top = RGB(250, 226, 221);
        palette.bottom = RGB(220, 169, 160);
        palette.highlight = RGB(255, 245, 241);
        palette.text = RGB(88, 44, 35);
        return palette;
    }

    if (strcmp(text, "/") == 0 || strcmp(text, "*") == 0 || strcmp(text, "-") == 0 || strcmp(text, "+") == 0) {
        palette.border = RGB(118, 146, 177);
        palette.top = RGB(231, 241, 252);
        palette.bottom = RGB(184, 206, 231);
        palette.highlight = RGB(248, 252, 255);
        palette.text = RGB(34, 63, 95);
        return palette;
    }

    palette.border = RGB(132, 153, 179);
    palette.top = RGB(252, 253, 255);
    palette.bottom = RGB(210, 223, 239);
    palette.highlight = RGB(255, 255, 255);
    palette.text = RGB(36, 49, 64);
    return palette;
}

// Resolve a button label from its control ID so owner-draw painting does not depend on window text retrieval.
const char* GetButtonTextById(UINT controlId) {
    if (controlId >= ID_BUTTON_BASE && controlId < ID_BUTTON_BASE + buttonCount) {
        return buttons[controlId - ID_BUTTON_BASE];
    }

    if (controlId >= ID_MEMORY_BUTTON_BASE && controlId < ID_MEMORY_BUTTON_BASE + MEM_BUTTON_COUNT) {
        return memoryButtons[controlId - ID_MEMORY_BUTTON_BASE];
    }

    return "";
}

// Paint one owner-drawn button using gradients, rounded border, and centered text.
void DrawVistaButton(const DRAWITEMSTRUCT* drawItem) {
    RECT outerRect = drawItem->rcItem;
    RECT innerRect = outerRect;
    RECT highlightRect = outerRect;
    RECT textRect = outerRect;
    const char* text = GetButtonTextById(drawItem->CtlID);
    ButtonPalette palette = GetButtonPalette(text);
    BOOL pressed = (drawItem->itemState & ODS_SELECTED) != 0;
    BOOL focused = (drawItem->itemState & ODS_FOCUS) != 0;
    HDC hdc = drawItem->hDC;
    BOOL isEquals = strcmp(text, "=") == 0;

    // Pressed state flips gradient direction and nudges text down by 1px.
    if (pressed) {
        COLORREF tempTop = palette.top;
        palette.top = palette.bottom;
        palette.bottom = tempTop;
    }

    FillVerticalGradient(hdc, outerRect, palette.top, palette.bottom);

    InflateRectBy(&innerRect, 1, 1);
    if (isEquals) {
        FillVerticalGradient(hdc, innerRect, RGB(65, 145, 232), RGB(18, 92, 182));
    } else {
        FillVerticalGradient(hdc, innerRect, palette.highlight, palette.top);

        highlightRect.bottom = highlightRect.top + (highlightRect.bottom - highlightRect.top) / 2;
        InflateRectBy(&highlightRect, 2, 2);
        FillVerticalGradient(hdc, highlightRect, RGB(255, 255, 255), palette.highlight);
    }

    RoundRect(hdc, outerRect.left, outerRect.top, outerRect.right, outerRect.bottom, 10, 10);

    HPEN borderPen = CreatePen(PS_SOLID, 1, palette.border);
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
    HBRUSH nullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, nullBrush);
    RoundRect(hdc, outerRect.left, outerRect.top, outerRect.right, outerRect.bottom, 10, 10);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, palette.text);
    SelectObject(hdc, hButtonFont);

    if (pressed) {
        OffsetRect(&textRect, 0, 1);
    }

    DrawTextA(hdc, text, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Draw keyboard focus rectangle when the button has focus.
    if (focused) {
        RECT focusRect = outerRect;
        InflateRectBy(&focusRect, 4, 4);
        DrawFocusRect(hdc, &focusRect);
    }
}

// Paint the rounded frame that sits behind the read-only display control.
void PaintDisplayFrame(HDC hdc) {
    RECT frameRect = {DISPLAY_X - 2, DISPLAY_Y - 2, DISPLAY_X + DISPLAY_WIDTH + 2, DISPLAY_Y + DISPLAY_HEIGHT + 2};
    RECT glowRect = frameRect;
    RECT innerRect = frameRect;
    HPEN borderPen;
    HPEN oldPen;
    HBRUSH oldBrush;

    glowRect.bottom = glowRect.top + 18;
    FillVerticalGradient(hdc, frameRect, RGB(251, 254, 255), RGB(222, 233, 246));
    FillVerticalGradient(hdc, glowRect, RGB(255, 255, 255), RGB(244, 249, 255));

    borderPen = CreatePen(PS_SOLID, 1, displayBorderColor);
    oldPen = (HPEN)SelectObject(hdc, borderPen);
    oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, frameRect.left, frameRect.top, frameRect.right, frameRect.bottom, 12, 12);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);

    InflateRectBy(&innerRect, 2, 2);
    borderPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    oldPen = (HPEN)SelectObject(hdc, borderPen);
    oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, innerRect.left, innerRect.top, innerRect.right, innerRect.bottom, 10, 10);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);
}

// Convert a numeric value to text and show it in the display control.
void UpdateDisplay(double value) {
    char buffer[32];
    sprintf_s(buffer, sizeof(buffer), "%.10g", value);
    SetWindowTextA(hDisplay, buffer);
}

// Full reset: clear operands, operators, repeat-equals state, and display.
void ResetAll() {
    currentValue = 0;
    previousValue = 0;
    repeatValue = 0;
    operation = 0;
    repeatOperation = 0;
    hasRepeatOperation = FALSE;
    newNumber = TRUE;
    UpdateDisplay(0);
}

// Clear only the current entry, keeping any pending operation intact.
void ClearEntry() {
    currentValue = 0;
    newNumber = TRUE;
    UpdateDisplay(0);
}

// Remove one digit from the current entry; if empty, fall back to 0.
void BackspaceEntry() {
    char buffer[32];
    size_t length;

    // If we are waiting for a brand-new number, backspace should do nothing.
    if (newNumber) {
        return;
    }

    GetWindowTextA(hDisplay, buffer, sizeof(buffer));
    length = strlen(buffer);

    // If only one digit remains (or just a negative sign + digit), clear entry.
    if (length <= 1 || (length == 2 && buffer[0] == '-')) {
        ClearEntry();
        return;
    }

    buffer[length - 1] = '\0';
    SetWindowTextA(hDisplay, buffer);
    currentValue = atof(buffer);
}

// Execute pending binary operation: previousValue <op> currentValue.
void Calculate() {
    double rightOperand;

    // No queued operator means there is nothing to evaluate.
    if (operation == 0) return;
    rightOperand = currentValue;
    
    switch (operation) {
        case '+': currentValue = previousValue + currentValue; break;
        case '-': currentValue = previousValue - currentValue; break;
        case '*': currentValue = previousValue * currentValue; break;
        case '/': if (currentValue != 0) currentValue = previousValue / currentValue; break;
    }

    // Save last operation and right operand so repeated '=' can replay it.
    repeatOperation = operation;
    repeatValue = rightOperand;
    hasRepeatOperation = TRUE;
    operation = 0;
    UpdateDisplay(currentValue);
    newNumber = TRUE;
}

// Handle clicks on the memory row buttons.
void OnMemoryButtonClick(int id) {
    int index = id - ID_MEMORY_BUTTON_BASE;
    const char* btnText = memoryButtons[index];

    if (strcmp(btnText, "MC") == 0) {
        memoryValue = 0;
    } else if (strcmp(btnText, "MR") == 0) {
        currentValue = memoryValue;
        UpdateDisplay(currentValue);
        newNumber = TRUE;
    } else if (strcmp(btnText, "M+") == 0) {
        memoryValue += currentValue;
    } else if (strcmp(btnText, "M-") == 0) {
        memoryValue -= currentValue;
    } else if (strcmp(btnText, "MS") == 0) {
        memoryValue = currentValue;
    }
}

// Central click handler for all calculator buttons.
void OnButtonClick(int id) {
    int buttonIndex = id - ID_BUTTON_BASE;
    const char* btnText = buttons[buttonIndex];

    // Full clear.
    if (strcmp(btnText, "C") == 0) {
        ResetAll();
        return;
    }

    // Clear current entry only.
    if (strcmp(btnText, "CE") == 0) {
        ClearEntry();
        return;
    }

    // Delete one digit.
    if (strcmp(btnText, "<-") == 0) {
        BackspaceEntry();
        return;
    }
    
    // Digit input.
    if (*btnText >= '0' && *btnText <= '9') {
        char buffer[32];
        GetWindowTextA(hDisplay, buffer, sizeof(buffer));
        
        if (newNumber) {
            strcpy_s(buffer, sizeof(buffer), btnText);
            newNumber = FALSE;
        } else {
            strcat_s(buffer, sizeof(buffer), btnText);
        }
        SetWindowTextA(hDisplay, buffer);
        currentValue = atof(buffer);
    }
    // Decimal point input.
    else if (*btnText == '.') {
        char buffer[32];
        GetWindowTextA(hDisplay, buffer, sizeof(buffer));
        
        if (newNumber) {
            strcpy_s(buffer, sizeof(buffer), "0.");
            newNumber = FALSE;
        } else if (strchr(buffer, '.') == NULL) {
            strcat_s(buffer, sizeof(buffer), ".");
        }
        SetWindowTextA(hDisplay, buffer);
    }
    // Evaluate now, or replay last operation for repeated '='.
    else if (*btnText == '=') {
        if (operation != 0) {
            Calculate();
        } else if (hasRepeatOperation) {
            previousValue = currentValue;
            operation = repeatOperation;
            currentValue = repeatValue;
            Calculate();
        }
    }
    // Operator input (+, -, *, /).
    else {
        if (operation != 0) Calculate();
        previousValue = currentValue;
        operation = *btnText;
        newNumber = TRUE;
    }
}

// Main window procedure that handles painting, input, and control notifications.
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            // Create display brush and display control.
            hDisplayBrush = CreateSolidBrush(displayBackColor);
            hDisplay = CreateWindowExA(0, "EDIT", "0",
                WS_CHILD | WS_VISIBLE | ES_RIGHT | ES_READONLY | ES_AUTOHSCROLL,
                DISPLAY_X, DISPLAY_Y, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                hWnd, (HMENU)(INT_PTR)ID_DISPLAY, NULL, NULL);

            // Create fonts for display text and button text.
            hDisplayFont = CreateFontA(48, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
            hButtonFont = CreateFontA(32, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
            SendMessage(hDisplay, WM_SETFONT, (WPARAM)hDisplayFont, TRUE);
            SendMessage(hDisplay, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(10, 10));
            
            // Create owner-drawn buttons from the label table.
            int col, row, x, y, width;
            for (int i = 0; i < buttonCount; i++) {
                col = i % BUTTON_COLS;
                row = i / BUTTON_COLS;
                x = BUTTON_X + col * (BUTTON_WIDTH + BUTTON_GAP);
                y = BUTTON_Y + row * (BUTTON_HEIGHT + BUTTON_GAP);
                width = BUTTON_WIDTH;

                // Make the 0 key double width in the bottom row.
                if (i == 16) {
                    width = BUTTON_WIDTH * 2 + BUTTON_GAP;
                }
                if (i == 17) {
                    x = BUTTON_X + 2 * (BUTTON_WIDTH + BUTTON_GAP);
                }
                if (i == 18) {
                    x = BUTTON_X + 3 * (BUTTON_WIDTH + BUTTON_GAP);
                }

                CreateWindowExA(0, "BUTTON", buttons[i],
                    WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                    x, y, width, BUTTON_HEIGHT, hWnd, (HMENU)(INT_PTR)(ID_BUTTON_BASE + i), NULL, NULL);
            }

            // Create memory buttons as a single row above the main grid.
            {
                int totalGridWidth = BUTTON_COLS * BUTTON_WIDTH + (BUTTON_COLS - 1) * BUTTON_GAP;
                int memBtnW = (totalGridWidth - (MEM_BUTTON_COUNT - 1) * BUTTON_GAP) / MEM_BUTTON_COUNT;
                for (int i = 0; i < MEM_BUTTON_COUNT; i++) {
                    int mx = BUTTON_X + i * (memBtnW + BUTTON_GAP);
                    CreateWindowExA(0, "BUTTON", memoryButtons[i],
                        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                        mx, MEM_BUTTON_Y, memBtnW, MEM_BUTTON_HEIGHT, hWnd,
                        (HMENU)(INT_PTR)(ID_MEMORY_BUTTON_BASE + i), NULL, NULL);
                }
            }
            break;
        }
        // We paint the full background ourselves, so default erase is unnecessary.
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            RECT clientRect;
            HDC hdc = BeginPaint(hWnd, &ps);

            GetClientRect(hWnd, &clientRect);
            PaintWindowBackground(hdc, &clientRect);
            PaintDisplayFrame(hdc);

            EndPaint(hWnd, &ps);
            return 0;
        }
        // Owner-drawn button paint callback.
        case WM_DRAWITEM:
            if ((wParam >= ID_BUTTON_BASE && wParam < ID_BUTTON_BASE + buttonCount) ||
                (wParam >= ID_MEMORY_BUTTON_BASE && wParam < ID_MEMORY_BUTTON_BASE + MEM_BUTTON_COUNT)) {
                DrawVistaButton((const DRAWITEMSTRUCT*)lParam);
                return TRUE;
            }
            break;
        // Display text/background colors.
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
            if ((HWND)lParam == hDisplay) {
                HDC hdc = (HDC)wParam;
                SetTextColor(hdc, RGB(23, 48, 82));
                SetBkColor(hdc, displayBackColor);
                return (LRESULT)hDisplayBrush;
            }
            break;
        // Button click notifications.
        case WM_COMMAND:
            if (LOWORD(wParam) >= ID_BUTTON_BASE && LOWORD(wParam) < ID_BUTTON_BASE + buttonCount) {
                OnButtonClick(LOWORD(wParam));
            } else if (LOWORD(wParam) >= ID_MEMORY_BUTTON_BASE && LOWORD(wParam) < ID_MEMORY_BUTTON_BASE + MEM_BUTTON_COUNT) {
                OnMemoryButtonClick(LOWORD(wParam));
            }
            break;
        case WM_KEYDOWN: {
            // Map keyboard keys to corresponding button IDs.
            int buttonId = -1;
            switch (wParam) {
                case VK_ESCAPE: buttonId = ID_BUTTON_BASE + 0; break;
                case VK_DELETE: buttonId = ID_BUTTON_BASE + 1; break;
                case VK_BACK: buttonId = ID_BUTTON_BASE + 2; break;
                case VK_OEM_PERIOD: buttonId = ID_BUTTON_BASE + 17; break;
                case VK_OEM_PLUS: buttonId = ID_BUTTON_BASE + 18; break;

                // Numpad numbers
                case VK_NUMPAD0: buttonId = ID_BUTTON_BASE + 16; break;
                case VK_NUMPAD1: buttonId = ID_BUTTON_BASE + 12; break;
                case VK_NUMPAD2: buttonId = ID_BUTTON_BASE + 13; break;
                case VK_NUMPAD3: buttonId = ID_BUTTON_BASE + 14; break;
                case VK_NUMPAD4: buttonId = ID_BUTTON_BASE + 8; break;
                case VK_NUMPAD5: buttonId = ID_BUTTON_BASE + 9; break;
                case VK_NUMPAD6: buttonId = ID_BUTTON_BASE + 10; break;
                case VK_NUMPAD7: buttonId = ID_BUTTON_BASE + 4; break;
                case VK_NUMPAD8: buttonId = ID_BUTTON_BASE + 5; break;
                case VK_NUMPAD9: buttonId = ID_BUTTON_BASE + 6; break;
                
                // Top row numbers
                case '0': buttonId = ID_BUTTON_BASE + 16; break;
                case '1': buttonId = ID_BUTTON_BASE + 12; break;
                case '2': buttonId = ID_BUTTON_BASE + 13; break;
                case '3': buttonId = ID_BUTTON_BASE + 14; break;
                case '4': buttonId = ID_BUTTON_BASE + 8; break;
                case '5': buttonId = ID_BUTTON_BASE + 9; break;
                case '6': buttonId = ID_BUTTON_BASE + 10; break;
                case '7': buttonId = ID_BUTTON_BASE + 4; break;
                case '8': buttonId = ID_BUTTON_BASE + 5; break;
                case '9': buttonId = ID_BUTTON_BASE + 6; break;
                
                // Decimal point
                case VK_DECIMAL:
                    buttonId = ID_BUTTON_BASE + 17; break;
                
                // Operators
                case VK_DIVIDE:
                case '/': buttonId = ID_BUTTON_BASE + 3; break;
                case VK_MULTIPLY:
                case '*': buttonId = ID_BUTTON_BASE + 7; break;
                case VK_SUBTRACT:
                case '-': buttonId = ID_BUTTON_BASE + 11; break;
                case VK_ADD:
                case '+': buttonId = ID_BUTTON_BASE + 15; break;
                
                // Equals
                case VK_RETURN:
                    buttonId = ID_BUTTON_BASE + 18; break;
            }
            if (buttonId != -1) {
                OnButtonClick(buttonId);
                return 0;
            }
            break;
        }
        // Redraw custom background on size changes.
        case WM_SIZE:
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        case WM_DESTROY:
            // Release GDI objects we created.
            DeleteObject(hDisplayFont);
            DeleteObject(hButtonFont);
            DeleteObject(hDisplayBrush);
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcA(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// Win32 application entry point.
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const char CLASS_NAME[] = "CalcWindow";
    // Window style: fixed-size standard window.
    DWORD windowStyle = WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;
    RECT windowRect;

    // Compute needed client size from the configured button grid.
    int rows = (buttonCount + BUTTON_COLS - 1) / BUTTON_COLS;
    int clientWidth = BUTTON_X + BUTTON_COLS * BUTTON_WIDTH + (BUTTON_COLS - 1) * BUTTON_GAP + CONTENT_RIGHT_PADDING;
    int clientHeight = BUTTON_Y + rows * BUTTON_HEIGHT + (rows - 1) * BUTTON_GAP + CONTENT_BOTTOM_PADDING;
    
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    RegisterClassA(&wc);

    // Convert desired client size to outer window size.
    windowRect.left = 0;
    windowRect.top = 0;
    windowRect.right = clientWidth;
    windowRect.bottom = clientHeight;
    AdjustWindowRect(&windowRect, windowStyle, FALSE);
    
    HWND hWnd = CreateWindowExA(0, CLASS_NAME, "Calculator",
        windowStyle,
        CW_USEDEFAULT, CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        NULL, NULL, hInstance, NULL);
    
    ShowWindow(hWnd, nCmdShow);
    
    // Standard Win32 message loop.
    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
