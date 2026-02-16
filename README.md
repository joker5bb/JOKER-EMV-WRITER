DONATION BTC bc1q4389vgy042up2h78eeel5ne64xnr5rx54ksp66

## How to Compile
g++ jokeremvwriter.cpp -o jokeremvwriter.exe -mwindows -lwinscard -lcomctl32 -luser32 -lgdi32 -std=c++17 -O2 -Wall

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

## Limitations

- **No Secure Channel**: Lacks SCP02/SCP03 security for production cards
- **Fixed File IDs**: Assumes Track 1=0x10, Track 2=0x20 (card-specific)
- **No Certificate Management**: Does not handle issuer certificates or keys
- **Windows Only**: Uses Win32 API (not cross-platform)
- **No Logging**: No persistent logging of operations or errors
