# LANChat: Decentralized Local P2P Chat Application

[📺 Watch the Architecture & Live Demo Video](https://youtu.be/zecsLOvPwKk)

## 📌 Project Overview
A lightweight, highly stable local Peer-to-Peer (P2P) chat application built from scratch for Windows using C++17 and the Qt 5.15.2 framework. The architecture is completely decentralized, eliminating the need for a central server by utilizing UDP socket broadcasts for real-time peer discovery and persistent, duplex TCP connections for message transport.

## 🧱 Architecture & Design Decisions
The system is divided into four highly focused, single-responsibility modules interacting via Qt's signals and slots mechanism:

- **PeerManager (UDP Subnet Discovery):** 
  Enables decentralization by broadcasting UDP heartbeats every 2.5 seconds on port `45454` using `QUdpSocket`. These heartbeats contain JSON-serialized metadata, including the peer's chosen username and local TCP listening port. The module queries active local network interfaces (filtering out loopback and virtual interfaces) to identify local subnet broadcast targets (e.g., `255.255.255.255` or subnet-specific broadcasts). It maintains an in-memory `QMap<QString, PeerInfo>` mapping usernames to details. A local sweep timer runs at 2.0-second intervals to check peer activity; if no heartbeat is received from a peer within an 8.0-second threshold, that peer is removed from the active map and the UI is updated immediately.

- **ChatManager (Persistent Duplex TCP Transport):** 
  To exchange chat messages, `ChatManager` starts a `QTcpServer` on an OS-assigned ephemeral port (port `0` at bind time). Upon discovering a peer via UDP, sending a message initiates a persistent TCP connection to the peer's listening port. Once connected, a custom handshake packet containing the local user's username is sent. To eliminate race conditions and avoid duplicate TCP sockets between the same pair of users, only a single socket connection is registered in the connection map `QMap<QString, QTcpSocket*> m_connections` (keyed by the remote peer's username). If both users initiate a connection simultaneously, the connection initiated by the lexicographically smaller username takes precedence, and the redundant socket is safely discarded. TCP stream fragmentation is mitigated using `QDataStream` serialization, prepending each packet with a 4-byte (quint32) size header. When a socket receives data, the receiver buffers incoming bytes until the full payload size indicated by the header is available, ensuring complete message packets are parsed.

- **DatabaseManager (User-Isolated File Persistence):** 
  To support message history without database dependencies, `DatabaseManager` handles structured text file writing and parsing. Messages are saved in user-centric log files (e.g., `chat_history_Username.txt`). Newlines and escape characters in the message content are encoded (with `\n`, `\r`, and `\\`) using custom escape routines to serialize each chat record onto a single line. This format guarantees that concurrent testing of multiple local instances in the same working directory avoids cross-instance file lock collisions, as each instance reads and writes to a file prefixed with its own sanitized username.

- **MainWindow (Programmatic QSS UI):** 
  The interface is constructed entirely via C++ layout code—without the use of Qt Designer `.ui` files—to eliminate UI compiler dependencies and ensure absolute layout stability. It applies a modern, custom Qt Style Sheet (QSS) layout modeled after modern chat applications (incorporating a `#17212b` sidebar/header and a `#0e1621` chat canvas). Since standard `QTextBrowser` instances do not support dynamic width adjustment for message bubbles, bubbles are rendered using HTML `<table>` structures inside the browser window. Setting `align="left"` (for incoming messages) or `align="right"` (for outgoing messages) forces the table cells to automatically wrap and fit the text width, delivering a polished user experience.

## 🛠️ Build and Execution Lifecycle
Follow these steps to compile and run the application on Windows:

1. **Prerequisites:**
   - Windows 10/11 OS.
   - Qt 5.15.2 (Miniconda distribution recommended, with binaries placed in `C:\Users\Dell\miniconda3\Library\bin`).
   - MSVC 2019 Build Tools (installed with Visual Studio 2019 or standalone).

2. **Compilation via Automated Tooling:**
   - Open the **Native Tools Command Prompt for VS 2019** (x64 or x86 depending on the targeted architecture).
   - Navigate to the project root directory.
   - Execute the build script:
     ```cmd
     build.bat
     ```
   - *Technical Explanation:* The `build.bat` script initializes the compiler environment variables by calling `vcvarsall.bat x64`, appends the Qt bin path to the local command prompt `PATH` variable, runs `qmake -makefile -spec win32-msvc LANChat.pro` to generate the MSVC-compatible Makefile, and executes `nmake` to build the optimized Release binary located in `release\LANChat.exe`.

3. **Execution Environment:**
   - Run the launcher script from the project root:
     ```cmd
     run.bat
     ```
   - *Technical Explanation:* The `run.bat` script appends the required Qt DLL paths to the session `PATH` variable and spawns `release\LANChat.exe` in the background, resolving all dynamic library dependencies.

## 🧪 Verification & Testing Protocol
Use this deterministic validation protocol to verify the decentralized discovery and chat loops:

- **Local Multi-Instance Testing:**
  1. Open two separate command windows in the project root.
  2. Launch a separate instance from each window using `run.bat`.
  3. Change the username of the first instance to `Alice` and the second instance to `Bob` via the "Change Name" button.
  4. With visibility toggled ON (indicated by the teal-colored "Visible" button state), verify that `Alice` and `Bob` instantly discover each other and appear in their respective left-hand panel lists within 2.5 seconds.
  5. Select `Bob` in Alice's list, type a message, and click Send. Confirm the message appears as a right-aligned blue bubble on Alice's window and a left-aligned gray bubble on Bob's window.
  6. Click the "Visible" button on Alice's window to turn visibility OFF. Observe Bob's window: verify that Alice disappears from Bob's list exactly 8.0 seconds after the final broadcast, conforming to the network timeout sweep rule.
  7. Toggle Alice's visibility back to ON. Verify she reappears in Bob's sidebar within the 2.5-second heartbeat window.
  8. Close the application. Verify that `chat_history_Alice.txt` and `chat_history_Bob.txt` have been created in the working directory containing escaped, single-line log entries for all transmitted messages.
