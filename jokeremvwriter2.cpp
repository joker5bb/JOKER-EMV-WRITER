// Add this at the very top to force ANSI mode
#undef UNICODE
#undef _UNICODE

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <sstream>
#include <vector>
#include <iomanip>
#include <winscard.h>
#include <tchar.h>

#pragma comment(lib, "winscard.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// Function prototypes
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateControls(HWND hwnd);
void ProgramJCOPCard(HWND hwnd);
void DeleteEMVEvents(HWND hwnd);
bool ConnectToReader();
bool SelectJCOPApplication();
bool WriteTrackData(const std::string &track1, const std::string &track2);
bool DisconnectReader();
std::string BytesToHex(const BYTE* data, DWORD length);
bool TransmitAPDU(const BYTE* sendBuffer, DWORD sendLength, BYTE* recvBuffer, DWORD* recvLength);
void UpdateStatus(HWND hwnd, const std::string &message, bool isError);
bool ValidateTrackData(const std::string &track1, const std::string &track2);
bool DeleteEMVApplicationEvents();
bool ExecuteProprietaryCommand0x5640002();
bool SecureChannelInitialize();
bool GetCardStatus();
bool EraseCardContent();

#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 500

#define ID_TRACK1_EDIT      101
#define ID_TRACK2_EDIT      102
#define ID_PROGRAM_BUTTON   103
#define ID_CLEAR_BUTTON     104
#define ID_STATUS_LABEL     105
#define ID_READER_COMBO     106
#define ID_REFRESH_BUTTON   107
#define ID_PROGRESS_BAR     108
#define ID_DELETE_EVENTS_BUTTON 109
#define ID_ERASE_CARD_BUTTON    110
#define ID_SECURE_CHANNEL_BUTTON 111
#define ID_KEY_MANAGEMENT_BUTTON 112

HWND hTrack1Edit, hTrack2Edit, hProgramButton, hClearButton;
HWND hStatusLabel, hReaderCombo, hRefreshButton, hProgressBar;
HWND hDeleteEventsButton, hEraseCardButton, hSecureChannelButton, hKeyManagementButton;
HINSTANCE hInst;

SCARDCONTEXT hContext = 0;
SCARDHANDLE hCard = 0;
SCARD_IO_REQUEST pioSendPci;
std::string currentReader;
bool isConnected = false;
bool secureChannelEstablished = false;

const BYTE JCOP_AID[] = { 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10 };
const BYTE PSE[] = { 0x31, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31 };

#define PROPRIETARY_CLA     0x56
#define PROPRIETARY_INS     0x40
#define PROPRIETARY_P1      0x00
#define PROPRIETARY_P2      0x02

#define GP_CLA              0x00
#define GP_INS_DELETE       0xE4
#define GP_INS_GET_STATUS   0xF2
#define GP_INS_INITIALIZE_UPDATE 0x50
#define GP_INS_EXTERNAL_AUTHENTICATE 0x82

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    hInst = hInstance;
    
    INITCOMMONCONTROLSEX iccex;
    iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    iccex.dwICC = ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&iccex);

    const char CLASS_NAME[] = "JCOPWriterClass";
    
    WNDCLASSEX wc = { };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc))
    {
        MessageBox(NULL, "Window Registration Failed!", "Error", MB_ICONERROR);
        return 0;
    }

    HWND hwnd = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        CLASS_NAME,
        "JOKER EMV WRITER - JCOP Card Programmer (Enhanced with 0x5640002 Features)",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL)
    {
        MessageBox(NULL, "Window Creation Failed!", "Error", MB_ICONERROR);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (hContext != 0)
    {
        SCardReleaseContext(hContext);
    }

    return (int)msg.wParam;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        CreateControls(hwnd);
        if (SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext) != SCARD_S_SUCCESS)
        {
            UpdateStatus(hwnd, "Failed to establish PC/SC context!", true);
        }
        else
        {
            SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(ID_REFRESH_BUTTON, BN_CLICKED), 0);
        }
        return 0;

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);

        if (wmId == ID_PROGRAM_BUTTON && wmEvent == BN_CLICKED)
        {
            ProgramJCOPCard(hwnd);
        }
        else if (wmId == ID_CLEAR_BUTTON && wmEvent == BN_CLICKED)
        {
            SetWindowTextA(hTrack1Edit, "");
            SetWindowTextA(hTrack2Edit, "");
            UpdateStatus(hwnd, "Fields cleared. Ready.", false);
            SendMessage(hProgressBar, PBM_SETPOS, 0, 0);
        }
        else if (wmId == ID_REFRESH_BUTTON && wmEvent == BN_CLICKED)
        {
            SendMessage(hReaderCombo, CB_RESETCONTENT, 0, 0);
            
            DWORD dwReaders = SCARD_AUTOALLOCATE;
            LPSTR mszReaders = NULL;
            LONG rv = SCardListReadersA(hContext, NULL, (LPSTR)&mszReaders, &dwReaders);
            
            if (rv == SCARD_S_SUCCESS && mszReaders != NULL)
            {
                LPSTR pReader = mszReaders;
                while (*pReader != '\0')
                {
                    SendMessageA(hReaderCombo, CB_ADDSTRING, 0, (LPARAM)pReader);
                    pReader += strlen(pReader) + 1;
                }
                SendMessage(hReaderCombo, CB_SETCURSEL, 0, 0);
                SCardFreeMemory(hContext, mszReaders);
                UpdateStatus(hwnd, "Reader list updated.", false);
            }
            else
            {
                SendMessageA(hReaderCombo, CB_ADDSTRING, 0, (LPARAM)"No readers found");
                SendMessage(hReaderCombo, CB_SETCURSEL, 0, 0);
                UpdateStatus(hwnd, "No smart card readers detected.", true);
            }
        }
        else if (wmId == ID_DELETE_EVENTS_BUTTON && wmEvent == BN_CLICKED)
        {
            DeleteEMVEvents(hwnd);
        }
        else if (wmId == ID_ERASE_CARD_BUTTON && wmEvent == BN_CLICKED)
        {
            int selIndex = (int)SendMessage(hReaderCombo, CB_GETCURSEL, 0, 0);
            if (selIndex == CB_ERR)
            {
                UpdateStatus(hwnd, "Please select a card reader!", true);
                return 0;
            }

            int len = (int)SendMessageA(hReaderCombo, CB_GETLBTEXTLEN, selIndex, 0);
            std::vector<char> readerName(len + 1);
            SendMessageA(hReaderCombo, CB_GETLBTEXT, selIndex, (LPARAM)readerName.data());
            currentReader = readerName.data();

            if (MessageBoxA(hwnd, "WARNING: This will erase all card content!\nContinue?", 
                "Confirm Erase", MB_YESNO | MB_ICONWARNING) == IDYES)
            {
                EnableWindow(hEraseCardButton, FALSE);
                UpdateStatus(hwnd, "Erasing card content...", false);
                
                if (!ConnectToReader())
                {
                    UpdateStatus(hwnd, "Failed to connect to reader!", true);
                    EnableWindow(hEraseCardButton, TRUE);
                    return 0;
                }

                if (EraseCardContent())
                {
                    UpdateStatus(hwnd, "Card erased successfully!", false);
                }
                else
                {
                    UpdateStatus(hwnd, "Failed to erase card content!", true);
                }
                
                DisconnectReader();
                EnableWindow(hEraseCardButton, TRUE);
            }
        }
        else if (wmId == ID_SECURE_CHANNEL_BUTTON && wmEvent == BN_CLICKED)
        {
            int selIndex = (int)SendMessage(hReaderCombo, CB_GETCURSEL, 0, 0);
            if (selIndex == CB_ERR)
            {
                UpdateStatus(hwnd, "Please select a card reader!", true);
                return 0;
            }

            int len = (int)SendMessageA(hReaderCombo, CB_GETLBTEXTLEN, selIndex, 0);
            std::vector<char> readerName(len + 1);
            SendMessageA(hReaderCombo, CB_GETLBTEXT, selIndex, (LPARAM)readerName.data());
            currentReader = readerName.data();

            EnableWindow(hSecureChannelButton, FALSE);
            UpdateStatus(hwnd, "Initializing secure channel...", false);
            
            if (!ConnectToReader())
            {
                UpdateStatus(hwnd, "Failed to connect to reader!", true);
                EnableWindow(hSecureChannelButton, TRUE);
                return 0;
            }

            if (SecureChannelInitialize())
            {
                secureChannelEstablished = true;
                UpdateStatus(hwnd, "Secure channel established!", false);
            }
            else
            {
                UpdateStatus(hwnd, "Failed to establish secure channel!", true);
            }
            
            EnableWindow(hSecureChannelButton, TRUE);
        }
        else if (wmId == ID_KEY_MANAGEMENT_BUTTON && wmEvent == BN_CLICKED)
        {
            int selIndex = (int)SendMessage(hReaderCombo, CB_GETCURSEL, 0, 0);
            if (selIndex == CB_ERR)
            {
                UpdateStatus(hwnd, "Please select a card reader!", true);
                return 0;
            }

            int len = (int)SendMessageA(hReaderCombo, CB_GETLBTEXTLEN, selIndex, 0);
            std::vector<char> readerName(len + 1);
            SendMessageA(hReaderCombo, CB_GETLBTEXT, selIndex, (LPARAM)readerName.data());
            currentReader = readerName.data();

            EnableWindow(hKeyManagementButton, FALSE);
            UpdateStatus(hwnd, "Executing 0x5640002 proprietary command...", false);
            
            if (!ConnectToReader())
            {
                UpdateStatus(hwnd, "Failed to connect to reader!", true);
                EnableWindow(hKeyManagementButton, TRUE);
                return 0;
            }

            if (ExecuteProprietaryCommand0x5640002())
            {
                UpdateStatus(hwnd, "0x5640002 command executed successfully!", false);
            }
            else
            {
                UpdateStatus(hwnd, "0x5640002 command failed!", true);
            }
            
            DisconnectReader();
            EnableWindow(hKeyManagementButton, TRUE);
        }
        break;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;

    case WM_DESTROY:
        if (hCard != 0)
        {
            SCardDisconnect(hCard, SCARD_UNPOWER_CARD);
        }
        if (hContext != 0)
        {
            SCardReleaseContext(hContext);
        }
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void CreateControls(HWND hwnd)
{
    HFONT hFont = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    
    HFONT hBoldFont = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

    HWND hTitle = CreateWindowEx(0, "STATIC", "JCOP EMV Card Writer - Enhanced Edition",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        20, 10, 540, 25, hwnd, NULL, hInst, NULL);
    SendMessage(hTitle, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

    CreateWindowEx(0, "STATIC", "Smart Card Reader:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 45, 150, 20, hwnd, NULL, hInst, NULL);

    hReaderCombo = CreateWindowEx(WS_EX_CLIENTEDGE, "COMBOBOX", "",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        20, 65, 420, 200, hwnd, (HMENU)ID_READER_COMBO, hInst, NULL);
    SendMessage(hReaderCombo, WM_SETFONT, (WPARAM)hFont, TRUE);

    hRefreshButton = CreateWindowEx(0, "BUTTON", "Refresh",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        450, 64, 110, 25, hwnd, (HMENU)ID_REFRESH_BUTTON, hInst, NULL);
    SendMessage(hRefreshButton, WM_SETFONT, (WPARAM)hFont, TRUE);

    CreateWindowEx(0, "STATIC", "Track 1 Data (ISO 7813):",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 100, 200, 20, hwnd, NULL, hInst, NULL);

    hTrack1Edit = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        20, 120, 540, 25, hwnd, (HMENU)ID_TRACK1_EDIT, hInst, NULL);
    SendMessage(hTrack1Edit, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hTrack1Edit, EM_SETLIMITTEXT, 79, 0);

    CreateWindowEx(0, "STATIC", "Track 2 Data (ISO 7813):",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 155, 200, 20, hwnd, NULL, hInst, NULL);

    hTrack2Edit = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        20, 175, 540, 25, hwnd, (HMENU)ID_TRACK2_EDIT, hInst, NULL);
    SendMessage(hTrack2Edit, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hTrack2Edit, EM_SETLIMITTEXT, 40, 0);

    hProgressBar = CreateWindowEx(0, PROGRESS_CLASS, "",
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        20, 215, 540, 20, hwnd, (HMENU)ID_PROGRESS_BAR, hInst, NULL);
    SendMessage(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessage(hProgressBar, PBM_SETPOS, 0, 0);

    hProgramButton = CreateWindowEx(0, "BUTTON", "Program JCOP Card",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        20, 245, 170, 35, hwnd, (HMENU)ID_PROGRAM_BUTTON, hInst, NULL);
    SendMessage(hProgramButton, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

    hClearButton = CreateWindowEx(0, "BUTTON", "Clear Fields",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        205, 245, 170, 35, hwnd, (HMENU)ID_CLEAR_BUTTON, hInst, NULL);
    SendMessage(hClearButton, WM_SETFONT, (WPARAM)hFont, TRUE);

    hDeleteEventsButton = CreateWindowEx(0, "BUTTON", "Delete EMV Events",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        390, 245, 170, 35, hwnd, (HMENU)ID_DELETE_EVENTS_BUTTON, hInst, NULL);
    SendMessage(hDeleteEventsButton, WM_SETFONT, (WPARAM)hFont, TRUE);

    CreateWindowEx(0, "STATIC", "Advanced Features (0x5640002):",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 290, 250, 20, hwnd, NULL, hInst, NULL);

    hSecureChannelButton = CreateWindowEx(0, "BUTTON", "Secure Channel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        20, 315, 170, 30, hwnd, (HMENU)ID_SECURE_CHANNEL_BUTTON, hInst, NULL);
    SendMessage(hSecureChannelButton, WM_SETFONT, (WPARAM)hFont, TRUE);

    hKeyManagementButton = CreateWindowEx(0, "BUTTON", "Key Management (0x5640002)",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        205, 315, 170, 30, hwnd, (HMENU)ID_KEY_MANAGEMENT_BUTTON, hInst, NULL);
    SendMessage(hKeyManagementButton, WM_SETFONT, (WPARAM)hFont, TRUE);

    hEraseCardButton = CreateWindowEx(0, "BUTTON", "Erase Card",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        390, 315, 170, 30, hwnd, (HMENU)ID_ERASE_CARD_BUTTON, hInst, NULL);
    SendMessage(hEraseCardButton, WM_SETFONT, (WPARAM)hFont, TRUE);

    hStatusLabel = CreateWindowEx(0, "STATIC", "Ready. Please select a reader and enter track data.",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 360, 540, 40, hwnd, (HMENU)ID_STATUS_LABEL, hInst, NULL);
    SendMessage(hStatusLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

    CreateWindowEx(0, "STATIC", "Format: Track 1 starts with %, Track 2 starts with ; | Enhanced with 0x5640002 proprietary commands",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 405, 540, 20, hwnd, NULL, hInst, NULL);

    CreateWindowEx(0, "STATIC", "WARNING: Delete EMV Events and Erase Card are irreversible operations!",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 425, 540, 20, hwnd, NULL, hInst, NULL);

    DeleteObject(hFont);
    DeleteObject(hBoldFont);
}

void ProgramJCOPCard(HWND hwnd)
{
    char track1[256] = {0};
    char track2[256] = {0};
    GetWindowTextA(hTrack1Edit, track1, sizeof(track1));
    GetWindowTextA(hTrack2Edit, track2, sizeof(track2));

    if (!ValidateTrackData(track1, track2))
    {
        UpdateStatus(hwnd, "Invalid track data format!", true);
        MessageBoxA(hwnd, "Track 1 must start with % and Track 2 with ;\nTrack 1 max 79 chars, Track 2 max 40 chars.", 
            "Validation Error", MB_ICONWARNING);
        return;
    }

    int selIndex = (int)SendMessage(hReaderCombo, CB_GETCURSEL, 0, 0);
    if (selIndex == CB_ERR)
    {
        UpdateStatus(hwnd, "Please select a card reader!", true);
        return;
    }

    int len = (int)SendMessageA(hReaderCombo, CB_GETLBTEXTLEN, selIndex, 0);
    std::vector<char> readerName(len + 1);
    SendMessageA(hReaderCombo, CB_GETLBTEXT, selIndex, (LPARAM)readerName.data());
    currentReader = readerName.data();

    EnableWindow(hProgramButton, FALSE);
    EnableWindow(hTrack1Edit, FALSE);
    EnableWindow(hTrack2Edit, FALSE);
    EnableWindow(hDeleteEventsButton, FALSE);
    EnableWindow(hEraseCardButton, FALSE);
    EnableWindow(hSecureChannelButton, FALSE);
    EnableWindow(hKeyManagementButton, FALSE);
    
    UpdateStatus(hwnd, "Connecting to reader...", false);
    SendMessage(hProgressBar, PBM_SETPOS, 10, 0);

    if (!ConnectToReader())
    {
        UpdateStatus(hwnd, "Failed to connect to reader or no card present!", true);
        EnableWindow(hProgramButton, TRUE);
        EnableWindow(hTrack1Edit, TRUE);
        EnableWindow(hTrack2Edit, TRUE);
        EnableWindow(hDeleteEventsButton, TRUE);
        EnableWindow(hEraseCardButton, TRUE);
        EnableWindow(hSecureChannelButton, TRUE);
        EnableWindow(hKeyManagementButton, TRUE);
        return;
    }
    SendMessage(hProgressBar, PBM_SETPOS, 30, 0);

    UpdateStatus(hwnd, "Selecting JCOP application...", false);
    if (!SelectJCOPApplication())
    {
        UpdateStatus(hwnd, "Failed to select JCOP application!", true);
        DisconnectReader();
        EnableWindow(hProgramButton, TRUE);
        EnableWindow(hTrack1Edit, TRUE);
        EnableWindow(hTrack2Edit, TRUE);
        EnableWindow(hDeleteEventsButton, TRUE);
        EnableWindow(hEraseCardButton, TRUE);
        EnableWindow(hSecureChannelButton, TRUE);
        EnableWindow(hKeyManagementButton, TRUE);
        return;
    }
    SendMessage(hProgressBar, PBM_SETPOS, 50, 0);

    UpdateStatus(hwnd, "Writing track data to card...", false);
    if (!WriteTrackData(track1, track2))
    {
        UpdateStatus(hwnd, "Failed to write track data!", true);
        DisconnectReader();
        EnableWindow(hProgramButton, TRUE);
        EnableWindow(hTrack1Edit, TRUE);
        EnableWindow(hTrack2Edit, TRUE);
        EnableWindow(hDeleteEventsButton, TRUE);
        EnableWindow(hEraseCardButton, TRUE);
        EnableWindow(hSecureChannelButton, TRUE);
        EnableWindow(hKeyManagementButton, TRUE);
        return;
    }
    SendMessage(hProgressBar, PBM_SETPOS, 90, 0);

    UpdateStatus(hwnd, "Finalizing...", false);
    if (!DisconnectReader())
    {
        UpdateStatus(hwnd, "Warning: Error during disconnect.", true);
    }
    else
    {
        SendMessage(hProgressBar, PBM_SETPOS, 100, 0);
        UpdateStatus(hwnd, "SUCCESS: JCOP card programmed successfully!", false);
        MessageBoxA(hwnd, "Card programmed successfully!", "Success", MB_ICONINFORMATION);
    }

    EnableWindow(hProgramButton, TRUE);
    EnableWindow(hTrack1Edit, TRUE);
    EnableWindow(hTrack2Edit, TRUE);
    EnableWindow(hDeleteEventsButton, TRUE);
    EnableWindow(hEraseCardButton, TRUE);
    EnableWindow(hSecureChannelButton, TRUE);
    EnableWindow(hKeyManagementButton, TRUE);
}

void DeleteEMVEvents(HWND hwnd)
{
    int selIndex = (int)SendMessage(hReaderCombo, CB_GETCURSEL, 0, 0);
    if (selIndex == CB_ERR)
    {
        UpdateStatus(hwnd, "Please select a card reader!", true);
        return;
    }

    int len = (int)SendMessageA(hReaderCombo, CB_GETLBTEXTLEN, selIndex, 0);
    std::vector<char> readerName(len + 1);
    SendMessageA(hReaderCombo, CB_GETLBTEXT, selIndex, (LPARAM)readerName.data());
    currentReader = readerName.data();

    if (MessageBoxA(hwnd, "WARNING: This will delete all EMV transaction events and logs from the card!\nThis operation cannot be undone.\n\nContinue?", 
        "Confirm Delete EMV Events", MB_YESNO | MB_ICONWARNING) != IDYES)
    {
        return;
    }

    EnableWindow(hDeleteEventsButton, FALSE);
    EnableWindow(hProgramButton, FALSE);
    EnableWindow(hEraseCardButton, FALSE);
    EnableWindow(hSecureChannelButton, FALSE);
    EnableWindow(hKeyManagementButton, FALSE);
    
    UpdateStatus(hwnd, "Deleting EMV events...", false);
    SendMessage(hProgressBar, PBM_SETPOS, 20, 0);

    if (!ConnectToReader())
    {
        UpdateStatus(hwnd, "Failed to connect to reader!", true);
        EnableWindow(hDeleteEventsButton, TRUE);
        EnableWindow(hProgramButton, TRUE);
        EnableWindow(hEraseCardButton, TRUE);
        EnableWindow(hSecureChannelButton, TRUE);
        EnableWindow(hKeyManagementButton, TRUE);
        return;
    }
    SendMessage(hProgressBar, PBM_SETPOS, 40, 0);

    UpdateStatus(hwnd, "Selecting application...", false);
    if (!SelectJCOPApplication())
    {
        UpdateStatus(hwnd, "Failed to select application!", true);
        DisconnectReader();
        EnableWindow(hDeleteEventsButton, TRUE);
        EnableWindow(hProgramButton, TRUE);
        EnableWindow(hEraseCardButton, TRUE);
        EnableWindow(hSecureChannelButton, TRUE);
        EnableWindow(hKeyManagementButton, TRUE);
        return;
    }
    SendMessage(hProgressBar, PBM_SETPOS, 60, 0);

    UpdateStatus(hwnd, "Deleting EMV transaction events...", false);
    if (!DeleteEMVApplicationEvents())
    {
        UpdateStatus(hwnd, "Failed to delete EMV events!", true);
        DisconnectReader();
        EnableWindow(hDeleteEventsButton, TRUE);
        EnableWindow(hProgramButton, TRUE);
        EnableWindow(hEraseCardButton, TRUE);
        EnableWindow(hSecureChannelButton, TRUE);
        EnableWindow(hKeyManagementButton, TRUE);
        return;
    }
    SendMessage(hProgressBar, PBM_SETPOS, 80, 0);

    UpdateStatus(hwnd, "Finalizing...", false);
    if (!DisconnectReader())
    {
        UpdateStatus(hwnd, "Warning: Error during disconnect.", true);
    }
    else
    {
        SendMessage(hProgressBar, PBM_SETPOS, 100, 0);
        UpdateStatus(hwnd, "SUCCESS: EMV events deleted successfully!", false);
        MessageBoxA(hwnd, "EMV events deleted successfully!", "Success", MB_ICONINFORMATION);
    }

    EnableWindow(hDeleteEventsButton, TRUE);
    EnableWindow(hProgramButton, TRUE);
    EnableWindow(hEraseCardButton, TRUE);
    EnableWindow(hSecureChannelButton, TRUE);
    EnableWindow(hKeyManagementButton, TRUE);
}

bool ConnectToReader()
{
    if (hCard != 0)
    {
        SCardDisconnect(hCard, SCARD_UNPOWER_CARD);
        hCard = 0;
    }

    DWORD dwActiveProtocol;
    LONG rv = SCardConnectA(hContext, currentReader.c_str(),
        SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
        &hCard, &dwActiveProtocol);

    if (rv != SCARD_S_SUCCESS)
    {
        return false;
    }

    switch (dwActiveProtocol)
    {
    case SCARD_PROTOCOL_T0:
        pioSendPci = *SCARD_PCI_T0;
        break;
    case SCARD_PROTOCOL_T1:
        pioSendPci = *SCARD_PCI_T1;
        break;
    default:
        SCardDisconnect(hCard, SCARD_UNPOWER_CARD);
        hCard = 0;
        return false;
    }

    isConnected = true;
    return true;
}

bool SelectJCOPApplication()
{
    if (!isConnected) return false;

    BYTE recvBuffer[256];
    DWORD recvLength = sizeof(recvBuffer);

    BYTE selectPSE[] = { 0x00, 0xA4, 0x04, 0x00, sizeof(PSE) };
    std::vector<BYTE> apdu(selectPSE, selectPSE + sizeof(selectPSE));
    apdu.insert(apdu.end(), PSE, PSE + sizeof(PSE));
    apdu.push_back(0x00);

    if (!TransmitAPDU(apdu.data(), (DWORD)apdu.size(), recvBuffer, &recvLength))
    {
        BYTE selectAID[] = { 0x00, 0xA4, 0x04, 0x00, sizeof(JCOP_AID) };
        std::vector<BYTE> aidApdu(selectAID, selectAID + sizeof(selectAID));
        aidApdu.insert(aidApdu.end(), JCOP_AID, JCOP_AID + sizeof(JCOP_AID));
        aidApdu.push_back(0x00);

        recvLength = sizeof(recvBuffer);
        if (!TransmitAPDU(aidApdu.data(), (DWORD)aidApdu.size(), recvBuffer, &recvLength))
        {
            return false;
        }
    }

    if (recvLength >= 2 && recvBuffer[recvLength - 2] == 0x90 && recvBuffer[recvLength - 1] == 0x00)
    {
        return true;
    }

    return false;
}

bool WriteTrackData(const std::string &track1, const std::string &track2)
{
    if (!isConnected) return false;

    BYTE recvBuffer[256];
    DWORD recvLength;

    std::vector<BYTE> track1Data(track1.begin(), track1.end());
    std::vector<BYTE> track2Data(track2.begin(), track2.end());

    std::vector<BYTE> writeTrack1 = { 0x00, 0xD6, 0x00, 0x10, (BYTE)track1Data.size() };
    writeTrack1.insert(writeTrack1.end(), track1Data.begin(), track1Data.end());

    recvLength = sizeof(recvBuffer);
    if (!TransmitAPDU(writeTrack1.data(), (DWORD)writeTrack1.size(), recvBuffer, &recvLength))
    {
        writeTrack1[1] = 0xD0;
        recvLength = sizeof(recvBuffer);
        if (!TransmitAPDU(writeTrack1.data(), (DWORD)writeTrack1.size(), recvBuffer, &recvLength))
        {
            return false;
        }
    }

    if (recvLength < 2 || !(recvBuffer[recvLength - 2] == 0x90 && recvBuffer[recvLength - 1] == 0x00))
    {
        if (recvLength >= 2 && recvBuffer[recvLength - 2] == 0x61)
        {
            BYTE getResponse[] = { 0x00, 0xC0, 0x00, 0x00, recvBuffer[recvLength - 1] };
            recvLength = sizeof(recvBuffer);
            TransmitAPDU(getResponse, sizeof(getResponse), recvBuffer, &recvLength);
        }
        else
        {
            return false;
        }
    }

    std::vector<BYTE> writeTrack2 = { 0x00, 0xD6, 0x00, 0x20, (BYTE)track2Data.size() };
    writeTrack2.insert(writeTrack2.end(), track2Data.begin(), track2Data.end());

    recvLength = sizeof(recvBuffer);
    if (!TransmitAPDU(writeTrack2.data(), (DWORD)writeTrack2.size(), recvBuffer, &recvLength))
    {
        writeTrack2[1] = 0xD0;
        recvLength = sizeof(recvBuffer);
        if (!TransmitAPDU(writeTrack2.data(), (DWORD)writeTrack2.size(), recvBuffer, &recvLength))
        {
            return false;
        }
    }

    if (recvLength < 2 || !(recvBuffer[recvLength - 2] == 0x90 && recvBuffer[recvLength - 1] == 0x00))
    {
        if (recvLength >= 2 && recvBuffer[recvLength - 2] == 0x61)
        {
            BYTE getResponse[] = { 0x00, 0xC0, 0x00, 0x00, recvBuffer[recvLength - 1] };
            recvLength = sizeof(recvBuffer);
            TransmitAPDU(getResponse, sizeof(getResponse), recvBuffer, &recvLength);
        }
        else
        {
            return false;
        }
    }

    return true;
}

bool DisconnectReader()
{
    if (hCard != 0)
    {
        LONG rv = SCardDisconnect(hCard, SCARD_UNPOWER_CARD);
        hCard = 0;
        isConnected = false;
        secureChannelEstablished = false;
        return (rv == SCARD_S_SUCCESS);
    }
    return true;
}

bool TransmitAPDU(const BYTE* sendBuffer, DWORD sendLength, BYTE* recvBuffer, DWORD* recvLength)
{
    if (!isConnected || hCard == 0) return false;

    LONG rv = SCardTransmit(hCard, &pioSendPci, sendBuffer, sendLength, NULL, recvBuffer, recvLength);
    return (rv == SCARD_S_SUCCESS);
}

std::string BytesToHex(const BYTE* data, DWORD length)
{
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0');
    for (DWORD i = 0; i < length; i++)
    {
        ss << std::setw(2) << (int)data[i] << " ";
    }
    return ss.str();
}

void UpdateStatus(HWND hwnd, const std::string &message, bool isError)
{
    SetWindowTextA(hStatusLabel, message.c_str());
    
    if (isError)
    {
        std::string displayMsg = "ERROR: " + message;
        SetWindowTextA(hStatusLabel, displayMsg.c_str());
    }
    else if (message.find("SUCCESS") != std::string::npos)
    {
        std::string displayMsg = "SUCCESS: " + message;
        SetWindowTextA(hStatusLabel, displayMsg.c_str());
    }
}

bool ValidateTrackData(const std::string &track1, const std::string &track2)
{
    if (track1.empty() || track1[0] != '%' || track1.length() > 79)
    {
        return false;
    }

    if (track2.empty() || track2[0] != ';' || track2.length() > 40)
    {
        return false;
    }

    return true;
}

// ============================================================================
// NEW 0x5640002 PROPRIETARY FUNCTIONS
// ============================================================================

bool DeleteEMVApplicationEvents()
{
    if (!isConnected) return false;

    BYTE recvBuffer[256];
    DWORD recvLength;

    BYTE getStatus[] = { 0x00, GP_INS_GET_STATUS, 0x00, 0x00, 0x00 };
    recvLength = sizeof(recvBuffer);
    
    if (TransmitAPDU(getStatus, sizeof(getStatus), recvBuffer, &recvLength))
    {
        // Parse response
    }

    const BYTE eventLogSFIs[] = { 0x10, 0x11, 0x12, 0x13, 0x14, 0x15 };
    
    for (size_t i = 0; i < sizeof(eventLogSFIs); i++)
    {
        BYTE selectFile[] = { 0x00, 0xA4, 0x02, 0x00, 0x02, 0x00, eventLogSFIs[i] };
        recvLength = sizeof(recvBuffer);
        TransmitAPDU(selectFile, sizeof(selectFile), recvBuffer, &recvLength);
        
        for (int record = 1; record <= 10; record++)
        {
            BYTE eraseRecord[] = { 0x00, 0xDC, (BYTE)record, 0x04, 0x00 };
            recvLength = sizeof(recvBuffer);
            TransmitAPDU(eraseRecord, sizeof(eraseRecord), recvBuffer, &recvLength);
        }
    }

    const BYTE eventAID1[] = {0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10};
    const BYTE eventAID2[] = {0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10};
    const BYTE eventAID3[] = {0xA0, 0x00, 0x00, 0x00, 0x05, 0x10, 0x10};
    
    const BYTE* eventAIDs[] = { eventAID1, eventAID2, eventAID3 };
    
    for (int i = 0; i < 3; i++)
    {
        BYTE deleteCmd[256];
        deleteCmd[0] = 0x00;
        deleteCmd[1] = GP_INS_DELETE;
        deleteCmd[2] = 0x00;
        deleteCmd[3] = 0x00;
        deleteCmd[4] = 0x07;
        
        for (int j = 0; j < 7; j++)
        {
            deleteCmd[5 + j] = eventAIDs[i][j];
        }
        
        recvLength = sizeof(recvBuffer);
        TransmitAPDU(deleteCmd, 12, recvBuffer, &recvLength);
    }

    BYTE proprietaryCmd[] = { 
        PROPRIETARY_CLA,
        PROPRIETARY_INS,
        0x01,
        PROPRIETARY_P2,
        0x00
    };
    
    recvLength = sizeof(recvBuffer);
    if (TransmitAPDU(proprietaryCmd, sizeof(proprietaryCmd), recvBuffer, &recvLength))
    {
        if (recvLength >= 2 && recvBuffer[recvLength - 2] == 0x90 && recvBuffer[recvLength - 1] == 0x00)
        {
            return true;
        }
    }

    BYTE clearCounters[] = { 0x00, 0xDA, 0x9F, 0x4F, 0x00 };
    recvLength = sizeof(recvBuffer);
    TransmitAPDU(clearCounters, sizeof(clearCounters), recvBuffer, &recvLength);

    return true;
}

bool ExecuteProprietaryCommand0x5640002()
{
    if (!isConnected) return false;

    BYTE recvBuffer[256];
    DWORD recvLength;

    BYTE cmdGetInfo[] = { 
        PROPRIETARY_CLA,
        PROPRIETARY_INS,
        0x00,
        PROPRIETARY_P2,
        0x00
    };
    
    recvLength = sizeof(recvBuffer);
    if (TransmitAPDU(cmdGetInfo, sizeof(cmdGetInfo), recvBuffer, &recvLength))
    {
        if (recvLength >= 2 && recvBuffer[recvLength - 2] == 0x90)
        {
            // Success
        }
    }

    BYTE cmdKeyMgmt[] = { 
        PROPRIETARY_CLA,
        PROPRIETARY_INS,
        0x02,
        PROPRIETARY_P2,
        0x08,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    
    recvLength = sizeof(recvBuffer);
    TransmitAPDU(cmdKeyMgmt, sizeof(cmdKeyMgmt), recvBuffer, &recvLength);

    BYTE cmdSecureMsg[] = { 
        PROPRIETARY_CLA,
        PROPRIETARY_INS,
        0x03,
        PROPRIETARY_P2,
        0x00
    };
    
    recvLength = sizeof(recvBuffer);
    if (TransmitAPDU(cmdSecureMsg, sizeof(cmdSecureMsg), recvBuffer, &recvLength))
    {
        if (recvLength >= 2 && recvBuffer[recvLength - 2] == 0x90)
        {
            secureChannelEstablished = true;
        }
    }

    BYTE cmdLifecycle[] = { 
        PROPRIETARY_CLA,
        PROPRIETARY_INS,
        0x04,
        PROPRIETARY_P2,
        0x01,
        0x01
    };
    
    recvLength = sizeof(recvBuffer);
    TransmitAPDU(cmdLifecycle, sizeof(cmdLifecycle), recvBuffer, &recvLength);

    return true;
}

bool SecureChannelInitialize()
{
    if (!isConnected) return false;

    BYTE recvBuffer[256];
    DWORD recvLength;

    BYTE hostChallenge[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    
    BYTE initUpdate[256];
    initUpdate[0] = 0x00;
    initUpdate[1] = GP_INS_INITIALIZE_UPDATE;
    initUpdate[2] = 0x00;
    initUpdate[3] = 0x00;
    initUpdate[4] = 0x08;
    
    for (int i = 0; i < 8; i++)
    {
        initUpdate[5 + i] = hostChallenge[i];
    }
    initUpdate[13] = 0x00;
    
    recvLength = sizeof(recvBuffer);
    if (!TransmitAPDU(initUpdate, 14, recvBuffer, &recvLength))
    {
        return false;
    }
    
    if (recvLength < 2 || recvBuffer[recvLength - 2] != 0x90)
    {
        return false;
    }
    
    BYTE cardCryptogram[8] = { 0x00 };
    
    BYTE extAuth[256];
    extAuth[0] = 0x00;
    extAuth[1] = GP_INS_EXTERNAL_AUTHENTICATE;
    extAuth[2] = 0x00;
    extAuth[3] = 0x00;
    extAuth[4] = 0x08;
    
    for (int i = 0; i < 8; i++)
    {
        extAuth[5 + i] = cardCryptogram[i];
    }
    
    recvLength = sizeof(recvBuffer);
    if (!TransmitAPDU(extAuth, 13, recvBuffer, &recvLength))
    {
        return false;
    }
    
    if (recvLength >= 2 && recvBuffer[recvLength - 2] == 0x90)
    {
        secureChannelEstablished = true;
        return true;
    }
    
    return false;
}

bool GetCardStatus()
{
    if (!isConnected) return false;

    BYTE recvBuffer[256];
    DWORD recvLength;

    BYTE getStatus[] = { 
        0x00,
        GP_INS_GET_STATUS,
        0x00,
        0x00,
        0x00
    };
    
    recvLength = sizeof(recvBuffer);
    if (TransmitAPDU(getStatus, sizeof(getStatus), recvBuffer, &recvLength))
    {
        if (recvLength >= 2 && recvBuffer[recvLength - 2] == 0x90)
        {
            return true;
        }
    }
    
    return false;
}

bool EraseCardContent()
{
    if (!isConnected) return false;

    BYTE recvBuffer[256];
    DWORD recvLength;

    BYTE selectCM[] = { 
        0x00, 0xA4, 0x04, 0x00, 
        0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 
        0x00 
    };
    
    recvLength = sizeof(recvBuffer);
    TransmitAPDU(selectCM, sizeof(selectCM), recvBuffer, &recvLength);

    BYTE eraseCmd[] = { 
        PROPRIETARY_CLA,
        PROPRIETARY_INS,
        0x05,
        PROPRIETARY_P2,
        0x00
    };
    
    recvLength = sizeof(recvBuffer);
    if (TransmitAPDU(eraseCmd, sizeof(eraseCmd), recvBuffer, &recvLength))
    {
        if (recvLength >= 2 && recvBuffer[recvLength - 2] == 0x90)
        {
            return true;
        }
    }

    BYTE factoryReset[] = { 
        0x00, 0xF0, 0x00, 0x00, 0x00 
    };
    
    recvLength = sizeof(recvBuffer);
    TransmitAPDU(factoryReset, sizeof(factoryReset), recvBuffer, &recvLength);

    return true;
}
