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

// Link libraries (ignored by g++ but kept for MSVC compatibility)
#pragma comment(lib, "winscard.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// Function prototypes
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateControls(HWND hwnd);
void ProgramJCOPCard(HWND hwnd);
bool ConnectToReader();
bool SelectJCOPApplication();
bool WriteTrackData(const std::string &track1, const std::string &track2);
bool DisconnectReader();
std::string BytesToHex(const BYTE* data, DWORD length);
bool TransmitAPDU(const BYTE* sendBuffer, DWORD sendLength, BYTE* recvBuffer, DWORD* recvLength);
void UpdateStatus(HWND hwnd, const std::string &message, bool isError);
bool ValidateTrackData(const std::string &track1, const std::string &track2);

// Window dimensions
#define WINDOW_WIDTH 500
#define WINDOW_HEIGHT 400

// Control IDs
#define ID_TRACK1_EDIT      101
#define ID_TRACK2_EDIT      102
#define ID_PROGRAM_BUTTON   103
#define ID_CLEAR_BUTTON     104
#define ID_STATUS_LABEL     105
#define ID_READER_COMBO     106
#define ID_REFRESH_BUTTON   107
#define ID_PROGRESS_BAR     108

// Global variables
HWND hTrack1Edit, hTrack2Edit, hProgramButton, hClearButton;
HWND hStatusLabel, hReaderCombo, hRefreshButton, hProgressBar;
HINSTANCE hInst;

// PC/SC Variables - use 0 instead of NULL for handles
SCARDCONTEXT hContext = 0;
SCARDHANDLE hCard = 0;
SCARD_IO_REQUEST pioSendPci;
std::string currentReader;
bool isConnected = false;

// JCOP Application AID (commonly used for JCOP cards)
const BYTE JCOP_AID[] = { 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10 };
const BYTE PSE[] = { 0x31, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31 };

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    hInst = hInstance;
    
    // Initialize Common Controls
    INITCOMMONCONTROLSEX iccex;
    iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    iccex.dwICC = ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&iccex);

    // Register the window class
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

    // Create the window
    HWND hwnd = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        CLASS_NAME,
        "JOKER EMV WRITER - JCOP Card Programmer",
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

    // Run the message loop
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
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
        // Initialize PC/SC context
        if (SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext) != SCARD_S_SUCCESS)
        {
            UpdateStatus(hwnd, "Failed to establish PC/SC context!", true);
        }
        else
        {
            // Populate reader list
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
            // Refresh reader list
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

    // Title label
    HWND hTitle = CreateWindowEx(0, "STATIC", "JCOP EMV Card Writer",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        20, 10, 440, 25, hwnd, NULL, hInst, NULL);
    SendMessage(hTitle, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

    // Reader Selection
    CreateWindowEx(0, "STATIC", "Smart Card Reader:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 45, 150, 20, hwnd, NULL, hInst, NULL);

    hReaderCombo = CreateWindowEx(WS_EX_CLIENTEDGE, "COMBOBOX", "",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        20, 65, 320, 200, hwnd, (HMENU)ID_READER_COMBO, hInst, NULL);
    SendMessage(hReaderCombo, WM_SETFONT, (WPARAM)hFont, TRUE);

    hRefreshButton = CreateWindowEx(0, "BUTTON", "Refresh",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        350, 64, 110, 25, hwnd, (HMENU)ID_REFRESH_BUTTON, hInst, NULL);
    SendMessage(hRefreshButton, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Track 1 Input
    CreateWindowEx(0, "STATIC", "Track 1 Data (ISO 7813):",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 100, 200, 20, hwnd, NULL, hInst, NULL);

    hTrack1Edit = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        20, 120, 440, 25, hwnd, (HMENU)ID_TRACK1_EDIT, hInst, NULL);
    SendMessage(hTrack1Edit, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hTrack1Edit, EM_SETLIMITTEXT, 79, 0); // Max 79 chars for Track 1

    // Track 2 Input
    CreateWindowEx(0, "STATIC", "Track 2 Data (ISO 7813):",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 155, 200, 20, hwnd, NULL, hInst, NULL);

    hTrack2Edit = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        20, 175, 440, 25, hwnd, (HMENU)ID_TRACK2_EDIT, hInst, NULL);
    SendMessage(hTrack2Edit, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hTrack2Edit, EM_SETLIMITTEXT, 40, 0); // Max 40 chars for Track 2

    // Progress Bar
    hProgressBar = CreateWindowEx(0, PROGRESS_CLASS, "",
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        20, 215, 440, 20, hwnd, (HMENU)ID_PROGRESS_BAR, hInst, NULL);
    SendMessage(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessage(hProgressBar, PBM_SETPOS, 0, 0);

    // Buttons
    hProgramButton = CreateWindowEx(0, "BUTTON", "Program JCOP Card",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        20, 250, 215, 35, hwnd, (HMENU)ID_PROGRAM_BUTTON, hInst, NULL);
    SendMessage(hProgramButton, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

    hClearButton = CreateWindowEx(0, "BUTTON", "Clear Fields",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        245, 250, 215, 35, hwnd, (HMENU)ID_CLEAR_BUTTON, hInst, NULL);
    SendMessage(hClearButton, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Status Label
    hStatusLabel = CreateWindowEx(0, "STATIC", "Ready. Please select a reader and enter track data.",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 300, 440, 40, hwnd, (HMENU)ID_STATUS_LABEL, hInst, NULL);
    SendMessage(hStatusLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Info label
    CreateWindowEx(0, "STATIC", "Format: Track 1 starts with %, Track 2 starts with ;",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 345, 440, 20, hwnd, NULL, hInst, NULL);

    DeleteObject(hFont);
    DeleteObject(hBoldFont);
}

void ProgramJCOPCard(HWND hwnd)
{
    // Get track data
    char track1[256] = {0};
    char track2[256] = {0};
    GetWindowTextA(hTrack1Edit, track1, sizeof(track1));
    GetWindowTextA(hTrack2Edit, track2, sizeof(track2));

    // Validate input
    if (!ValidateTrackData(track1, track2))
    {
        UpdateStatus(hwnd, "Invalid track data format!", true);
        MessageBoxA(hwnd, "Track 1 must start with % and Track 2 with ;\nTrack 1 max 79 chars, Track 2 max 40 chars.", 
            "Validation Error", MB_ICONWARNING);
        return;
    }

    // Get selected reader
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

    // Disable controls during operation
    EnableWindow(hProgramButton, FALSE);
    EnableWindow(hTrack1Edit, FALSE);
    EnableWindow(hTrack2Edit, FALSE);
    UpdateStatus(hwnd, "Connecting to reader...", false);
    SendMessage(hProgressBar, PBM_SETPOS, 10, 0);

    // Step 1: Connect to reader
    if (!ConnectToReader())
    {
        UpdateStatus(hwnd, "Failed to connect to reader or no card present!", true);
        EnableWindow(hProgramButton, TRUE);
        EnableWindow(hTrack1Edit, TRUE);
        EnableWindow(hTrack2Edit, TRUE);
        return;
    }
    SendMessage(hProgressBar, PBM_SETPOS, 30, 0);

    // Step 2: Select JCOP Application
    UpdateStatus(hwnd, "Selecting JCOP application...", false);
    if (!SelectJCOPApplication())
    {
        UpdateStatus(hwnd, "Failed to select JCOP application!", true);
        DisconnectReader();
        EnableWindow(hProgramButton, TRUE);
        EnableWindow(hTrack1Edit, TRUE);
        EnableWindow(hTrack2Edit, TRUE);
        return;
    }
    SendMessage(hProgressBar, PBM_SETPOS, 50, 0);

    // Step 3: Write Track Data
    UpdateStatus(hwnd, "Writing track data to card...", false);
    if (!WriteTrackData(track1, track2))
    {
        UpdateStatus(hwnd, "Failed to write track data!", true);
        DisconnectReader();
        EnableWindow(hProgramButton, TRUE);
        EnableWindow(hTrack1Edit, TRUE);
        EnableWindow(hTrack2Edit, TRUE);
        return;
    }
    SendMessage(hProgressBar, PBM_SETPOS, 90, 0);

    // Step 4: Disconnect
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

    // Re-enable controls
    EnableWindow(hProgramButton, TRUE);
    EnableWindow(hTrack1Edit, TRUE);
    EnableWindow(hTrack2Edit, TRUE);
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

    // Set protocol
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

    // Try selecting PSE first (Payment System Environment)
    BYTE selectPSE[] = { 0x00, 0xA4, 0x04, 0x00, sizeof(PSE) };
    std::vector<BYTE> apdu(selectPSE, selectPSE + sizeof(selectPSE));
    apdu.insert(apdu.end(), PSE, PSE + sizeof(PSE));
    apdu.push_back(0x00); // Le

    if (!TransmitAPDU(apdu.data(), (DWORD)apdu.size(), recvBuffer, &recvLength))
    {
        // If PSE fails, try direct JCOP AID selection
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

    // Check SW1 SW2 (should be 90 00 for success)
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

    // Convert track data to bytes
    std::vector<BYTE> track1Data(track1.begin(), track1.end());
    std::vector<BYTE> track2Data(track2.begin(), track2.end());

    // Write Track 1 to file 0x10 (example file ID for Track 1)
    // APDU: UPDATE BINARY or WRITE BINARY
    // CLA INS P1 P2 Lc Data
    std::vector<BYTE> writeTrack1 = { 0x00, 0xD6, 0x00, 0x10, (BYTE)track1Data.size() };
    writeTrack1.insert(writeTrack1.end(), track1Data.begin(), track1Data.end());

    recvLength = sizeof(recvBuffer);
    if (!TransmitAPDU(writeTrack1.data(), (DWORD)writeTrack1.size(), recvBuffer, &recvLength))
    {
        // Try alternative instruction code
        writeTrack1[1] = 0xD0; // WRITE BINARY
        recvLength = sizeof(recvBuffer);
        if (!TransmitAPDU(writeTrack1.data(), (DWORD)writeTrack1.size(), recvBuffer, &recvLength))
        {
            return false;
        }
    }

    // Verify response
    if (recvLength < 2 || !(recvBuffer[recvLength - 2] == 0x90 && recvBuffer[recvLength - 1] == 0x00))
    {
        // Some cards return 61 XX meaning more data available
        if (recvLength >= 2 && recvBuffer[recvLength - 2] == 0x61)
        {
            // Get Response
            BYTE getResponse[] = { 0x00, 0xC0, 0x00, 0x00, recvBuffer[recvLength - 1] };
            recvLength = sizeof(recvBuffer);
            TransmitAPDU(getResponse, sizeof(getResponse), recvBuffer, &recvLength);
        }
        else
        {
            return false;
        }
    }

    // Write Track 2 to file 0x20 (example file ID for Track 2)
    std::vector<BYTE> writeTrack2 = { 0x00, 0xD6, 0x00, 0x20, (BYTE)track2Data.size() };
    writeTrack2.insert(writeTrack2.end(), track2Data.begin(), track2Data.end());

    recvLength = sizeof(recvBuffer);
    if (!TransmitAPDU(writeTrack2.data(), (DWORD)writeTrack2.size(), recvBuffer, &recvLength))
    {
        writeTrack2[1] = 0xD0; // Try WRITE BINARY
        recvLength = sizeof(recvBuffer);
        if (!TransmitAPDU(writeTrack2.data(), (DWORD)writeTrack2.size(), recvBuffer, &recvLength))
        {
            return false;
        }
    }

    // Verify response
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
    
    // Set text color based on status
    if (isError)
    {
        // Red color for errors - requires owner draw or simple color change not directly supported
        // For simplicity, we prepend ERROR: or SUCCESS:
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
    // Track 1 validation: Starts with %, max 79 characters
    if (track1.empty() || track1[0] != '%' || track1.length() > 79)
    {
        return false;
    }

    // Track 2 validation: Starts with ;, max 40 characters, numeric only (except sentinel)
    if (track2.empty() || track2[0] != ';' || track2.length() > 40)
    {
        return false;
    }

    // Check Track 2 format (should end with ? usually)
    if (track2[track2.length() - 1] != '?')
    {
        // Not strictly required but recommended
    }

    return true;
}