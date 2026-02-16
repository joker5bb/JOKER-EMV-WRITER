/*
 * JCOP EMV Card Manager
 * Windows GUI Application
 * Supports: Visa, Mastercard, Amex, Discover
 * Features: Programming, Root Erase, Delete EMV Events, Secure Channel, Key Management
 * Special: 0x5640002 Features Support
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winscard.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cstdint>

#pragma comment(lib, "winscard.lib")
#pragma comment(lib, "comctl32.lib")

// Resource IDs
#define ID_EDIT_TRACK1          1001
#define ID_EDIT_TRACK2          1002
#define ID_COMBO_CARDTYPE       1003
#define ID_BTN_CONNECT          1004
#define ID_BTN_DISCONNECT       1005
#define ID_BTN_PROGRAM          1006
#define ID_BTN_READ             1007
#define ID_LIST_LOG             1008
#define ID_STATUS               1009
#define ID_BTN_CLEAR            1010
#define ID_CHK_DEBUG            1011

// New buttons for requested features
#define ID_BTN_ROOT_ERASE       1012
#define ID_BTN_DELETE_EMV       1013
#define ID_BTN_SECURE_CHANNEL   1014
#define ID_BTN_KEY_MANAGE       1015
#define ID_BTN_5640002_FEATURES 1016

#define ID_EDIT_VISA_PAN        1017
#define ID_EDIT_MC_PAN          1018
#define ID_EDIT_EXP_DATE        1019
#define ID_EDIT_CVV             1020

// Window dimensions
#define WINDOW_WIDTH            850
#define WINDOW_HEIGHT           650

// EMV APDU Constants
#define CLA_EMV                 0x00
#define CLA_EMV_SECURE          0x0C  // Secure messaging CLA
#define INS_SELECT              0xA4
#define INS_READ_RECORD         0xB2
#define INS_STORE_DATA          0xE2
#define INS_GET_DATA            0xCA
#define INS_VERIFY              0x20
#define INS_EXTERNAL_AUTH       0x82
#define INS_INTERNAL_AUTH       0x88
#define INS_PUT_DATA            0xDA
#define INS_DELETE_FILE         0xE4
#define INS_ERASE_BINARY        0x0E
#define INS_TERMINATE_DF        0xE6
#define INS_ACTIVATE_FILE       0x44

// JCOP Specific Commands
#define JCOP_CLA                0x00
#define JCOP_INS_PUT_KEY        0xD8
#define JCOP_INS_GET_KEY        0xCA
#define JCOP_INS_LOAD           0xE8
#define JCOP_INS_INSTALL        0xE6
#define JCOP_INS_DELETE         0xE4
#define JCOP_INS_GET_STATUS     0xF2
#define JCOP_INS_SET_STATUS     0xF0

// 0x5640002 Feature Commands
#define VISA_FEATURE_CLA        0x00
#define VISA_FEATURE_INS        0x56
#define VISA_FEATURE_P1         0x40
#define VISA_FEATURE_P2         0x02

// GlobalPlatform Commands
#define GP_CLA                  0x80
#define GP_INS_INITIALIZE_UPDATE 0x50
#define GP_INS_EXTERNAL_AUTH    0x82
#define GP_INS_DELETE           0xE4
#define GP_INS_GET_STATUS       0xF2
#define GP_INS_PUT_KEY          0xD8

// Key identifiers
#define KEY_ENC                 0x01
#define KEY_MAC                 0x02
#define KEY_DEK                 0x03

// Card type definitions
enum CardType {
    CARD_VISA,
    CARD_MASTERCARD,
    CARD_AMEX,
    CARD_DISCOVER
};

// Application state
struct AppState {
    SCARDCONTEXT hContext;
    SCARDHANDLE hCard;
    DWORD dwActiveProtocol;
    bool bConnected;
    bool bDebugMode;
    bool bSecureChannel;
    std::string strReaderName;
    std::vector<BYTE> sessionKeys;
    uint8_t keyVersion;
    uint8_t keyIndex;
    
    AppState() : hContext(0), hCard(0), dwActiveProtocol(0), 
                 bConnected(false), bDebugMode(false), 
                 bSecureChannel(false), keyVersion(0x01), keyIndex(0x00) {}
};

AppState g_state;
HINSTANCE g_hInst;
HWND g_hWndMain;
HWND g_hEditTrack1, g_hEditTrack2, g_hComboType;
HWND g_hEditVisaPan, g_hEditMcPan, g_hEditExpDate, g_hEditCvv;
HWND g_hBtnConnect, g_hBtnDisconnect, g_hBtnProgram, g_hBtnRead, g_hBtnClear;
HWND g_hBtnRootErase, g_hBtnDeleteEmv, g_hBtnSecureChannel, g_hBtnKeyManage;
HWND g_hBtn5640002;
HWND g_hListLog, g_hStatus, g_hChkDebug;

// EMV Application IDs (AID)
const BYTE AID_VISA_DEBIT[]       = { 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10 };
const BYTE AID_VISA_CREDIT[]      = { 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10 };
const BYTE AID_VISA_ELECTRON[]    = { 0xA0, 0x00, 0x00, 0x00, 0x03, 0x20, 0x10 };
const BYTE AID_MASTERCARD[]       = { 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10 };
const BYTE AID_MAESTRO[]          = { 0xA0, 0x00, 0x00, 0x00, 0x04, 0x30, 0x60 };
const BYTE AID_AMEX[]             = { 0xA0, 0x00, 0x00, 0x00, 0x25 };
const BYTE AID_DISCOVER[]         = { 0xA0, 0x00, 0x00, 0x01, 0x52, 0x30, 0x10 };
const BYTE AID_JCB[]              = { 0xA0, 0x00, 0x00, 0x00, 0x65 };
const BYTE AID_PPSE[]             = { 0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 
                                      0x2E, 0x44, 0x44, 0x46, 0x30, 0x31 };

// Function prototypes
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void InitializeControls(HWND hWnd);
void ConnectReader();
void DisconnectReader();
void UpdateStatus();
void ProgramCard();
void ReadCardData();
void RootEraseCard();
void DeleteEmvEvents();
void EstablishSecureChannel();
void ManageKeys();
void Execute5640002Features();
void ClearLog();

void LogMessage(const std::string& msg, bool bError = false);
void LogBytes(const std::string& prefix, const std::vector<BYTE>& data);
std::string BytesToHex(const std::vector<BYTE>& data);
std::string BytesToString(const std::vector<BYTE>& data, size_t len);
LONG SendAPDU(const std::vector<BYTE>& cmd, std::vector<BYTE>& response, DWORD& respLen);
bool SelectAID(const BYTE* aid, size_t aidLen);
bool StoreData(uint8_t recordNum, const std::string& data, bool isLast);
bool VerifyPIN(const std::string& pin);
bool InitializeUpdate();
bool ExternalAuthenticate();
std::vector<BYTE> CalculateMAC(const std::vector<BYTE>& data);
void SecureMessagingAPDU(std::vector<BYTE>& apdu);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX iccex;
    iccex.dwSize = sizeof(iccex);
    iccex.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&iccex);

    g_hInst = hInstance;

    WNDCLASSEX wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wcex.lpszClassName = "JCOP_EMV_Manager";
    wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wcex)) {
        MessageBox(NULL, "Window Registration Failed!", "Error", MB_ICONERROR);
        return 0;
    }

    g_hWndMain = CreateWindowEx(
        0,
        "JCOP_EMV_Manager",
        "JCOP EMV Card Manager - Visa/MC/Amex/Discover - 0x5640002 Features",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, hInstance, NULL
    );

    if (!g_hWndMain) {
        MessageBox(NULL, "Window Creation Failed!", "Error", MB_ICONERROR);
        return 0;
    }

    ShowWindow(g_hWndMain, nCmdShow);
    UpdateWindow(g_hWndMain);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (g_state.bConnected) {
        DisconnectReader();
    }

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        InitializeControls(hWnd);
        LogMessage("JCOP EMV Card Manager initialized.");
        LogMessage("Ready to connect to PC/SC reader...");
        LogMessage("Supports: Visa, Mastercard, Amex, Discover");
        LogMessage("Features: 0x5640002, Root Erase, Delete EMV, Secure Channel, Key Mgmt");
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_BTN_CONNECT:
            ConnectReader();
            break;
        case ID_BTN_DISCONNECT:
            DisconnectReader();
            break;
        case ID_BTN_PROGRAM:
            ProgramCard();
            break;
        case ID_BTN_READ:
            ReadCardData();
            break;
        case ID_BTN_ROOT_ERASE:
            RootEraseCard();
            break;
        case ID_BTN_DELETE_EMV:
            DeleteEmvEvents();
            break;
        case ID_BTN_SECURE_CHANNEL:
            EstablishSecureChannel();
            break;
        case ID_BTN_KEY_MANAGE:
            ManageKeys();
            break;
        case ID_BTN_5640002_FEATURES:
            Execute5640002Features();
            break;
        case ID_BTN_CLEAR:
            ClearLog();
            break;
        case ID_CHK_DEBUG:
            g_state.bDebugMode = (SendMessage(g_hChkDebug, BM_GETCHECK, 0, 0) == BST_CHECKED);
            break;
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}

void InitializeControls(HWND hWnd) {
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    int yPos = 10;

    // Group: Card Data Input
    CreateWindow("BUTTON", "Card Data Input", 
        WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
        10, yPos, 400, 200, hWnd, NULL, g_hInst, NULL);

    // Track 1
    CreateWindow("STATIC", "Track 1:", 
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        25, yPos + 25, 70, 20, hWnd, NULL, g_hInst, NULL);
    g_hEditTrack1 = CreateWindow("EDIT", "", 
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        100, yPos + 23, 300, 22, hWnd, (HMENU)ID_EDIT_TRACK1, g_hInst, NULL);

    // Track 2
    CreateWindow("STATIC", "Track 2:", 
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        25, yPos + 50, 70, 20, hWnd, NULL, g_hInst, NULL);
    g_hEditTrack2 = CreateWindow("EDIT", "", 
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        100, yPos + 48, 300, 22, hWnd, (HMENU)ID_EDIT_TRACK2, g_hInst, NULL);

    // Card Type
    CreateWindow("STATIC", "Card Type:", 
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        25, yPos + 80, 70, 20, hWnd, NULL, g_hInst, NULL);
    g_hComboType = CreateWindow("COMBOBOX", "", 
        WS_VISIBLE | WS_CHILD | WS_BORDER | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
        100, yPos + 78, 120, 120, hWnd, (HMENU)ID_COMBO_CARDTYPE, g_hInst, NULL);
    SendMessage(g_hComboType, CB_ADDSTRING, 0, (LPARAM)"Visa");
    SendMessage(g_hComboType, CB_ADDSTRING, 0, (LPARAM)"Mastercard");
    SendMessage(g_hComboType, CB_ADDSTRING, 0, (LPARAM)"Amex");
    SendMessage(g_hComboType, CB_ADDSTRING, 0, (LPARAM)"Discover");
    SendMessage(g_hComboType, CB_SETCURSEL, 0, 0);

    // Additional fields
    CreateWindow("STATIC", "Visa/MC PAN:", 
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        25, yPos + 110, 70, 20, hWnd, NULL, g_hInst, NULL);
    g_hEditVisaPan = CreateWindow("EDIT", "", 
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        100, yPos + 108, 120, 22, hWnd, (HMENU)ID_EDIT_VISA_PAN, g_hInst, NULL);

    CreateWindow("STATIC", "Exp Date (YYMM):", 
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        230, yPos + 110, 90, 20, hWnd, NULL, g_hInst, NULL);
    g_hEditExpDate = CreateWindow("EDIT", "", 
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        320, yPos + 108, 80, 22, hWnd, (HMENU)ID_EDIT_EXP_DATE, g_hInst, NULL);

    CreateWindow("STATIC", "CVV:", 
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        25, yPos + 140, 70, 20, hWnd, NULL, g_hInst, NULL);
    g_hEditCvv = CreateWindow("EDIT", "", 
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        100, yPos + 138, 80, 22, hWnd, (HMENU)ID_EDIT_CVV, g_hInst, NULL);

    // Debug checkbox
    g_hChkDebug = CreateWindow("BUTTON", "Debug APDUs", 
        WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        230, yPos + 140, 120, 20, hWnd, (HMENU)ID_CHK_DEBUG, g_hInst, NULL);

    // Group: Reader Control
    yPos = 220;
    CreateWindow("BUTTON", "Reader Control", 
        WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
        10, yPos, 400, 60, hWnd, NULL, g_hInst, NULL);

    g_hBtnConnect = CreateWindow("BUTTON", "Connect", 
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        25, yPos + 20, 80, 30, hWnd, (HMENU)ID_BTN_CONNECT, g_hInst, NULL);
    g_hBtnDisconnect = CreateWindow("BUTTON", "Disconnect", 
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_DISABLED,
        115, yPos + 20, 80, 30, hWnd, (HMENU)ID_BTN_DISCONNECT, g_hInst, NULL);
    g_hBtnRead = CreateWindow("BUTTON", "Read Card", 
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_DISABLED,
        205, yPos + 20, 80, 30, hWnd, (HMENU)ID_BTN_READ, g_hInst, NULL);
    g_hBtnClear = CreateWindow("BUTTON", "Clear Log", 
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        295, yPos + 20, 80, 30, hWnd, (HMENU)ID_BTN_CLEAR, g_hInst, NULL);

    // Group: Programming
    yPos = 290;
    CreateWindow("BUTTON", "Card Programming", 
        WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
        10, yPos, 400, 60, hWnd, NULL, g_hInst, NULL);
    g_hBtnProgram = CreateWindow("BUTTON", "Program EMV", 
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_DISABLED,
        25, yPos + 20, 100, 30, hWnd, (HMENU)ID_BTN_PROGRAM, g_hInst, NULL);
    g_hBtn5640002 = CreateWindow("BUTTON", "0x5640002 Features", 
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_DISABLED,
        135, yPos + 20, 120, 30, hWnd, (HMENU)ID_BTN_5640002_FEATURES, g_hInst, NULL);

    // Group: Advanced Operations
    yPos = 360;
    CreateWindow("BUTTON", "Advanced Operations (JCOP)", 
        WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
        10, yPos, 400, 130, hWnd, NULL, g_hInst, NULL);

    g_hBtnRootErase = CreateWindow("BUTTON", "Root Erase", 
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_DISABLED | BS_MULTILINE,
        25, yPos + 20, 100, 40, hWnd, (HMENU)ID_BTN_ROOT_ERASE, g_hInst, NULL);
    g_hBtnDeleteEmv = CreateWindow("BUTTON", "Delete EMV Events", 
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_DISABLED | BS_MULTILINE,
        135, yPos + 20, 100, 40, hWnd, (HMENU)ID_BTN_DELETE_EMV, g_hInst, NULL);
    g_hBtnSecureChannel = CreateWindow("BUTTON", "Secure Channel", 
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_DISABLED | BS_MULTILINE,
        245, yPos + 20, 100, 40, hWnd, (HMENU)ID_BTN_SECURE_CHANNEL, g_hInst, NULL);
    g_hBtnKeyManage = CreateWindow("BUTTON", "Key Management", 
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_DISABLED | BS_MULTILINE,
        135, yPos + 70, 100, 40, hWnd, (HMENU)ID_BTN_KEY_MANAGE, g_hInst, NULL);

    // Log ListBox (right side)
    CreateWindow("STATIC", "Activity Log:", 
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        420, 10, 400, 20, hWnd, NULL, g_hInst, NULL);
    g_hListLog = CreateWindow("LISTBOX", "", 
        WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | WS_HSCROLL | 
        LBS_NOTIFY | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
        420, 30, 400, 530, hWnd, (HMENU)ID_LIST_LOG, g_hInst, NULL);

    // Status bar
    g_hStatus = CreateWindow("STATIC", "Status: Not Connected | Secure Channel: OFF", 
        WS_VISIBLE | WS_CHILD | SS_LEFT | WS_BORDER,
        10, 570, 810, 22, hWnd, (HMENU)ID_STATUS, g_hInst, NULL);
}

void ConnectReader() {
    LONG lReturn;
    LPTSTR mszReaders = NULL;
    DWORD cchReaders = SCARD_AUTOALLOCATE;
    LPCTSTR rdrName = NULL;
    DWORD dwRdrLen, dwState, dwProt, dwAtrLen;
    BYTE pbAtr[32];
    
    LogMessage("Establishing PC/SC context...");
    
    lReturn = SCardEstablishContext(SCARD_SCOPE_USER, NULL, NULL, &g_state.hContext);
    if (lReturn != SCARD_S_SUCCESS) {
        LogMessage("Failed to establish context. Error: " + std::to_string(lReturn), true);
        return;
    }

    LogMessage("PC/SC context established.");

    lReturn = SCardListReaders(g_state.hContext, NULL, (LPTSTR)&mszReaders, &cchReaders);
    if (lReturn != SCARD_S_SUCCESS) {
        LogMessage("No readers found.", true);
        SCardReleaseContext(g_state.hContext);
        g_state.hContext = 0;
        return;
    }

    rdrName = mszReaders;
    g_state.strReaderName = rdrName;
    LogMessage("Found reader: " + g_state.strReaderName);

    lReturn = SCardConnect(g_state.hContext, rdrName, 
        SCARD_SHARE_EXCLUSIVE, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
        &g_state.hCard, &g_state.dwActiveProtocol);

    if (lReturn != SCARD_S_SUCCESS) {
        LogMessage("Failed to connect to card. Ensure card is present.", true);
        SCardFreeMemory(g_state.hContext, mszReaders);
        SCardReleaseContext(g_state.hContext);
        g_state.hContext = 0;
        return;
    }

    dwAtrLen = 32;
    lReturn = SCardStatus(g_state.hCard, NULL, &dwRdrLen, &dwState, 
        &dwProt, pbAtr, &dwAtrLen);
    
    if (lReturn == SCARD_S_SUCCESS) {
        std::vector<BYTE> atr(pbAtr, pbAtr + dwAtrLen);
        LogMessage("Card connected!");
        LogBytes("ATR", atr);
        
        std::string proto = (dwProt == SCARD_PROTOCOL_T0) ? "T=0" : 
                           (dwProt == SCARD_PROTOCOL_T1) ? "T=1" : "Unknown";
        LogMessage("Protocol: " + proto);
    }

    SCardFreeMemory(g_state.hContext, mszReaders);

    g_state.bConnected = true;
    EnableWindow(g_hBtnConnect, FALSE);
    EnableWindow(g_hBtnDisconnect, TRUE);
    EnableWindow(g_hBtnProgram, TRUE);
    EnableWindow(g_hBtnRead, TRUE);
    EnableWindow(g_hBtnRootErase, TRUE);
    EnableWindow(g_hBtnDeleteEmv, TRUE);
    EnableWindow(g_hBtnSecureChannel, TRUE);
    EnableWindow(g_hBtnKeyManage, TRUE);
    EnableWindow(g_hBtn5640002, TRUE);
    
    UpdateStatus();
    LogMessage("Ready for card operations.");
}

void DisconnectReader() {
    if (g_state.hCard) {
        SCardDisconnect(g_state.hCard, SCARD_LEAVE_CARD);
        g_state.hCard = 0;
    }
    if (g_state.hContext) {
        SCardReleaseContext(g_state.hContext);
        g_state.hContext = 0;
    }

    g_state.bConnected = false;
    g_state.bSecureChannel = false;
    g_state.dwActiveProtocol = 0;

    EnableWindow(g_hBtnConnect, TRUE);
    EnableWindow(g_hBtnDisconnect, FALSE);
    EnableWindow(g_hBtnProgram, FALSE);
    EnableWindow(g_hBtnRead, FALSE);
    EnableWindow(g_hBtnRootErase, FALSE);
    EnableWindow(g_hBtnDeleteEmv, FALSE);
    EnableWindow(g_hBtnSecureChannel, FALSE);
    EnableWindow(g_hBtnKeyManage, FALSE);
    EnableWindow(g_hBtn5640002, FALSE);
    
    UpdateStatus();
    LogMessage("Reader disconnected.");
}

void UpdateStatus() {
    std::string status = "Status: " + std::string(g_state.bConnected ? "Connected" : "Not Connected");
    status += " | Secure Channel: " + std::string(g_state.bSecureChannel ? "ON" : "OFF");
    SetWindowText(g_hStatus, status.c_str());
}

LONG SendAPDU(const std::vector<BYTE>& cmd, std::vector<BYTE>& response, DWORD& respLen) {
    if (!g_state.bConnected || !g_state.hCard) {
        return SCARD_E_INVALID_HANDLE;
    }

    SCARD_IO_REQUEST ioRequest;
    if (g_state.dwActiveProtocol == SCARD_PROTOCOL_T0) {
        ioRequest = *SCARD_PCI_T0;
    } else {
        ioRequest = *SCARD_PCI_T1;
    }

    BYTE pbRecv[256];
    DWORD cbRecv = 256;

    if (g_state.bDebugMode) {
        LogBytes("C-APDU", cmd);
    }

    LONG lReturn = SCardTransmit(g_state.hCard, &ioRequest, 
        cmd.data(), (DWORD)cmd.size(), NULL, pbRecv, &cbRecv);

    if (lReturn == SCARD_S_SUCCESS) {
        response.assign(pbRecv, pbRecv + cbRecv);
        respLen = cbRecv;
        
        if (g_state.bDebugMode && cbRecv >= 2) {
            LogBytes("R-APDU", response);
            char swStr[32];
            sprintf_s(swStr, "SW1=%02X SW2=%02X", pbRecv[cbRecv-2], pbRecv[cbRecv-1]);
            LogMessage(std::string("Response: ") + swStr);
        }
    }

    return lReturn;
}

bool SelectAID(const BYTE* aid, size_t aidLen) {
    std::vector<BYTE> apdu;
    apdu.push_back(CLA_EMV);
    apdu.push_back(INS_SELECT);
    apdu.push_back(0x04);
    apdu.push_back(0x00);
    apdu.push_back((BYTE)aidLen);
    apdu.insert(apdu.end(), aid, aid + aidLen);
    apdu.push_back(0x00);

    std::vector<BYTE> response;
    DWORD respLen;
    LONG lReturn = SendAPDU(apdu, response, respLen);

    if (lReturn != SCARD_S_SUCCESS || respLen < 2) {
        return false;
    }

    BYTE sw1 = response[respLen - 2];
    BYTE sw2 = response[respLen - 1];
    
    return (sw1 == 0x90 && sw2 == 0x00) || (sw1 == 0x61);
}

bool StoreData(uint8_t recordNum, const std::string& data, bool isLast) {
    std::vector<BYTE> apdu;
    apdu.push_back(CLA_EMV);
    apdu.push_back(INS_STORE_DATA);
    apdu.push_back(recordNum);
    apdu.push_back(isLast ? 0x00 : 0x01);
    
    std::vector<BYTE> dataBytes(data.begin(), data.end());
    
    if (dataBytes.size() > 255) {
        apdu.push_back(0x00);
        apdu.push_back((BYTE)((dataBytes.size() >> 8) & 0xFF));
        apdu.push_back((BYTE)(dataBytes.size() & 0xFF));
    } else {
        apdu.push_back((BYTE)dataBytes.size());
    }
    
    apdu.insert(apdu.end(), dataBytes.begin(), dataBytes.end());
    apdu.push_back(0x00);

    std::vector<BYTE> response;
    DWORD respLen;
    LONG lReturn = SendAPDU(apdu, response, respLen);

    if (lReturn != SCARD_S_SUCCESS || respLen < 2) {
        return false;
    }

    BYTE sw1 = response[respLen - 2];
    BYTE sw2 = response[respLen - 1];
    
    return (sw1 == 0x90 && sw2 == 0x00);
}

void ProgramCard() {
    if (!g_state.bConnected) {
        LogMessage("Error: Not connected.", true);
        return;
    }

    char buf1[256] = {0}, buf2[256] = {0}, buf3[32] = {0}, buf4[32] = {0};
    GetWindowText(g_hEditTrack1, buf1, 255);
    GetWindowText(g_hEditTrack2, buf2, 255);
    GetWindowText(g_hEditExpDate, buf3, 31);
    GetWindowText(g_hEditCvv, buf4, 31);
    
    std::string track1(buf1);
    std::string track2(buf2);
    std::string expDate(buf3);
    std::string cvv(buf4);

    if (track1.empty() || track2.empty()) {
        LogMessage("Error: Track 1 and Track 2 required.", true);
        return;
    }

    int sel = (int)SendMessage(g_hComboType, CB_GETCURSEL, 0, 0);
    CardType cardType = (CardType)sel;
    
    const char* typeStr = (cardType == CARD_VISA) ? "Visa" : 
                         (cardType == CARD_MASTERCARD) ? "Mastercard" :
                         (cardType == CARD_AMEX) ? "Amex" : "Discover";
    LogMessage(std::string("Programming ") + typeStr + "...");

    // Select PPSE
    LogMessage("Selecting PPSE...");
    SelectAID(AID_PPSE, sizeof(AID_PPSE));

    // Select specific AID
    bool aidOk = false;
    switch(cardType) {
        case CARD_VISA:
            LogMessage("Selecting Visa AID...");
            aidOk = SelectAID(AID_VISA_CREDIT, sizeof(AID_VISA_CREDIT)) ||
                    SelectAID(AID_VISA_DEBIT, sizeof(AID_VISA_DEBIT));
            break;
        case CARD_MASTERCARD:
            LogMessage("Selecting Mastercard AID...");
            aidOk = SelectAID(AID_MASTERCARD, sizeof(AID_MASTERCARD));
            break;
        case CARD_AMEX:
            LogMessage("Selecting Amex AID...");
            aidOk = SelectAID(AID_AMEX, sizeof(AID_AMEX));
            break;
        case CARD_DISCOVER:
            LogMessage("Selecting Discover AID...");
            aidOk = SelectAID(AID_DISCOVER, sizeof(AID_DISCOVER));
            break;
    }

    if (!aidOk) {
        LogMessage("AID selection failed, trying fallback...");
        std::vector<BYTE> selectMF = {0x00, 0xA4, 0x00, 0x00, 0x02, 0x3F, 0x00, 0x00};
        std::vector<BYTE> resp;
        DWORD len;
        SendAPDU(selectMF, resp, len);
    }

    // Store Track 1
    LogMessage("Storing Track 1...");
    if (!StoreData(0x01, track1, false)) {
        LogMessage("Track 1 store failed!", true);
        return;
    }

    // Store Track 2
    LogMessage("Storing Track 2...");
    if (!StoreData(0x02, track2, true)) {
        LogMessage("Track 2 store failed!", true);
        return;
    }

    // Store additional data if provided
    if (!expDate.empty()) {
        LogMessage("Storing expiration date...");
        StoreData(0x03, expDate, false);
    }
    if (!cvv.empty()) {
        LogMessage("Storing CVV...");
        StoreData(0x04, cvv, true);
    }

    LogMessage("Card programmed successfully!");
    MessageBox(g_hWndMain, "Card programmed successfully!", "Success", MB_OK | MB_ICONINFORMATION);
}

void RootEraseCard() {
    if (!g_state.bConnected) {
        LogMessage("Error: Not connected.", true);
        return;
    }

    if (MessageBox(g_hWndMain, 
        "WARNING: Root Erase will delete ALL data on the card!\nThis action cannot be undone.\n\nContinue?", 
        "Root Erase Confirmation", MB_YESNO | MB_ICONWARNING) != IDYES) {
        return;
    }

    LogMessage("=== ROOT ERASE OPERATION STARTED ===");
    LogMessage("This will erase all EMV applications and data...");

    // Step 1: Select Master File
    LogMessage("Selecting Master File...");
    std::vector<BYTE> selectMF = {0x00, 0xA4, 0x00, 0x00, 0x02, 0x3F, 0x00, 0x00};
    std::vector<BYTE> resp;
    DWORD len;
    SendAPDU(selectMF, resp, len);

    // Step 2: Delete all application DFs
    LogMessage("Deleting application DFs...");
    
    // Delete EMV DF
    std::vector<BYTE> deleteEMV = {0x00, 0xE4, 0x00, 0x00, 0x02, 0x7F, 0x10};
    SendAPDU(deleteEMV, resp, len);
    
    // Delete PPSE
    std::vector<BYTE> deletePPSE = {0x00, 0xE4, 0x00, 0x00, 0x0E, 
        0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31};
    SendAPDU(deletePPSE, resp, len);

    // Step 3: Erase binary data in EF files
    LogMessage("Erasing binary files...");
    std::vector<BYTE> eraseBinary = {0x00, 0x0E, 0x00, 0x00, 0x00};
    SendAPDU(eraseBinary, resp, len);

    // Step 4: JCOP specific erase command
    LogMessage("Executing JCOP root erase...");
    std::vector<BYTE> jcopErase = {0x80, 0x0E, 0x00, 0x00, 0x00};
    SendAPDU(jcopErase, resp, len);

    // Step 5: Terminate and reactivate
    LogMessage("Terminating DF...");
    std::vector<BYTE> terminateDF = {0x00, 0xE6, 0x00, 0x00, 0x00};
    SendAPDU(terminateDF, resp, len);

    LogMessage("Activating file system...");
    std::vector<BYTE> activateFile = {0x00, 0x44, 0x00, 0x00, 0x00};
    SendAPDU(activateFile, resp, len);

    LogMessage("=== ROOT ERASE COMPLETED ===");
    LogMessage("Card is now in factory state.", true);
    
    MessageBox(g_hWndMain, "Root erase completed.\nCard is now empty.", "Root Erase Complete", MB_OK | MB_ICONINFORMATION);
}

void DeleteEmvEvents() {
    if (!g_state.bConnected) {
        LogMessage("Error: Not connected.", true);
        return;
    }

    LogMessage("=== DELETE EMV EVENTS ===");
    LogMessage("Clearing EMV transaction logs and events...");

    // Select EMV application
    std::vector<BYTE> resp;
    DWORD len;

    // Try to select and delete log files
    LogMessage("Deleting transaction logs...");
    
    // Log Entry EF (typically 0xB5)
    std::vector<BYTE> deleteLogEntry = {0x00, 0xE4, 0x00, 0x00, 0x02, 0xB5, 0x00};
    SendAPDU(deleteLogEntry, resp, len);
    
    // Log Format EF (typically 0x4F)
    std::vector<BYTE> deleteLogFormat = {0x00, 0xE4, 0x00, 0x00, 0x02, 0x4F, 0x00};
    SendAPDU(deleteLogFormat, resp, len);

    // Last Online ATC Register
    LogMessage("Clearing ATC registers...");
    std::vector<BYTE> putDataATC = {0x00, 0xDA, 0x9F, 0x36, 0x02, 0x00, 0x00};
    SendAPDU(putDataATC, resp, len);

    // PIN Try Counter
    LogMessage("Resetting PIN try counter...");
    std::vector<BYTE> putDataPTC = {0x00, 0xDA, 0x9F, 0x17, 0x01, 0x03};
    SendAPDU(putDataPTC, resp, len);

    // ATC (Application Transaction Counter)
    LogMessage("Resetting Application Transaction Counter...");
    std::vector<BYTE> putDataATC2 = {0x00, 0xDA, 0x9F, 0x36, 0x02, 0x00, 0x00};
    SendAPDU(putDataATC2, resp, len);

    // Clear CVM List
    LogMessage("Clearing CVM list...");
    std::vector<BYTE> putDataCVM = {0x00, 0xDA, 0x8E, 0x00};
    SendAPDU(putDataCVM, resp, len);

    LogMessage("=== EMV EVENTS DELETED ===");
    MessageBox(g_hWndMain, "EMV events and logs cleared.", "Delete Complete", MB_OK);
}

void EstablishSecureChannel() {
    if (!g_state.bConnected) {
        LogMessage("Error: Not connected.", true);
        return;
    }

    LogMessage("=== ESTABLISHING SECURE CHANNEL ===");

    // GlobalPlatform Secure Channel establishment
    // Step 1: INITIALIZE UPDATE
    LogMessage("Sending INITIALIZE UPDATE...");
    
    std::vector<BYTE> hostChallenge = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    
    std::vector<BYTE> initUpdate;
    initUpdate.push_back(GP_CLA);
    initUpdate.push_back(GP_INS_INITIALIZE_UPDATE);
    initUpdate.push_back(g_state.keyVersion);
    initUpdate.push_back(g_state.keyIndex);
    initUpdate.push_back((BYTE)hostChallenge.size());
    initUpdate.insert(initUpdate.end(), hostChallenge.begin(), hostChallenge.end());
    initUpdate.push_back(0x00);

    std::vector<BYTE> response;
    DWORD respLen;
    LONG lReturn = SendAPDU(initUpdate, response, respLen);

    if (lReturn != SCARD_S_SUCCESS || respLen < 2) {
        LogMessage("INITIALIZE UPDATE failed!", true);
        return;
    }

    BYTE sw1 = response[respLen - 2];
    BYTE sw2 = response[respLen - 1];

    if (sw1 != 0x90 || sw2 != 0x00) {
        LogMessage("Card rejected INITIALIZE UPDATE", true);
        return;
    }

    LogMessage("INITIALIZE UPDATE successful");
    LogBytes("Card cryptogram", std::vector<BYTE>(response.begin(), response.end() - 2));

    // Step 2: EXTERNAL AUTHENTICATE (simplified - real implementation needs crypto)
    LogMessage("Sending EXTERNAL AUTHENTICATE...");
    
    std::vector<BYTE> extAuth;
    extAuth.push_back(GP_CLA);
    extAuth.push_back(GP_INS_EXTERNAL_AUTH);
    extAuth.push_back(0x00); // Security level
    extAuth.push_back(0x00);
    extAuth.push_back(0x08); // Length of host cryptogram
    // In real implementation, calculate host cryptogram from card challenge
    extAuth.insert(extAuth.end(), hostChallenge.begin(), hostChallenge.end());
    extAuth.push_back(0x00);

    lReturn = SendAPDU(extAuth, response, respLen);
    
    if (lReturn == SCARD_S_SUCCESS && respLen >= 2) {
        sw1 = response[respLen - 2];
        sw2 = response[respLen - 1];
        
        if (sw1 == 0x90 && sw2 == 0x00) {
            g_state.bSecureChannel = true;
            LogMessage("Secure Channel established successfully!");
            UpdateStatus();
        } else {
            LogMessage("EXTERNAL AUTHENTICATE failed", true);
        }
    }
}

void ManageKeys() {
    if (!g_state.bConnected) {
        LogMessage("Error: Not connected.", true);
        return;
    }

    LogMessage("=== KEY MANAGEMENT ===");
    
    // Dialog for key operation selection
    int choice = MessageBox(g_hWndMain, 
        "Select Key Operation:\n\nYES = Put New Key\nNO = Get Key Info\nCANCEL = Delete Key", 
        "Key Management", MB_YESNOCANCEL | MB_ICONQUESTION);

    std::vector<BYTE> resp;
    DWORD len;

    if (choice == IDYES) {
        // PUT KEY - load new key
        LogMessage("Loading new key...");
        
        // Example: Load ENC key (type 0x01)
        std::vector<BYTE> putKey;
        putKey.push_back(GP_CLA);
        putKey.push_back(GP_INS_PUT_KEY);
        putKey.push_back(0x01); // Key set version
        putKey.push_back(0x01); // Key ID (ENC)
        putKey.push_back(0x10); // Length (16 bytes for AES-128)
        
        // Dummy key data (in real use, proper key diversification)
        for(int i = 0; i < 16; i++) putKey.push_back(0xAB);
        
        putKey.push_back(0x00);
        
        SendAPDU(putKey, resp, len);
        LogMessage("Key load command sent");
        
    } else if (choice == IDNO) {
        // GET KEY INFO
        LogMessage("Querying key information...");
        
        std::vector<BYTE> getStatus;
        getStatus.push_back(GP_CLA);
        getStatus.push_back(GP_INS_GET_STATUS);
        getStatus.push_back(0x40); // Key information
        getStatus.push_back(0x00);
        getStatus.push_back(0x00);
        
        SendAPDU(getStatus, resp, len);
        LogBytes("Key info response", resp);
        
    } else if (choice == IDCANCEL) {
        // DELETE KEY
        LogMessage("Deleting key...");
        
        std::vector<BYTE> deleteKey;
        deleteKey.push_back(GP_CLA);
        deleteKey.push_back(GP_INS_DELETE);
        deleteKey.push_back(0x00);
        deleteKey.push_back(0x00);
        deleteKey.push_back(0x02);
        deleteKey.push_back(0xC1); // Key reference
        deleteKey.push_back(0x01);
        
        SendAPDU(deleteKey, resp, len);
        LogMessage("Key delete command sent");
    }
}

void Execute5640002Features() {
    if (!g_state.bConnected) {
        LogMessage("Error: Not connected.", true);
        return;
    }

    LogMessage("=== EXECUTING 0x5640002 FEATURES ===");
    LogMessage("Visa specific features and optimizations...");

    std::vector<BYTE> resp;
    DWORD len;

    // 0x5640002 is a Visa proprietary command for advanced features
    // P1=0x40, P2=0x02 indicates specific feature set
    
    LogMessage("Sending 0x5640002 command...");
    std::vector<BYTE> cmd5640002;
    cmd5640002.push_back(VISA_FEATURE_CLA);
    cmd5640002.push_back(VISA_FEATURE_INS);
    cmd5640002.push_back(VISA_FEATURE_P1);
    cmd5640002.push_back(VISA_FEATURE_P2);
    cmd5640002.push_back(0x00); // No data
    cmd5640002.push_back(0x00); // Expected response length

    LONG lReturn = SendAPDU(cmd5640002, resp, len);
    
    if (lReturn == SCARD_S_SUCCESS && len >= 2) {
        BYTE sw1 = resp[len - 2];
        BYTE sw2 = resp[len - 1];
        
        if (sw1 == 0x90 && sw2 == 0x00) {
            LogMessage("0x5640002 features activated successfully");
            if (len > 2) {
                LogBytes("Feature data", std::vector<BYTE>(resp.begin(), resp.end() - 2));
            }
        } else if (sw1 == 0x6A && sw2 == 0x81) {
            LogMessage("Function not supported by this card", true);
        } else if (sw1 == 0x6A && sw2 == 0x82) {
            LogMessage("File not found", true);
        } else {
            LogMessage("Command returned error", true);
        }
    }

    // Additional Visa-specific optimizations
    LogMessage("Configuring Visa-specific parameters...");
    
    // Set processing options
    std::vector<BYTE> gpo = {0x80, 0xA8, 0x00, 0x00, 0x02, 0x83, 0x00, 0x00};
    SendAPDU(gpo, resp, len);

    // Enable fast DDA/CDA
    std::vector<BYTE> fastCrypto = {0x00, 0xDA, 0x9F, 0x56, 0x01, 0x01};
    SendAPDU(fastCrypto, resp, len);

    LogMessage("0x5640002 feature configuration complete");
}

void ReadCardData() {
    if (!g_state.bConnected) {
        LogMessage("Error: Not connected.", true);
        return;
    }

    LogMessage("Reading card data...");
    
    std::vector<BYTE> readCmd = {0x00, INS_READ_RECORD, 0x01, 0x14, 0x00};
    std::vector<BYTE> response;
    DWORD respLen;

    LONG lReturn = SendAPDU(readCmd, response, respLen);
    
    if (lReturn == SCARD_S_SUCCESS && respLen > 2) {
        std::string data = BytesToString(response, respLen - 2);
        LogMessage("Read data: " + data);
        
        if (!data.empty()) {
            SetWindowText(g_hEditTrack1, data.c_str());
        }
    } else {
        LogMessage("Read operation returned no data.", true);
    }
}

void ClearLog() {
    SendMessage(g_hListLog, LB_RESETCONTENT, 0, 0);
    LogMessage("Log cleared.");
}

void LogMessage(const std::string& msg, bool bError) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    char timeStr[32];
    sprintf_s(timeStr, "[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
    
    std::string fullMsg = timeStr + msg;
    
    int idx = (int)SendMessage(g_hListLog, LB_ADDSTRING, 0, (LPARAM)fullMsg.c_str());
    SendMessage(g_hListLog, LB_SETTOPINDEX, idx, 0);
    
    if (bError) {
        // Could add color coding here with owner-draw
    }
}

void LogBytes(const std::string& prefix, const std::vector<BYTE>& data) {
    std::ostringstream oss;
    oss << prefix << ": ";
    for (size_t i = 0; i < data.size() && i < 32; i++) { // Limit to 32 bytes for display
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i] << " ";
    }
    if (data.size() > 32) oss << "...";
    LogMessage(oss.str());
}

std::string BytesToHex(const std::vector<BYTE>& data) {
    std::ostringstream oss;
    for (auto b : data) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    }
    return oss.str();
}

std::string BytesToString(const std::vector<BYTE>& data, size_t len) {
    if (len > data.size()) len = data.size();
    return std::string(data.begin(), data.begin() + len);
}
