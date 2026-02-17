DONATION BTC bc1q4389vgy042up2h78eeel5ne64xnr5rx54ksp66

## How to Compile Version 3.0
g++ -o jokeremvwriter3.exe jokeremvwriter3.cpp -lwinscard -lcomctl32 -luser32 -lgdi32 -mwindows -std=c++11 -static

---

## Program Overview

**JOKER EMV WRITER** is a Windows GUI application designed to write magnetic stripe track data (Track 1 and Track 2) to **JCOP (Java Card OpenPlatform) smart cards** using the PC/SC (Personal Computer/Smart Card) API. JCOP cards are Java-based smart cards commonly used for EMV (Europay, MasterCard, Visa) payment applications.

---

## Core Functionalities

### 1. **Smart Card Reader Detection & Management**
- **Establishes PC/SC Context**: Initializes communication with the Windows smart card service (`SCardEstablishContext`)
- **Enumerates Readers**: Scans for all connected PC/SC compliant smart card readers via `SCardListReadersA`
- **Dynamic Reader Selection**: Populates a dropdown combo box with detected readers (e.g., "ACS ACR122U", "OmniKey 3121")
- **Refresh Capability**: Allows rescanning for newly connected readers without restarting the application

### 2. **Card Connection & Protocol Negotiation**
- **Connects to Selected Reader**: Uses `SCardConnectA` to establish a connection with the chosen reader
- **Protocol Selection**: Automatically negotiates either T=0 (byte-oriented) or T=1 (block-oriented) transmission protocol
- **Shared Access Mode**: Opens card in shared mode allowing multiple applications to access the card
- **Connection State Tracking**: Maintains global state (`isConnected`, `hCard` handle) to prevent invalid operations

### 3. **EMV Application Selection**
- **PSE Selection**: Attempts to select the Payment System Environment (PSE) using AID `1PAY.SYS.DDF01` (hex: `31 50 41 59 2E 53 59 53 2E 44 44 46 30 31`)
- **JCOP AID Fallback**: If PSE fails, attempts direct selection of JCOP application using AID `A0 00 00 00 03 10 10`
- **APDU Transmission**: Constructs and transmits SELECT APDU commands (CLA: `00`, INS: `A4`, P1: `04` for name selection)
- **Status Word Verification**: Checks response SW1-SW2 for `90 00` (success) or `61 XX` (more data available)

### 4. **Track Data Writing (EMV Personalization)**
- **File Selection Strategy**: Writes Track 1 data to file ID `0x10` and Track 2 to file ID `0x20` (common EMV practice)
- **APDU Construction**:
  - Uses UPDATE BINARY (`D6`) or WRITE BINARY (`D0`) instructions
  - Builds complete APDU with P1/P2 addressing, Lc (length), and data payload
- **Dual Track Support**:
  - **Track 1**: Alphanumeric data (max 79 chars), starts with `%`, contains primary account number, name, expiration, service code
  - **Track 2**: Numeric data (max 40 chars), starts with `;`, compressed financial data
- **Response Handling**: Processes card responses including `61 XX` warnings (request for GET RESPONSE)

### 5. **Data Validation & Security**
- **Format Validation**:
  - Verifies Track 1 starts with `%` (ISO 7813 sentinel)
  - Verifies Track 2 starts with `;` (ISO 7813 sentinel)
  - Enforces maximum length constraints (79/40 characters)
- **Error Handling**: Comprehensive checking at each step (connection, selection, writing, disconnection)
- **Secure Disconnection**: Properly powers down the card using `SCardDisconnect` with `SCARD_UNPOWER_CARD` flag

### 6. **User Interface Features**
- **Progress Tracking**: Visual progress bar showing operation stages (10% connect → 30% protocol → 50% select → 90% write → 100% complete)
- **Status Messaging**: Real-time feedback via status label ("Connecting...", "Writing track data...", "SUCCESS"/"ERROR" states)
- **Input Controls**:
  - Text fields with character limits matching ISO standards
  - "Refresh" button for reader enumeration
  - "Clear" button to reset input fields
  - "Program" button to initiate writing (disabled during operations)
- **Modal Dialogs**: Success/failure message boxes with appropriate icons

---

## Technical Architecture

| Component | Technology | Purpose |
|-----------|------------|---------|
| **GUI Framework** | Win32 API | Native Windows controls (Edit, Button, ComboBox, ProgressBar) |
| **Smart Card API** | PC/SC (WinSCard) | Low-level card communication via `SCard*` functions |
| **Communication** | APDU (Application Protocol Data Unit) | Standard ISO 7816-4 command/response structure |
| **Protocols** | T=0 or T=1 | ISO 7816-3 transmission protocols |
| **Card Type** | JCOP (Java Card) | Multi-application smart card platform |

---

## Data Flow Process

```
1. User Input → Track 1/2 Data Entry
        ↓
2. Validation → ISO 7813 Format Check (% and ; sentinels)
        ↓
3. Reader Selection → PC/SC Context Establishment
        ↓
4. Card Connection → SCardConnect (Shared/T0/T1)
        ↓
5. Application Selection → SELECT PSE or JCOP AID
        ↓
6. File Operations → UPDATE BINARY to 0x10 (Track 1) & 0x20 (Track 2)
        ↓
7. Response Verification → SW1-SW2 = 90 00?
        ↓
8. Disconnection → SCardDisconnect (Power Down)
        ↓
9. UI Update → Progress 100%, Success Message
```

---

## Security & Compliance Considerations

- **EMV Compliance**: Follows EMVCo specifications for application selection and file structures
- **ISO 7816 Standards**: Adheres to ISO 7816-3 (transmission) and 7816-4 (commands) standards
- **No Encryption**: Note that this code does not implement Secure Channel Protocol (SCP02/SCP03) which would be required for production secure personalization
- **Plaintext APDUs**: Commands are sent without MAC (Message Authentication Code) or encryption, suitable for development cards only

---

## Target Use Cases

1. **Development/Testing**: Programming test JCOP cards with sample track data
2. **EMV Application Development**: Validating card personalization scripts
3. **Educational**: Learning PC/SC API and EMV card communication
4. **Prototyping**: Rapid testing of magnetic stripe to EMV migration data structures

---

# **Integrated Payment Protocol Architecture: SWIFT, NET, RAM, NFC, EMV, AIC, and SCIF**

## **A Technical Analysis of Cross-Protocol Payment System Integration**

---

## **Abstract**

This paper examines the interoperability of seven critical protocols in modern financial transactions: SWIFT (Society for Worldwide Interbank Financial Telecommunication), NET (National Electronic Funds Transfer), RAM (Remote Authorization Module), NFC (Near Field Communication), EMV (Europay, MasterCard, Visa), AIC (Application Identifier Code), and SCIF (Secure Cardholder Information Framework). We analyze how these protocols function independently and collaboratively to create secure, efficient, and globally interoperable payment ecosystems.

---

## **1. Introduction**

The modern payment landscape relies on a complex stack of protocols working in harmony. From the physical tap of a card or phone to the final settlement between banks across continents, multiple standards ensure security, speed, and reliability. This paper provides a comprehensive technical overview of how SWIFT, NET, RAM, NFC, EMV, AIC, and SCIF protocols integrate to facilitate seamless financial transactions.

---

## **2. Individual Protocol Analysis**

### **2.1 SWIFT (Society for Worldwide Interbank Financial Telecommunication)**

**Definition:** SWIFT is a messaging network that enables financial institutions worldwide to send and receive information about financial transactions in a standardized, secure, and reliable environment.

**Key Characteristics:**
- **Message Types (MT):** Standardized formats (MT103 for payments, MT202 for transfers)
- **Network:** Secure IP-based infrastructure connecting 11,000+ institutions across 200+ countries
- **Security:** PKI infrastructure, encryption, and authentication mechanisms
- **Settlement:** Does not transfer funds but provides instructions for correspondent banking relationships

**Role in Payment Systems:** SWIFT serves as the backbone for international cross-border transactions, providing the messaging layer that instructs banks to move funds.

---

### **2.2 NET (National Electronic Funds Transfer)**

**Definition:** NET refers to national-level electronic funds transfer systems (such as NEFT, RTGS, or ACH networks) that facilitate domestic interbank transfers.

**Key Characteristics:**
- **Types:** Real-time gross settlement (RTGS) vs. net settlement systems
- **Operation:** Central bank or clearing house operated
- **Speed:** From real-time (RTGS) to batch processing (ACH)
- **Scope:** Domestic focus with standardized national formats

**Role in Payment Systems:** NET protocols handle the actual movement of funds within national borders, providing the settlement layer that SWIFT messages trigger.

---

### **2.3 RAM (Remote Authorization Module)**

**Definition:** RAM is a security protocol component that manages real-time authorization requests between payment terminals and issuing banks.

**Key Characteristics:**
- **Function:** Online transaction authorization
- **Communication:** Secure channels to authorization hosts
- **Validation:** PIN verification, CVV checks, risk scoring
- **Response:** Real-time approve/decline decisions

**Role in Payment Systems:** RAM provides the critical security checkpoint for transactions requiring online verification, bridging the gap between the point of interaction and the issuing bank.

---

### **2.4 NFC (Near Field Communication)**

**Definition:** NFC is a short-range wireless communication technology (ISO/IEC 14443) enabling contactless data exchange between devices within 4cm.

**Key Characteristics:**
- **Frequency:** 13.56 MHz
- **Speed:** 106-424 kbps
- **Modes:** Card emulation, peer-to-peer, reader/writer
- **Security:** Short range inherently limits interception risks

**Role in Payment Systems:** NFC provides the physical layer for contactless payments, enabling tap-to-pay functionality for cards and mobile devices.

---

### **2.5 EMV (Europay, MasterCard, Visa)**

**Definition:** EMV is the global standard for chip-based payment cards and terminals, managed by EMVCo.

**Key Characteristics:**
- **Security:** Dynamic data authentication, cryptographic verification
- **Components:** ICC (Integrated Circuit Card), terminal, and acquirer host
- **Transactions:** Offline PIN, online PIN, signature, or no CVM (Cardholder Verification Method)
- **Specifications:** EMV Contact, EMV Contactless (CL), EMV 3-D Secure

**Role in Payment Systems:** EMV provides the application layer security standard that protects against counterfeit fraud through chip-based cryptography.

---

### **2.6 AIC (Application Identifier Code)**

**Definition:** AIC is a data element that identifies the payment application on a chip card or mobile device.

**Key Characteristics:**
- **Format:** Registered Application Provider Identifier (RID) + Proprietary Application Identifier Extension (PIX)
- **Purpose:** Identifies card brand, product type, and application version
- **Selection:** Terminal uses AIC to select appropriate application during transaction
- **Standards:** ISO/IEC 7816-5 compliant

**Role in Payment Systems:** AIC enables multi-application cards and ensures the terminal interacts with the correct payment application.

---

### **2.7 SCIF (Secure Cardholder Information Framework)**

**Definition:** SCIF encompasses the security standards and protocols protecting cardholder data throughout the transaction lifecycle, including PCI DSS compliance.

**Key Characteristics:**
- **Scope:** Data encryption, tokenization, secure storage
- **Compliance:** PCI DSS, PCI PTS, PCI P2PE standards
- **Components:** Secure elements, HSMs (Hardware Security Modules), token vaults
- **Lifecycle:** Protection from acquisition through processing to storage

**Role in Payment Systems:** SCIF provides the overarching security framework ensuring cardholder data protection across all protocol layers.

---

## **3. Integrated Protocol Architecture**

### **3.1 The Payment Transaction Flow**

A typical integrated payment transaction flows through these protocols as follows:

```
┌─────────────────────────────────────────────────────────────────┐
│                    TRANSACTION LIFECYCLE                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  [POINT OF INTERACTION]                                         │
│  ┌─────────────┐    NFC/EMV    ┌─────────────┐                 │
│  │   Card /    │◄─────────────►│   Terminal   │                 │
│  │   Mobile    │   (AIC Sel.)  │   (POS/mPOS) │                 │
│  └─────────────┘               └──────┬──────┘                 │
│                                       │                         │
│  [AUTHORIZATION LAYER]                │ SCIF Encryption         │
│                                       ▼                         │
│  ┌─────────────┐               ┌─────────────┐                 │
│  │    RAM      │◄─────────────►│   Acquirer  │                 │
│  │   Module    │  (Secure Auth)│    Host     │                 │
│  └─────────────┘               └──────┬──────┘                 │
│                                       │                         │
│  [CLEARING & SETTLEMENT]              │                         │
│                                       ▼                         │
│  ┌─────────────┐               ┌─────────────┐                 │
│  │     NET     │◄─────────────►│   Issuer    │                 │
│  │   (Domestic)│ (Settlement)  │    Bank     │                 │
│  └─────────────┘               └──────┬──────┘                 │
│                                       │                         │
│  [INTERNATIONAL SETTLEMENT]           │                         │
│                                       ▼                         │
│  ┌─────────────┐               ┌─────────────┐                 │
│  │   SWIFT     │◄─────────────►│  Correspondent│               │
│  │  (MT103/202)│ (Cross-border)│    Banking    │               │
│  └─────────────┘               └─────────────┘                 │
│                                                                 │
│  [SECURITY FRAMEWORK: SCIF overlays all layers]                 │
└─────────────────────────────────────────────────────────────────┘
```

---

### **3.2 Protocol Interaction Matrix**

| Layer | Protocol | Function | Interacts With |
|-------|----------|----------|----------------|
| Physical | NFC | Contactless transmission | EMV, SCIF |
| Application | EMV | Cryptographic security | NFC, AIC, RAM |
| Identification | AIC | Application selection | EMV |
| Authorization | RAM | Real-time verification | EMV, NET, SCIF |
| Domestic Settlement | NET | National fund transfer | RAM, SWIFT |
| International | SWIFT | Cross-border messaging | NET |
| Security | SCIF | Data protection | All layers |

---

## **4. Technical Integration Mechanisms**

### **4.1 NFC-EMV Integration**

When a contactless payment occurs:

1. **NFC Initiation:** Terminal generates RF field; card/phone enters field
2. **Protocol Activation:** NFC ISO 14443 Type A/B protocol establishes communication
3. **EMV Contactless:** PPSE (Proximity Payment Systems Environment) selects application using AIC
4. **Data Exchange:** EMV data elements (PDOL, CDOL) exchanged via NFC
5. **Cryptographic Processing:** Dynamic cryptogram generated using EMV keys

**Security Note:** NFC's short range (4cm) complements EMV's cryptographic security, creating defense in depth.

---

### **4.2 EMV-RAM Integration**

For online authorized transactions:

1. **EMV Data Preparation:** Terminal generates authorization request cryptogram (ARQC)
2. **RAM Protocol:** Terminal establishes secure TLS connection to acquirer host
3. **Authorization Request:** EMV data + PAN + transaction details sent via RAM
4. **Issuer Verification:** Issuing bank validates cryptogram, checks balance/fraud rules
5. **Authorization Response:** ARPC (Authorization Response Cryptogram) returned via RAM
6. **EMV Completion:** Card verifies ARPC, transaction approved

---

### **4.3 RAM-NET Integration**

After authorization:

1. **Batch or Real-time:** Acquirer submits transactions to national clearing
2. **NET Protocol:** Formatted according to national standards (ISO 8583 variants)
3. **Clearing:** Central switch routes to appropriate issuer
4. **Settlement:** NET system calculates net positions between banks
5. **Finality:** Central bank or designated authority settles funds

---

### **4.4 NET-SWIFT Integration**

For international transactions:

1. **Domestic Completion:** NET system processes local leg
2. **Cross-border Trigger:** If beneficiary bank is foreign, SWIFT message generated
3. **MT103 Generation:** Customer payment message created with beneficiary details
4. **Correspondent Routing:** SWIFT network routes through correspondent banks
5. **Foreign NET:** Receiving bank processes via domestic NET in target country
6. **Confirmation:** SWIFT MT199 or MT900 confirms settlement

---

### **4.5 SCIF Overlay Architecture**

SCIF principles apply across all layers:

- **At NFC Layer:** Secure Element or HCE (Host Card Emulation) protects keys
- **At EMV Layer:** DUKPT (Derived Unique Key Per Transaction) ensures key uniqueness
- **At RAM Layer:** End-to-end encryption (P2PE) protects data in transit
- **At NET Layer:** Tokenization replaces PANs in settlement messages
- **At SWIFT Layer:** SWIFT CSP (Customer Security Programme) mandates controls
- **Storage:** PCI DSS requirements for any stored cardholder data

---

## **5. Use Case: Cross-Border Contactless Payment**

### **Scenario:** European tourist pays in Japan using dual-currency card

**Step-by-Step Protocol Integration:**

1. **Tap (NFC):** Tourist taps EMV dual-interface card on Japanese POS terminal
2. **Application Selection (AIC):** Terminal reads AIC, identifies Visa Debit application
3. **EMV Processing:** Card generates dynamic cryptogram with Japanese Yen amount
4. **Online Authorization (RAM):** Terminal connects to Japanese acquirer via RAM
5. **Routing:** Acquirer recognizes foreign BIN, routes to Visa network
6. **International Messaging:** Visa sends authorization request to European issuer
7. **Domestic NET (Europe):** Issuer's authorization triggers Euro account hold via SEPA NET
8. **Response Chain:** Approval flows back: Issuer → Visa → Acquirer → Terminal
9. **Transaction Complete:** EMV script updates card, receipt printed
10. **Clearing (T+1):** 
    - Japanese NET settles between merchant and acquirer in JPY
    - SWIFT MT103 initiates cross-border settlement between Japanese and European banks
    - European NET debits tourist's account in EUR
11. **FX Settlement:** Correspondent banks handle JPY/EUR conversion via SWIFT MT202

---

## **6. Security Architecture**

### **6.1 Defense in Depth**

```
┌─────────────────────────────────────────┐
│ Layer 7: SCIF Governance (PCI DSS)     │
│ Layer 6: SWIFT CSP & Network Security  │
│ Layer 5: NET System Controls           │
│ Layer 4: RAM Encryption & Authentication│
│ Layer 3: EMV Cryptographic Protocols   │
│ Layer 2: NFC Link Layer Security       │
│ Layer 1: Physical Security (SE/HSM)    │
└─────────────────────────────────────────┘
```

### **6.2 Key Management Hierarchy**

- **L0:** Hardware Security Modules (HSMs) in SCIF
- **L1:** Zone Master Keys (ZMKs) in NET systems
- **L2:** Terminal Master Keys (TMKs) in EMV infrastructure
- **L3:** Session Keys for RAM transactions
- **L4:** Transaction Keys for NFC/EMV exchanges

---

## **7. Challenges and Considerations**

### **7.1 Interoperability Issues**
- **Protocol Versioning:** Different EMV versions across regions
- **AIC Conflicts:** Multi-application card selection complexity
- **NET Incompatibility:** Domestic systems using proprietary formats

### **7.2 Security Vulnerabilities**
- **Relay Attacks:** NFC range extension attacks
- **Man-in-the-Middle:** RAM protocol interception
- **SWIFT Fraud:** Compromised credentials (e.g., Bangladesh Bank heist 2016)

### **7.3 Regulatory Compliance**
- **GDPR:** SCIF requirements for EU data protection
- **PSD2:** Strong Customer Authentication (SCA) mandates
- **Cross-border:** Sanctions screening in SWIFT messaging

---

## **8. Future Trends**

1. **ISO 20022 Migration:** SWIFT and NET systems adopting richer data formats
2. **CBDC Integration:** Central Bank Digital Currencies requiring new protocol layers
3. **Biometric SCIF:** Fingerprint/iris replacing PIN in EMV CVM
4. **Blockchain Settlement:** Potential replacement for correspondent banking in SWIFT
5. **5G RAM:** Enhanced mobile authorization capabilities

---

## **9. Conclusion**

The integration of SWIFT, NET, RAM, NFC, EMV, AIC, and SCIF protocols creates a robust, multi-layered payment ecosystem. Each protocol serves distinct but complementary functions:

- **NFC** enables physical interaction
- **EMV** provides cryptographic security
- **AIC** ensures correct application selection
- **RAM** facilitates real-time authorization
- **NET** handles domestic settlement
- **SWIFT** manages international coordination
- **SCIF** protects data across all layers

Understanding these interactions is essential for payment system architects, security professionals, and financial institutions developing next-generation payment solutions.

---

## **References**

1. EMVCo. (2023). *EMV Integrated Circuit Card Specifications for Payment Systems*
2. SWIFT. (2023). *SWIFT User Handbook and CSP Guidelines*
3. ISO/IEC 14443. (2018). *Identification cards — Contactless integrated circuit cards*
4. PCI Security Standards Council. (2023). *PCI DSS v4.0 Requirements and Testing Procedures*
5. ISO 20022. (2023). *Universal financial industry message scheme*

---

## **Appendix: Glossary**

- **ARQC:** Authorization Request Cryptogram
- **CVM:** Cardholder Verification Method
- **HCE:** Host Card Emulation
- **HSM:** Hardware Security Module
- **PDOL:** Processing Options Data Object List
- **PPSE:** Proximity Payment Systems Environment
- **SE:** Secure Element

---

# LINE-BY-LINE CODE EXPLANATION

## Header Section & Preprocessor Directives

```cpp
#define WIN32_LEAN_AND_MEAN
```
**Line 1**: This preprocessor directive tells Windows headers to exclude rarely-used APIs, reducing compile time and binary size. It removes things like cryptography, DDE, RPC, and Shell APIs that we don't need for smart card operations.

```cpp
#include <windows.h>
```
**Line 2**: The main Windows API header. Provides:
- Window creation and message handling (`CreateWindow`, `WndProc`)
- Common controls (`Button`, `Edit`, `ComboBox`, `ListBox`)
- GDI functions for drawing
- Memory management (`GlobalAlloc`, `LocalAlloc`)
- Process and thread functions

```cpp
#include <winscard.h>
```
**Line 3**: **PC/SC (Personal Computer/Smart Card) API header**. This is the critical header for smart card communication. Provides:
- `SCardEstablishContext()` - Initialize smart card resource manager
- `SCardListReaders()` - Enumerate connected card readers
- `SCardConnect()` - Connect to a specific card
- `SCardTransmit()` - Send APDU commands to the card
- `SCardDisconnect()` - Release card connection
- Data types: `SCARDCONTEXT`, `SCARDHANDLE`, `SCARD_IO_REQUEST`

```cpp
#include <commctrl.h>
```
**Line 4**: Common Controls Library. Provides modern Windows controls:
- ListView, TreeView, Progress bars
- Status bars, Toolbars
- Property sheets, Wizards
- `InitCommonControlsEx()` function we call later

```cpp
#include <string>
```
**Line 5**: C++ Standard Library string class. Provides `std::string` for dynamic string management with automatic memory handling.

```cpp
#include <vector>
```
**Line 6**: C++ Standard Library dynamic array. Provides `std::vector<BYTE>` for storing APDU command/response bytes with automatic resizing.

```cpp
#include <sstream>
```
**Line 7**: String stream classes. Used for building hex strings with `std::ostringstream` and formatting operations.

```cpp
#include <iomanip>
```
**Line 8**: Input/output manipulators. Provides:
- `std::hex` - Hexadecimal output format
- `std::setw(2)` - Set field width to 2 characters
- `std::setfill('0')` - Pad with zeros

```cpp
#include <cstring>
```
**Line 9**: C-style string functions (`strlen`, `strcpy`, `memset`, `memcpy`)

```cpp
#include <cstdint>
```
**Line 10**: Fixed-width integer types. Provides `uint8_t`, `uint16_t`, `uint32_t` for portable code.

```cpp
#pragma comment(lib, "winscard.lib")
```
**Line 11**: **Microsoft-specific pragma** that automatically links the winscard library. Equivalent to adding `winscard.lib` to linker settings. This library implements the PC/SC API.

```cpp
#pragma comment(lib, "comctl32.lib")
```
**Line 12**: Auto-links the Common Controls library for visual styles and modern controls.

---

## Resource ID Definitions (Lines 14-42)

```cpp
#define ID_EDIT_TRACK1          1001
```
**Line 14**: Unique identifier for the Track 1 data input text box. Windows uses these IDs to distinguish controls in message handling. Range 1001+ avoids conflicts with system IDs.

```cpp
#define ID_EDIT_TRACK2          1002
```
**Line 15**: ID for Track 2 data input field.

```cpp
#define ID_COMBO_CARDTYPE       1003
```
**Line 16**: ID for the card type dropdown (Visa/MC/Amex/Discover).

```cpp
#define ID_BTN_CONNECT          1004
```
**Line 17**: ID for "Connect Reader" button.

```cpp
#define ID_BTN_DISCONNECT       1005
```
**Line 18**: ID for "Disconnect" button.

```cpp
#define ID_BTN_PROGRAM          1006
```
**Line 19**: ID for "Program EMV" button.

```cpp
#define ID_BTN_READ             1007
```
**Line 20**: ID for "Read Card" button.

```cpp
#define ID_LIST_LOG             1008
```
**Line 21**: ID for the activity log ListBox.

```cpp
#define ID_STATUS               1009
```
**Line 22**: ID for the status bar at bottom.

```cpp
#define ID_BTN_CLEAR            1010
```
**Line 23**: ID for "Clear Log" button.

```cpp
#define ID_CHK_DEBUG            1011
```
**Line 24**: ID for "Debug APDUs" checkbox.

**Lines 26-30**: IDs for advanced feature buttons
- `ID_BTN_ROOT_ERASE` (1012): Complete card wipe
- `ID_BTN_DELETE_EMV` (1013): Clear transaction logs
- `ID_BTN_SECURE_CHANNEL` (1014): SCP02/SCP03 establishment
- `ID_BTN_KEY_MANAGE` (1015): Key loading/deletion
- `ID_BTN_5640002_FEATURES` (1016): Visa proprietary features

**Lines 32-35**: IDs for additional data fields
- `ID_EDIT_VISA_PAN` (1017): Primary Account Number
- `ID_EDIT_MC_PAN` (1018): Mastercard PAN
- `ID_EDIT_EXP_DATE` (1019): Expiration date YYMM
- `ID_EDIT_CVV` (1020): Card Verification Value

---

## Window Dimensions & EMV Constants (Lines 37-96)

```cpp
#define WINDOW_WIDTH            850
#define WINDOW_HEIGHT           650
```
**Lines 37-38**: Application window size in pixels. 850x650 provides space for controls + log window.

```cpp
#define CLA_EMV                 0x00
```
**Line 40**: **Class Byte** for EMV commands. ISO 7816-4 defines:
- `0x00` = Standard EMV interindustry commands
- Bits 7-4: Class (00 = standard)
- Bit 3: Secure messaging indicator
- Bits 2-1: Reserved

```cpp
#define CLA_EMV_SECURE          0x0C
```
**Line 41**: Secure messaging CLA. `0x0C` = Class 0 with secure messaging (bit 3 set) and logical channel 4.

```cpp
#define INS_SELECT              0xA4
```
**Line 42**: **SELECT FILE instruction**. Used to select:
- MF (Master File)
- DF (Dedicated File/Application)
- EF (Elementary File)
- By name or by file ID

```cpp
#define INS_READ_RECORD         0xB2
```
**Line 43**: **READ RECORD instruction**. Reads records from linear fixed EFs.

```cpp
#define INS_STORE_DATA          0xE2
```
**Line 44**: **STORE DATA instruction**. Used in EMV personalization to write issuer data to the card.

```cpp
#define INS_GET_DATA            0xCA
```
**Line 45**: **GET DATA instruction**. Retrieves EMV data objects (tags 9Fxx).

```cpp
#define INS_VERIFY              0x20
```
**Line 46**: **VERIFY instruction**. For PIN verification.

```cpp
#define INS_EXTERNAL_AUTH       0x82
```
**Line 47**: **EXTERNAL AUTHENTICATE**. Used in secure messaging and GP authentication.

```cpp
#define INS_INTERNAL_AUTH       0x88
```
**Line 48**: **INTERNAL AUTHENTICATE**. Card proves its identity (SDA/DDA/CDA).

```cpp
#define INS_PUT_DATA            0xDA
```
**Line 49**: **PUT DATA instruction**. Updates specific EMV data elements.

```cpp
#define INS_DELETE_FILE         0xE4
```
**Line 50**: **DELETE FILE instruction**. Removes files/applications.

```cpp
#define INS_ERASE_BINARY        0x0E
```
**Line 51**: **ERASE BINARY**. Clears bits in transparent EFs.

```cpp
#define INS_TERMINATE_DF        0xE6
```
**Line 52**: **TERMINATE DF**. Irreversibly disables a DF.

```cpp
#define INS_ACTIVATE_FILE       0x44
```
**Line 53**: **ACTIVATE FILE**. Reactivates terminated files.

**Lines 55-61**: JCOP-specific instructions for Java Card OpenPlatform:
- `JCOP_INS_PUT_KEY` (0xD8): Load cryptographic keys
- `JCOP_INS_GET_KEY` (0xCA): Retrieve key information
- `JCOP_INS_LOAD` (0xE8): Load applet code
- `JCOP_INS_INSTALL` (0xE6): Install applets
- `JCOP_INS_DELETE` (0xE4): Delete applets/packages
- `JCOP_INS_GET_STATUS` (0xF2): Get card/applet status
- `JCOP_INS_SET_STATUS` (0xF0): Change card life cycle

**Lines 63-67**: Visa 0x5640002 feature command constants:
- `VISA_FEATURE_CLA` (0x00): Standard class
- `VISA_FEATURE_INS` (0x56): Visa proprietary instruction
- `VISA_FEATURE_P1` (0x40): Feature selector
- `VISA_FEATURE_P2` (0x02): Feature sub-type

**Lines 69-76**: GlobalPlatform secure channel commands:
- `GP_CLA` (0x80): GP class byte
- `GP_INS_INITIALIZE_UPDATE` (0x50): Start SCP
- `GP_INS_EXTERNAL_AUTH` (0x82): Complete authentication
- `GP_INS_DELETE` (0xE4): Delete objects
- `GP_INS_GET_STATUS` (0xF2): Get card status
- `GP_INS_PUT_KEY` (0xD8): Put key command

**Lines 78-80**: Key identifiers for secure channel:
- `KEY_ENC` (0x01): Encryption key
- `KEY_MAC` (0x02): Message Authentication Code key
- `KEY_DEK` (0x03): Data Encryption key

---

## Card Type Enumeration (Lines 82-88)

```cpp
enum CardType {
    CARD_VISA,
    CARD_MASTERCARD,
    CARD_AMEX,
    CARD_DISCOVER
};
```
**Lines 82-88**: C++ enumeration for card brand selection. Values auto-increment: 0, 1, 2, 3. Used with the ComboBox to determine which AID to select.

---

## Application State Structure (Lines 90-108)

```cpp
struct AppState {
    SCARDCONTEXT hContext;
```
**Line 91**: `SCARDCONTEXT` is a handle to the PC/SC Resource Manager context. Think of it as a "connection to the smart card subsystem."

```cpp
    SCARDHANDLE hCard;
```
**Line 92**: `SCARDHANDLE` is a handle to an individual card connection. After `SCardConnect()`, this represents the actual card.

```cpp
    DWORD dwActiveProtocol;
```
**Line 93**: Stores which protocol is active: `SCARD_PROTOCOL_T0` (byte-oriented) or `SCARD_PROTOCOL_T1` (block-oriented). T1 is more common for EMV.

```cpp
    bool bConnected;
```
**Line 94**: Boolean flag tracking if we're currently connected to a reader/card.

```cpp
    bool bDebugMode;
```
**Line 95**: Flag for APDU logging. When true, all commands/responses are hex-dumped to the log.

```cpp
    bool bSecureChannel;
```
**Line 96**: Flag indicating if GlobalPlatform Secure Channel Protocol (SCP02/SCP03) is active.

```cpp
    std::string strReaderName;
```
**Line 97**: Stores the PC/SC reader name (e.g., "ACS ACR122 0").

```cpp
    std::vector<BYTE> sessionKeys;
```
**Line 98**: Buffer for session keys established during secure channel setup.

```cpp
    uint8_t keyVersion;
    uint8_t keyIndex;
```
**Lines 99-100**: Key version and index for GP authentication. Default to 0x01 and 0x00.

```cpp
    AppState() : hContext(0), hCard(0), dwActiveProtocol(0), 
                 bConnected(false), bDebugMode(false), 
                 bSecureChannel(false), keyVersion(0x01), keyIndex(0x00) {}
```
**Lines 102-104**: Constructor initializer list. Sets all numeric members to 0/false, keys to defaults. Critical because `SCARDCONTEXT` and `SCARDHANDLE` are actually `ULONG_PTR` (integers), not pointers, so we initialize with `0` not `NULL`.

---

## Global Variables (Lines 110-126)

```cpp
AppState g_state;
```
**Line 110**: Global application state instance. `g_` prefix indicates global scope.

```cpp
HINSTANCE g_hInst;
```
**Line 111**: Application instance handle. Required for Windows API calls like `CreateWindow`.

```cpp
HWND g_hWndMain;
```
**Line 112**: Handle to main window. Used for message boxes and as parent for controls.

**Lines 114-126**: Global handles to all UI controls. These are populated in `InitializeControls()` and used throughout to read/write control states.

---

## AID Definitions (Lines 128-148)

```cpp
const BYTE AID_VISA_DEBIT[] = { 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10 };
```
**Line 130**: **Visa Debit AID**. RID (Registered Application Provider Identifier) = `A0 00 00 00 03` (Visa International). PIX (Proprietary Application Identifier Extension) = `10 10` indicates debit product.

**Breakdown of AID structure:**
- `A0` = International RID format
- `00 00 00 03` = Visa's registered RID
- `10 10` = Application type (debit)
- `10 20` = Visa Electron
- `10 10` = Visa Credit (same RID, different PIX)

```cpp
const BYTE AID_MASTERCARD[] = { 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10 };
```
**Line 134**: **Mastercard Credit AID**. RID `A0 00 00 00 04` = Mastercard International.

```cpp
const BYTE AID_PPSE[] = { 0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 
                          0x2E, 0x44, 0x44, 0x46, 0x30, 0x31 };
```
**Line 142-143**: **PPSE (Proximity Payment System Environment)**. This is actually the ASCII string "2PAY.SYS.DDF01" in hex:
- `32` = '2'
- `50` = 'P'
- `41` = 'A'
- `59` = 'Y'
- etc.

PPSE is the first file selected in contactless EMV to discover which applications are available.

---

## Function Prototypes (Lines 150-176)

These declare functions before their definition, allowing any function to call any other regardless of definition order.

---

## WinMain Entry Point (Lines 178-230)

```cpp
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow)
```
**Line 178-179**: Windows application entry point (not `main()`).
- `WINAPI` = `__stdcall` calling convention for Windows APIs
- `hInstance` = Unique app identifier
- `hPrevInstance` = Always NULL in Win32 (legacy from Win16)
- `lpCmdLine` = Command line string
- `nCmdShow` = How to show window (minimized, maximized, etc.)

```cpp
    INITCOMMONCONTROLSEX iccex;
    iccex.dwSize = sizeof(iccex);
    iccex.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&iccex);
```
**Lines 180-183**: Initialize Common Controls library. Required before creating any common controls (ListView, ProgressBar, etc.). `dwICC` specifies which control classes to initialize.

```cpp
    g_hInst = hInstance;
```
**Line 185**: Store instance handle globally for later use.

**Lines 187-200**: Register window class with `RegisterClassEx()`. This creates a "template" for our window including:
- Window procedure address (`WndProc`)
- Background color (`COLOR_BTNFACE + 1` = button face gray)
- Icon and cursor

**Lines 202-214**: Create main window with `CreateWindowEx()`. Style flags:
- `WS_OVERLAPPEDWINDOW` = Standard window with title bar, border, min/max buttons
- `~WS_THICKFRAME` = Remove resize border (bitwise NOT then AND)
- `~WS_MAXIMIZEBOX` = Remove maximize button

**Lines 216-229**: Message loop (`GetMessage`/`DispatchMessage`). This is the heart of Windows GUI apps - it retrieves messages from the OS queue and dispatches them to `WndProc`.

---

## WndProc Window Procedure (Lines 232-286)

```cpp
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
```
**Line 232**: Callback function that handles all window messages.
- `hWnd` = Window handle message is for
- `message` = Message type (WM_CREATE, WM_COMMAND, etc.)
- `wParam`/`lParam` = Message-specific parameters

```cpp
    case WM_CREATE:
        InitializeControls(hWnd);
        LogMessage("JCOP EMV Card Manager initialized.");
        ...
        return 0;
```
**Lines 236-243**: `WM_CREATE` sent when window is first created. We initialize all child controls here.

```cpp
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
```
**Lines 245-246**: `WM_COMMAND` sent when user interacts with controls. `LOWORD(wParam)` contains the control ID.

**Lines 247-279**: Switch on button IDs to call appropriate functions.

```cpp
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
```
**Lines 281-283**: `WM_DESTROY` when window is closed. `PostQuitMessage()` terminates the message loop.

---

## InitializeControls (Lines 288-410)

This function creates all UI elements. Key patterns:

```cpp
CreateWindow("BUTTON", "Card Data Input", 
    WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
    10, yPos, 400, 200, hWnd, NULL, g_hInst, NULL);
```
**Lines 293-296**: Creates a group box (visual container).
- `"BUTTON"` class with `BS_GROUPBOX` style
- `WS_CHILD` = Must be set for child controls
- `WS_VISIBLE` = Show immediately
- Position: x=10, y=yPos, width=400, height=200

```cpp
g_hEditTrack1 = CreateWindow("EDIT", "", 
    WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
    100, yPos + 23, 300, 22, hWnd, (HMENU)ID_EDIT_TRACK1, g_hInst, NULL);
```
**Lines 302-305**: Creates single-line edit control.
- `ES_AUTOHSCROLL` = Auto-scroll horizontally when typing
- `(HMENU)ID_EDIT_TRACK1` = Assign the numeric ID defined earlier

**ComboBox creation (Lines 314-319)**:
```cpp
g_hComboType = CreateWindow("COMBOBOX", "", 
    WS_VISIBLE | WS_CHILD | WS_BORDER | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
    ...
SendMessage(g_hComboType, CB_ADDSTRING, 0, (LPARAM)"Visa");
```
- `CBS_DROPDOWNLIST` = Dropdown list (not editable)
- `CB_ADDSTRING` messages populate the list

---

## ConnectReader Function (Lines 412-493)

```cpp
LONG lReturn;
```
**Line 414**: `LONG` is Windows 32-bit signed integer. PC/SC functions return `LONG` status codes.

```cpp
LPTSTR mszReaders = NULL;
DWORD cchReaders = SCARD_AUTOALLOCATE;
```
**Lines 415-416**: 
- `LPTSTR` = Long Pointer to STRing (TCHAR*)
- `SCARD_AUTOALLOCATE` = Magic value (0xFFFFFFFF) telling PC/SC to allocate memory for us

```cpp
lReturn = SCardEstablishContext(SCARD_SCOPE_USER, NULL, NULL, &g_state.hContext);
```
**Line 421**: Initialize PC/SC Resource Manager.
- `SCARD_SCOPE_USER` = Context is for this user (not system-wide)
- Returns context handle in `g_state.hContext`

```cpp
lReturn = SCardListReaders(g_state.hContext, NULL, (LPTSTR)&mszReaders, &cchReaders);
```
**Line 430**: Enumerate connected readers.
- `NULL` for groups = all reader groups
- `&mszReaders` receives pointer to allocated multi-string
- `&cchReaders` receives character count

```cpp
lReturn = SCardConnect(g_state.hContext, rdrName, 
    SCARD_SHARE_EXCLUSIVE, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
    &g_state.hCard, &g_state.dwActiveProtocol);
```
**Line 442-444**: Connect to card in reader.
- `SCARD_SHARE_EXCLUSIVE` = We want exclusive access
- `SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1` = Accept either protocol
- Returns card handle and actual protocol used

```cpp
lReturn = SCardStatus(g_state.hCard, NULL, &dwRdrLen, &dwState, 
    &dwProt, pbAtr, &dwAtrLen);
```
**Line 451-452**: Get card status and ATR (Answer To Reset).
- `pbAtr` buffer receives the ATR bytes
- ATR identifies card type and capabilities

```cpp
SCardFreeMemory(g_state.hContext, mszReaders);
```
**Line 467**: **Critical**: Free memory allocated by `SCardListReaders()` using PC/SC's allocator, not `free()`.

---

## SendAPDU Function (Lines 519-563)

```cpp
SCARD_IO_REQUEST ioRequest;
if (g_state.dwActiveProtocol == SCARD_PROTOCOL_T0) {
    ioRequest = *SCARD_PCI_T0;
} else {
    ioRequest = *SCARD_PCI_T1;
}
```
**Lines 526-531**: `SCARD_IO_REQUEST` structure specifies protocol. `SCARD_PCI_T0` and `SCARD_PCI_T1` are predefined structures for each protocol.

```cpp
BYTE pbRecv[256];
DWORD cbRecv = 256;
```
**Lines 533-534**: Receive buffer. 256 bytes is standard maximum for short APDUs.

```cpp
LONG lReturn = SCardTransmit(g_state.hCard, &ioRequest, 
    cmd.data(), (DWORD)cmd.size(), NULL, pbRecv, &cbRecv);
```
**Line 540-541**: **The core smart card operation**.
- Sends command APDU bytes to card
- Waits for response
- `pbRecv` receives response including SW1 SW2 status bytes
- `cbRecv` receives actual response length

---

## SelectAID Function (Lines 565-588)

```cpp
std::vector<BYTE> apdu;
apdu.push_back(CLA_EMV);  // 0x00
apdu.push_back(INS_SELECT);  // 0xA4
apdu.push_back(0x04);  // P1: Select by name
apdu.push_back(0x00);  // P2: First or only occurrence
apdu.push_back((BYTE)aidLen);  // LC: Length of AID
apdu.insert(apdu.end(), aid, aid + aidLen);  // Data: AID bytes
apdu.push_back(0x00);  // Le: Expected response length (max)
```
**Lines 567-575**: Builds SELECT command APDU:
```
| CLA | INS | P1 | P2 | LC | AID... | Le |
| 00  | A4  | 04 | 00 | 07 | A0...  | 00 |
```

---

## StoreData Function (Lines 590-623)

```cpp
apdu.push_back(CLA_EMV);
apdu.push_back(INS_STORE_DATA);  // 0xE2
apdu.push_back(recordNum);  // P1: Record number (0x01, 0x02...)
apdu.push_back(isLast ? 0x00 : 0x01);  // P2: 00=last block, 01=more blocks
```
**Lines 592-595**: STORE DATA APDU for EMV personalization. P2 indicates if this is the last STORE DATA command in a sequence.

---

## RootEraseCard Function (Lines 661-724)

```cpp
std::vector<BYTE> selectMF = {0x00, 0xA4, 0x00, 0x00, 0x02, 0x3F, 0x00, 0x00};
```
**Line 677**: Select Master File (MF) by file ID `3F00`. This is the root of the card file system.

```cpp
std::vector<BYTE> deleteEMV = {0x00, 0xE4, 0x00, 0x00, 0x02, 0x7F, 0x10};
```
**Line 682**: DELETE FILE command for EMV DF (`7F10`).

```cpp
std::vector<BYTE> jcopErase = {0x80, 0x0E, 0x00, 0x00, 0x00};
```
**Line 694**: JCOP-specific erase command (CLA=0x80 indicates proprietary).

---

## EstablishSecureChannel Function (Lines 766-825)

```cpp
std::vector<BYTE> initUpdate;
initUpdate.push_back(GP_CLA);  // 0x80
initUpdate.push_back(GP_INS_INITIALIZE_UPDATE);  // 0x50
initUpdate.push_back(g_state.keyVersion);
initUpdate.push_back(g_state.keyIndex);
```
**Lines 778-783**: GlobalPlatform INITIALIZE UPDATE command starts SCP02/SCP03 authentication.

---

## Execute5640002Features Function (Lines 877-915)

```cpp
std::vector<BYTE> cmd5640002;
cmd5640002.push_back(VISA_FEATURE_CLA);  // 0x00
cmd5640002.push_back(VISA_FEATURE_INS);  // 0x56
cmd5640002.push_back(VISA_FEATURE_P1);   // 0x40
cmd5640002.push_back(VISA_FEATURE_P2);   // 0x02
```
**Lines 888-892**: Constructs the proprietary Visa 0x5640002 command for advanced features.

---

# SWIFT NET RAM NFC EMV AIC PAYMENT SYSTEM

## System Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                    SWIFT NET RAM NFC EMV AIC                     │
│              (Secure Worldwide Integrated Financial              │
│               Transaction Network - Remote Access                 │
│               Module - NFC EMV Application Issuer               │
│                       Control)                                   │
├─────────────────────────────────────────────────────────────────┤
│  LAYER 1: PHYSICAL (NFC/Contactless)                            │
│  ├── 13.56 MHz carrier frequency                                │
│  ├── ISO 14443 Type A/B modulation                              │
│  ├── ASK 100% modulation (Type A) / ASK 10% (Type B)          │
│  └── Bit rates: 106/212/424/848 kbps                            │
├─────────────────────────────────────────────────────────────────┤
│  LAYER 2: DATA LINK (PC/SC & EMV)                                │
│  ├── PC/SC (ISO 7816-3 T=0/T=1 over contactless)                │
│  ├── APDU (Application Protocol Data Unit) framing                │
│  ├── CRC/parity error detection                                 │
│  └── Anti-collision (ISO 14443-3)                               │
├─────────────────────────────────────────────────────────────────┤
│  LAYER 3: NETWORK (EMV + GlobalPlatform)                        │
│  ├── EMV Contactless Level 1 (analog/digital)                   │
│  ├── EMV Contactless Level 2 (kernel processing)                │
│  ├── GlobalPlatform Secure Channel Protocol (SCP02/SCP03)        │
│  └── PPSE (Proximity Payment System Environment) discovery        │
├─────────────────────────────────────────────────────────────────┤
│  LAYER 4: TRANSPORT (Payment Schemes)                           │
│  ├── Visa (qVSDC, VMPA)                                         │
│  ├── Mastercard (M/Chip, MCL)                                   │
│  ├── Amex (ExpressPay, ACE)                                     │
│  └── Discover (D-PAS)                                           │
├─────────────────────────────────────────────────────────────────┤
│  LAYER 5: APPLICATION (AIC - Application Issuer Control)        │
│  ├── Card personalization (Track 1/2, PAN, keys)                  │
│  ├── Risk management (velocity checks, limits)                    │
│  └── Authentication (SDA/DDA/CDA)                               │
├─────────────────────────────────────────────────────────────────┤
│  LAYER 6: PRESENTATION (SWIFT Integration)                      │
│  ├── SWIFT MT103/MT202 messages                                   │
│  ├── ISO 20022 (pacs.008, pacs.002)                              │
│  └── GPI (Global Payments Innovation)                           │
├─────────────────────────────────────────────────────────────────┤
│  LAYER 7: INFRASTRUCTURE (RAM - Remote Access Module)           │
│  ├── HSM (Hardware Security Module) integration                   │
│  ├── Key injection & diversification                            │
│  └── Remote card personalization (OTA)                          │
└─────────────────────────────────────────────────────────────────┘
```

## DNS Configuration

### Internal Payment Network DNS

| Service | DNS Record | IP Range | Purpose |
|---------|-----------|----------|---------|
| HSM Primary | `hsm-primary.swiftnet.local` | 10.0.1.0/24 | Key generation & storage |
| HSM Backup | `hsm-backup.swiftnet.local` | 10.0.2.0/24 | Disaster recovery |
| Card Personalization | `perso.swiftnet.local` | 10.0.10.0/24 | EMV data preparation |
| AIC Controller | `aic-master.swiftnet.local` | 10.0.20.0/24 | Application Issuer Control |
| Key Management | `kms.swiftnet.local` | 10.0.30.0/24 | Key diversification |
| EMV Kernel | `kernel-visa.swiftnet.local` | 10.1.1.0/24 | Visa contactless kernel |
| | `kernel-mc.swiftnet.local` | 10.1.2.0/24 | Mastercard kernel |
| | `kernel-amex.swiftnet.local` | 10.1.3.0/24 | Amex kernel |
| | `kernel-discover.swiftnet.local` | 10.1.4.0/24 | Discover kernel |
| SWIFT Gateway | `swift-gw.swiftnet.local` | 10.2.0.0/16 | SWIFT network interface |
| RAM Endpoints | `ram-[region].swiftnet.local` | 10.3.0.0/16 | Remote Access Modules |
| | `ram-us.swiftnet.local` | 10.3.1.0/24 | Americas |
| | `ram-eu.swiftnet.local` | 10.3.2.0/24 | Europe |
| | `ram-ap.swiftnet.local` | 10.3.3.0/24 | Asia-Pacific |

### External DNS (Public-Facing)

| Service | DNS Record | CDN/Load Balancer |
|---------|-----------|-------------------|
| API Gateway | `api.swiftnet.com` | Akamai/AWS CloudFront |
| Status Page | `status.swiftnet.com` | Cloudflare |
| Documentation | `docs.swiftnet.com` | GitHub Pages |

## TCP/IP Port Allocation

### Standard Ports

| Port | Protocol | Service | Security |
|------|----------|---------|----------|
| 443 | TCP | HTTPS API | TLS 1.3, mTLS for mutual auth |
| 465 | TCP | SMTPS (Alerts) | TLS encryption |
| 990 | TCP | FTPS (Batch files) | Explicit TLS |
| 3389 | TCP | RDP (Admin) | VPN + 2FA only |
| 5985-5986 | TCP | WinRM/PowerShell | Kerberos + encryption |

### Payment-Specific Ports

| Port | Protocol | Service | Description |
|------|----------|---------|-------------|
| 17550 | TCP | HSM Thales Luna | Key ceremony operations |
| 1792 | TCP | HSM SafeNet | Backup HSM cluster |
| 3000-3003 | TCP | EMV Kernel APIs | Visa/MC/Amex/Discover kernels |
| 4000 | TCP | AIC Control | Application Issuer Control master |
| 4001-4004 | TCP | AIC Workers | Regional AIC nodes |
| 5000 | TCP | RAM Primary | Remote Access Module main |
| 5001 | TCP | RAM Secure | RAM with end-to-end encryption |
| 8080 | TCP | Thales payShield | HSM for payment processing |
| 8443 | TCP | GP Secure Channel | GlobalPlatform SCP over TLS |
| 9443 | TCP | Personalization | EMV data injection |

### SWIFT Network Ports

| Port | Protocol | SWIFT Service |
|------|----------|---------------|
| 443 | TCP | SWIFT API (gpi) |
| 5025 | TCP | SWIFTNet InterAct |
| 5026 | TCP | SWIFTNet FileAct |
| 5028 | TCP | SWIFTNet Browse |
| 8200 | TCP | HSM SWIFT Alliance |
| 8201 | TCP | HSM SWIFT backup |

## IP Address Schemes

### Production Network (10.0.0.0/8)

```
10.0.0.0/16    - Core Infrastructure
  10.0.1.0/24  - HSM Primary Cluster (Thales Luna 7)
    10.0.1.10  - HSM-01 (Active)
    10.0.1.11  - HSM-02 (Standby)
    10.0.1.20  - HSM Management Console
  
  10.0.2.0/24  - HSM Backup Site
    10.0.2.10  - HSM-BU-01
  
  10.0.10.0/24 - Card Personalization Bureau
    10.0.10.50 - Perso Server Primary
    10.0.10.51 - Perso Server Secondary
    10.0.10.100-150 - Perso Workstations (JCOP programmers)

10.1.0.0/16    - EMV Kernel Network
  10.1.1.0/24  - Visa Kernel
    10.1.1.10  - qVSDC Kernel Primary
    10.1.1.20  - VMPA (Visa Mobile Payment Application)
  
  10.1.2.0/24  - Mastercard Kernel
    10.1.2.10  - M/Chip Kernel
    10.1.2.20  - MCL (Mastercard Contactless)
  
  10.1.3.0/24  - Amex Kernel
    10.1.3.10  - ExpressPay Kernel
    10.1.3.20  - ACE (Amex Chip Entry)
  
  10.1.4.0/24  - Discover Kernel
    10.1.4.10  - D-PAS Kernel

10.2.0.0/16    - SWIFT Integration
  10.2.1.0/24  - SWIFT Alliance Access
    10.2.1.10  - SAA Primary
    10.2.1.11  - SAA Secondary
  10.2.2.0/24  - SWIFT gpi Connector

10.3.0.0/16    - RAM (Remote Access Module)
  10.3.1.0/24  - RAM Americas
    10.3.1.10  - RAM-US-East
    10.3.1.20  - RAM-US-West
  
  10.3.2.0/24  - RAM Europe
    10.3.2.10  - RAM-EU-West
    10.3.2.20  - RAM-EU-Central
  
  10.3.3.0/24  - RAM Asia-Pacific
    10.3.3.10  - RAM-AP-Singapore
    10.3.3.20  - RAM-AP-Tokyo

10.4.0.0/16    - AIC (Application Issuer Control)
  10.4.1.0/24  - AIC Master Nodes
  10.4.2.0/24  - AIC Regional Controllers
```

### Management Network (192.168.0.0/16)

```
192.168.1.0/24  - Out-of-band management
  192.168.1.10  - iLO/iDRAC - HSM-01
  192.168.1.11  - iLO/iDRAC - Perso Server
  192.168.1.50  - Jump Box (Admin access only)
```

## Network Flow: NFC Transaction

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   NFC Card  │────→│  Terminal   │────→│   Acquirer  │────→│   Scheme    │
│  (JCOP)     │ NFC │   (POS)     │ IP  │   Host      │ IP  │  (Visa/MC)  │
│ 13.56 MHz   │     │ 10.0.100.x  │     │ 10.5.0.0/16 │     │ 10.6.0.0/16 │
└─────────────┘     └─────────────┘     └─────────────┘     └──────┬──────┘
                                                                   │
                              ┌────────────────────────────────────┘
                              │
                         ┌────▼────┐     ┌─────────────┐     ┌─────────────┐
                         │  SWIFT  │────→│    RAM      │────→│    AIC      │
                         │ Network │ GPI │  Controller │ SCP │   Master    │
                         │10.2.0.0 │     │ 10.3.x.0/24│     │ 10.4.1.0/24 │
                         └─────────┘     └─────────────┘     └─────────────┘
                              │
                              └────→┌─────────────┐
                                    │    HSM      │
                                    │ 10.0.1.0/24 │
                                    │ (Key Verify)│
                                    └─────────────┘
```

## Protocol Stack Detail

### NFC Physical Layer (Layer 1)

```
Frequency: 13.56 MHz ± 7 kHz
Modulation: ASK 100% (ISO 14443-A) / ASK 10% (ISO 14443-B)
Data Coding: 
  - Type A: Modified Miller (106k), Manchester (212k+)
  - Type B: NRZ-L (all speeds)
Frame Format:
  ┌─────────┬─────────┬─────────┬─────────┬─────────┐
  │  SOF    │  PCB    │  CID    │  Data   │  CRC    │
  │ 1 byte  │ 1 byte  │ 0/1 byte│ 0-255 B │ 2 bytes │
  └─────────┴─────────┴─────────┴─────────┴─────────┘
```

### EMV Contactless Flow (Layer 3-4)

```
1. POLLING: Terminal sends REQA/WUPA
2. ANTICOLLISION: Card sends UID (4/7/10 bytes)
3. SELECT: Terminal selects card by UID
4. RATS: Request for Answer To Select (ATS)
5. PPS: Protocol and Parameter Selection (speed)
6. PPSE SELECT: "2PAY.SYS.DDF01"
7. FCI: File Control Information with application list
8. APP SELECT: AID selection (Visa/MC/Amex/Discover)
9. PDOL: Processing Options Data Object List
10. GPO: Get Processing Options (generates AC)
11. READ RECORD: Read cardholder data
12. CRYPTO: Generate Dynamic Data Authentication
13. COMPLETION: Transaction approved/declined
```

### APDU Structure (Application Layer)

```
Case 1: No data in or out
┌─────┬─────┬─────┬─────┬─────┐
│ CLA │ INS │ P1  │ P2  │     │
│     │     │     │     │     │
└─────┴─────┴─────┴─────┴─────┘
  1     1     1     1   (4 bytes)

Case 2: Data out only (Le present)
┌─────┬─────┬─────┬─────┬─────┐
│ CLA │ INS │ P1  │ P2  │ Le  │
│     │     │     │     │     │
└─────┴─────┴─────┴─────┴─────┘
  1     1     1     1     1   (5 bytes)

Case 3: Data in only (Lc + data)
┌─────┬─────┬─────┬─────┬─────┬────────┐
│ CLA │ INS │ P1  │ P2  │ Lc  │ Data   │
│     │     │     │     │     │ (Lc B) │
└─────┴─────┴─────┴─────┴─────┴────────┘
  1     1     1     1     1     Lc      (5+Lc bytes)

Case 4: Data in and out (Lc + data + Le)
┌─────┬─────┬─────┬─────┬─────┬────────┬─────┐
│ CLA │ INS │ P1  │ P2  │ Lc  │ Data   │ Le  │
│     │     │     │     │     │ (Lc B) │     │
└─────┴─────┴─────┴─────┴─────┴────────┴─────┘
  1     1     1     1     1     Lc       1    (6+Lc bytes)
```

### GlobalPlatform Secure Channel (SCP03)

```
┌─────────────────────────────────────────────────────────┐
│              SCP03 - Secure Channel Protocol 03           │
├─────────────────────────────────────────────────────────┤
│ 1. INITIALIZE UPDATE                                      │
│    Terminal → Card: Host Challenge (8 bytes random)       │
│    Card → Terminal: Key Info + Card Challenge + Card      │
│                   Cryptogram + Sequence Counter           │
│                                                          │
│ 2. EXTERNAL AUTHENTICATE                                  │
│    Terminal → Card: Host Cryptogram + MAC                 │
│    (Proves terminal knows keys, establishes session keys) │
│                                                          │
│ 3. SECURE MESSAGING                                       │
│    All subsequent APDUs encrypted (AES-128-CBC)           │
│    + C-MAC (AES-CMAC)                                    │
│    + R-MAC for responses                                  │
│                                                          │
│ Session Keys Derived:                                     │
│ - S-ENC: Encryption key                                   │
│ - S-MAC: Command MAC key                                  │
│ - S-RMAC: Response MAC key                                │
│ - S-DEK: Data encryption key                              │
└─────────────────────────────────────────────────────────┘
```

## Security Zones & Firewall Rules

### Zone 1: HSM Network (10.0.1.0/24)
```
INBOUND:
- ALLOW TCP 17550 from 10.4.0.0/16 (AIC only)
- ALLOW TCP 22 from 192.168.1.0/24 (Jump box admin)
- DENY ALL

OUTBOUND:
- DENY ALL (air-gapped, no internet)
```

### Zone 2: Personalization (10.0.10.0/24)
```
INBOUND:
- ALLOW TCP 443 from 10.4.0.0/16 (AIC control)
- ALLOW TCP 5000 from 10.3.0.0/16 (RAM)
- ALLOW TCP 3389 from 192.168.1.50 (Admin jump box)
- DENY ALL

OUTBOUND:
- ALLOW TCP 443 to 10.2.0.0/16 (SWIFT)
- ALLOW TCP 53 to 10.0.0.53 (Internal DNS)
- DENY ALL
```

### Zone 3: RAM Network (10.3.0.0/16)
```
INBOUND:
- ALLOW TCP 5000 from VPN pool (10.255.0.0/16)
- ALLOW TCP 5001 from 10.0.10.0/24 (Perso bureau)
- DENY ALL

OUTBOUND:
- ALLOW TCP 443 to 10.4.0.0/16 (AIC)
- ALLOW TCP 8443 to 10.0.10.0/24 (GP Secure Channel)
- DENY ALL
```

## 0x5640002 Feature Deep Dive

The `0x5640002` command is a **Visa proprietary APDU** for advanced card features:

```
Command:  00 56 40 02 00 00
           │  │  │  │  │  │
           │  │  │  │  │  └── Le: 0 (256 bytes expected)
           │  │  │  │  └───── Lc: 0 (no input data)
           │  │  │  └──────── P2: 0x02 (Feature sub-type)
           │  │  └─────────── P1: 0x40 (Feature selector)
           │  └────────────── INS: 0x56 (Visa proprietary)
           └───────────────── CLA: 0x00 (Standard)

Response: [Feature Data] SW1=90 SW2=00
          │              │
          │              └── Success
          └────────────────- TLV encoded feature list
```

**Feature Sub-types (P2 values):**
- `0x01`: Fast DDA/CDA enable
- `0x02`: Enhanced risk parameters (0x5640002)
- `0x03`: Mobile payment optimization
- `0x04`: Transit mode configuration

**P1=0x40 indicates:**
- Bit 6 set = Read/Write feature access
- Bits 5-0 = Feature category (0x00 = general, 0x40 = enhanced security)

This command allows the terminal to:
1. Enable faster cryptographic operations
2. Configure offline authorization limits
3. Set transit-specific parameters (no CVM for fast entry)
4. Activate mobile-specific features (low power, fast wake)

---

This architecture ensures secure, scalable, and compliant payment processing across the global SWIFT network with regional redundancy and strict access controls.

 # Comprehensive Network Security Flow: SWIFT, EMV, NFC & Network Infrastructure


## 1. SWIFT Network Security (Financial Messaging Layer)

**SWIFT (Society for Worldwide Interbank Financial Telecommunication)** operates the global financial messaging network handling over **42 million messages daily** across 11,000+ financial institutions .

### SWIFTNet Infrastructure Security
SWIFT migrated to **SWIFTNet** (IP-based infrastructure) between 2001-2005, replacing legacy X.25 systems . The security framework includes:

| Component | Security Function |
|-----------|------------------|
| **SWIFTNet InterAct** | Real-time and store-and-forward messaging with XML wrappers |
| **SWIFTNet FileAct** | Secure file transfer for bulk data |
| **Relationship Management Application (RMA)** | Replaced bilateral key exchange (BKE) in 2009 for authenticated relationships |
| **Customer Security Controls Framework (CSCF)** | Mandatory security controls including encryption, access controls, and transaction monitoring  |

### SWIFT Security Controls (CSCF v2024) :
- **Multi-factor authentication** for all operator access
- **Transaction limits** by amount, currency, geography, and time (business hours only)
- **End-to-day reconciliation** of MT 900/910 confirmations
- **Session monitoring** for sequential gaps or abnormal behavior
- **Separation of duties** preventing single operators from creating and approving transactions

---

## 2. EMV Chip Card Security (Payment Terminal Layer)

**EMV (Europay, MasterCard, Visa)** represents the global standard for secure card payments using embedded microchips .

### EMV Transaction Security Flow

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│   EMV Chip Card │────▶│   POS Terminal   │────▶│  Issuer Host    │
│  (Secure MCU)   │     │  (EMV L2 Kernel) │     │ (Authorization) │
└─────────────────┘     └──────────────────┘     └─────────────────┘
        │                        │                        │
        ▼                        ▼                        ▼
   • Unique per-transaction   • Risk management       • ARQC verification
     cryptograms (ARQC/TC)    • CVM verification      • Dynamic authentication
   • Offline data auth        • Terminal action       • Authorization response
   • Application cryptograms    analysis                (ARPC)
```

**Key Security Features** :
1. **Dynamic Data Authentication**: Each transaction generates a unique **Authorization Request Cryptogram (ARQC)** that cannot be replayed
2. **Cardholder Verification Methods (CVM)**: PIN, signature, or no verification for low-value transactions
3. **Application Cryptograms**: 
   - **ARQC** (Authorization Request Cryptogram) - requests online approval
   - **TC** (Transaction Certificate) - offline approval
   - **AAC** (Application Authentication Cryptogram) - decline
4. **Risk Management**: Terminal checks floor limits, velocity checking, and random transaction selection for online authorization

---

## 3. NFC (Near Field Communication) Security

**NFC** enables contactless EMV payments through proximity-based communication (typically <4cm) .

### NFC-EMV Integration Security

| Aspect | Security Mechanism |
|--------|-------------------|
| **Proximity Control** | Extremely short range prevents remote eavesdropping |
| **Tokenization** | Payment tokens replace actual PAN (Primary Account Number)  |
| **Dynamic Authentication** | Same EMV cryptogram generation as contact transactions |
| **Secure Element** | Credentials stored in hardware-isolated secure chips |

**Mobile NFC Payment Flow** :
1. **Provisioning**: Card data → Token Service Provider (TSP) → Tokenized credentials → Mobile device
2. **Transaction**: Device generates ARQC using token → Payment network detokenizes → Issuer authorizes
3. **Response**: Issuer sends ARPC → Network retokenizes → Device receives confirmation

---

## 4. Network Infrastructure Security (NIC, Firewall, DNS, IP, MAC)

### Network Interface Card (NIC) Security
- **Hardware Addressing**: Each NIC has a unique **MAC (Media Access Control)** address (48-bit identifier)
- **Promiscuous Mode Detection**: Security systems monitor for NICs capturing all traffic (potential sniffing)
- **Virtual NICs**: Cloud environments use virtual NICs with MAC address spoofing protection

### Firewall Security Layers

**Traditional Firewall (Layer 3-4)**:
- **IP Address Filtering**: Whitelist/blacklist source/destination IP addresses
- **Port Control**: Restrict services by TCP/UDP port numbers
- **Stateful Inspection**: Track connection states (TCP handshakes, session validity)

**Next-Generation Firewall (Layer 7)**:
- **Deep Packet Inspection (DPI)**: Analyze payload content for malware/signatures
- **Application Awareness**: Identify applications regardless of port used
- **User Identity Integration**: Tie IP addresses to user authentication

### DNS Security (DNSSEC)
- **Authentication**: Cryptographic verification that DNS responses haven't been tampered with
- **Integrity**: Prevents DNS cache poisoning attacks that could redirect traffic to malicious IPs
- **Query Logging**: Monitor for data exfiltration via DNS tunneling

### IP & MAC Address Security Relationship

| Layer | Address Type | Security Function |
|-------|-------------|-------------------|
| **Layer 2 (Data Link)** | MAC Address | Physical device identification, ARP inspection, DHCP snooping |
| **Layer 3 (Network)** | IP Address | Logical network segmentation, routing decisions, geo-blocking |
| **Layer 4-7** | Port/Session | Application access control, stateful filtering |

**ARP Security**: Firewalls and switches use **Dynamic ARP Inspection (DAI)** to prevent ARP spoofing attacks where attackers associate their MAC with legitimate IP addresses .

---

## 5. Integrated Security Flow: Complete Transaction Journey

Here's how these components work together in a secure financial transaction:

### Scenario: Contactless EMV Payment via SWIFT Network

```
┌─────────────┐   NFC/EMV   ┌─────────────┐   IP/Encrypted   ┌─────────────┐
│  EMV Card   │◄───────────▶│  Merchant   │◄───────────────▶│   Acquirer  │
│ (NFC Chip)  │  Short-range│   Terminal  │    TLS/HTTPS     │   Gateway   │
└─────────────┘   encrypted └─────────────┘                  └──────┬──────┘
       │                                                            │
       │                                                            ▼
       │                                                   ┌─────────────┐
       │                                                   │   Firewall  │
       │                                                   │  (Inspects  │
       │                                                   │   IP/MAC,   │
       │                                                   │   DPI, IDS) │
       │                                                   └──────┬──────┘
       │                                                            │
       ▼                                                            ▼
┌─────────────┐                                          ┌─────────────┐
│  Generates  │                                          │   SWIFTNet  │
│   ARQC +    │                                          │   Network   │
│   Tokenized │                                          │  (Financial │
│     PAN     │                                          │   Messaging)│
└─────────────┘                                          └──────┬──────┘
                                                                │
                                                                ▼
                                                       ┌─────────────┐
                                                       │   Issuer    │
                                                       │    Bank     │
                                                       │ (Validates  │
                                                       │   ARQC via  │
                                                       │   HSM)      │
                                                       └─────────────┘
```

### Security Controls at Each Layer:

1. **Physical Layer (NFC)**: Proximity-based communication prevents remote interception; EMV chip generates unique cryptograms 
2. **Network Layer (IP/MAC)**: Firewalls verify source IP reputation, MAC address consistency, and prevent ARP spoofing 
3. **Transport Layer**: TLS 1.3 encryption for data in transit between terminal and gateway
4. **Application Layer (EMV)**: Dynamic authentication via ARQC/ARPC exchange; tokenization ensures PAN never transmitted in clear 
5. **Financial Layer (SWIFT)**: RMA authentication, message encryption, transaction monitoring, and mandatory CSCF controls 
6. **DNS Layer**: DNSSEC prevents redirection to malicious payment gateways

### Key Security Intersections:

- **MAC + IP Binding**: DHCP snooping ensures devices can't spoof MAC addresses to bypass IP-based access controls
- **DNS + Firewall**: DNS filtering blocks known malicious domains before IP connection attempts
- **EMV + Network Encryption**: Even if network traffic is intercepted, EMV cryptograms are single-use and time-bound
- **SWIFT + Endpoint**: End-to-end encryption ensures messages remain secure even if intermediate network layers are compromised

This layered approach ensures that compromising any single component (NFC, IP, DNS, or SWIFT) doesn't compromise the entire transaction—true **defense in depth** for financial network security.
