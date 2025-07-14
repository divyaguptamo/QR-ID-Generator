# 📷 QR-ID Generator

A lightweight Windows application built in C using the Win32 API and `qrcodegen` library to generate QR codes from personal information like Name, Phone, and Email.

## ✨ Features

- 🖥️ Simple GUI using Windows API
- 🧾 Input: Name, Phone Number, Email
- 🔳 Real-time QR Code generation
- 💾 Save QR as `.png` using GDI+
- 🧱 Standalone `.exe` without external dependencies
- 🗂️ File Save Dialog using `commdlg.h`

---

## 📷 Screenshot

<img width="395" height="583" alt="image" src="https://github.com/user-attachments/assets/04fe8486-37f0-4b78-9d03-fb2e16da4762" />

---

## 🛠️ Tech Stack

- **Language**: C
- **Libraries**: 
  - `qrcodegen` (for QR creation)
  - GDI+ (for image rendering)
  - Win32 API (for GUI)
- **Output Format**: `.png`

---

## ▶️ How to Run

1. Clone or download this repository.
2. Open the `.c` file in any C/C++ IDE that supports Win32 (e.g., Code::Blocks or Visual Studio).
3. Link the following libraries:
   - `gdiplus.lib`
   - `Comdlg32.lib` (for file dialog)
4. Compile and run the project.

### 🔗 Required Headers/Libraries:
- `windows.h`
- `gdiplus.h`
- `commdlg.h`
- `qrcodegen.h` (add the `qrcodegen` C library)

---

## 🚀 Future Enhancements

- 🔐 Add encryption to the saved QR content
- 🎨 Let user customize QR color or size
- 📄 Support other export formats like JPG or SVG
- 📤 Direct print or email options after generation
- 📥 Load data from a CSV or form

---

## 🌍 Impact

Ideal for:
- Healthcare centers to tag patient records
- Business cards with QR info
- Event registrations and identity systems
- Offline contactless information sharing

---

## 👩‍💻 Author

**Divya Gupta**  
A developer passionate about building useful tools using low-level programming and Windows APIs.

---
