#ifdef _WIN32_WCE
#include <shlobj.h>  // for BROWSEINFOW, LPCITEMIDLIST
#include <objbase.h> // for CoTaskMemFree prototype, but we reimplement
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>



// Implementation of compatibility functions for WinCE

HFONT CreateFontW(int nHeight, int nWidth, int nEscapement, int nOrientation,
                  int fnWeight, DWORD fdwItalic, DWORD fdwUnderline,
                  DWORD fdwStrikeOut, DWORD fdwCharSet, DWORD fdwOutputPrecision,
                  DWORD fdwClipPrecision, DWORD fdwQuality, DWORD fdwPitchAndFamily,
                  LPCWSTR lpszFaceName) {
    // Construct LOGFONTW and call CreateFontIndirectW
    LOGFONTW lf;
    ZeroMemory(&lf, sizeof(lf));
    lf.lfHeight = nHeight;
    lf.lfWidth = nWidth;
    lf.lfEscapement = nEscapement;
    lf.lfOrientation = nOrientation;
    lf.lfWeight = fnWeight;
    lf.lfItalic = (BYTE)fdwItalic;
    lf.lfUnderline = (BYTE)fdwUnderline;
    lf.lfStrikeOut = (BYTE)fdwStrikeOut;
    lf.lfCharSet = (BYTE)fdwCharSet;
    lf.lfOutPrecision = (BYTE)fdwOutputPrecision;
    lf.lfClipPrecision = (BYTE)fdwClipPrecision;
    lf.lfQuality = (BYTE)fdwQuality;
    lf.lfPitchAndFamily = (BYTE)fdwPitchAndFamily;
    if (lpszFaceName) {
        wcsncpy(lf.lfFaceName, lpszFaceName, LF_FACESIZE - 1);
        lf.lfFaceName[LF_FACESIZE - 1] = 0;
    }
    // On WinCE CreateFontIndirectW should exist
    return CreateFontIndirectW(&lf);
}

// Helper: Convert wide string to ASCII string
static std::string WideToAscii(LPCWSTR wstr) {
    if (!wstr) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    if (len <= 0) return "";
    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], len, NULL, NULL);
    return result;
}

// Helper: Convert ASCII string to wide string
static std::wstring AsciiToWide(const std::string& str) {
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    if (len <= 0) return L"";
    std::wstring result(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], len);
    return result;
}

// Helper: Trim leading/trailing whitespace
static std::string Trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

// Helper: Check if line is a section header [SectionName]
static bool IsSection(const std::string& line, const std::string& sectionName) {
    std::string trimmed = Trim(line);
    if (trimmed.empty() || trimmed[0] != '[') return false;
    size_t closeBracket = trimmed.find(']');
    if (closeBracket == std::string::npos) return false;
    std::string section = Trim(trimmed.substr(1, closeBracket - 1));
    return section == sectionName;
}

// Helper: Parse key=value line
static bool ParseKeyValue(const std::string& line, std::string& outKey, std::string& outValue) {
    size_t eqPos = line.find('=');
    if (eqPos == std::string::npos) return false;
    outKey = Trim(line.substr(0, eqPos));
    outValue = Trim(line.substr(eqPos + 1));
    return true;
}

// Helper: Search for a key within a section, return its value if found
static bool FindKeyInSection(const std::vector<std::string>& lines,
                             const std::string& section,
                             const std::string& key,
                             std::string& outValue) {
    bool inSection = false;
    for (const auto& raw : lines) {
        std::string line = Trim(raw);
        if (line.empty() || line[0] == ';' || line[0] == '#')
            continue;
        if (line[0] == '[') {
            inSection = IsSection(line, section);
            continue;
        }
        if (inSection) {
            std::string k, v;
            if (ParseKeyValue(line, k, v) && k == key) {
                outValue = v;
                return true;
            }
        }
    }
    return false;
}

// Helper: Read entire file into memory
static bool ReadFile(const std::string& filename, std::vector<std::string>& lines) {
    FILE* fp = fopen(filename.c_str(), "r");
    if (!fp) return false;
    
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fp)) {
        std::string line(buffer);
        // Remove trailing newline
        if (!line.empty()) {
            size_t lastPos = line.length() - 1;
            if (line[lastPos] == '\n') {
                line = line.substr(0, lastPos);
            }
        }
        lines.push_back(line);
    }
    fclose(fp);
    return true;
}

// Helper: Write lines to file
static bool WriteFile(const std::string& filename, const std::vector<std::string>& lines) {
    FILE* fp = fopen(filename.c_str(), "w");
    if (!fp) return false;
    
    for (size_t i = 0; i < lines.size(); ++i) {
        fputs(lines[i].c_str(), fp);
        fputc('\n', fp);
    }
    fclose(fp);
    return true;
}


DWORD GetPrivateProfileStringW(LPCWSTR lpAppName, LPCWSTR lpKeyName,
                               LPCWSTR lpDefault, LPWSTR lpReturnedString,
                               DWORD nSize, LPCWSTR lpFileName) {
    if (!lpFileName || !lpAppName || !lpKeyName || !lpReturnedString || nSize == 0) {
        if (lpDefault && lpReturnedString && nSize > 0) {
            wcsncpy(lpReturnedString, lpDefault, nSize - 1);
            lpReturnedString[nSize - 1] = 0;
        }
        return lpDefault ? (DWORD)wcslen(lpDefault) : 0;
    }

    std::string filename = WideToAscii(lpFileName);
    std::string section = WideToAscii(lpAppName);
    std::string key = WideToAscii(lpKeyName);
    std::string defValue = WideToAscii(lpDefault);

    std::vector<std::string> lines;
    if (!ReadFile(filename, lines)) {
        std::wstring wdefValue = AsciiToWide(defValue);
        wcsncpy(lpReturnedString, wdefValue.c_str(), nSize - 1);
        lpReturnedString[nSize - 1] = 0;
        return (DWORD)wdefValue.length();
    }

    std::string found;
    if (FindKeyInSection(lines, section, key, found)) {
        std::wstring wvalue = AsciiToWide(found);
        wcsncpy(lpReturnedString, wvalue.c_str(), nSize - 1);
        lpReturnedString[nSize - 1] = 0;
        return (DWORD)wvalue.length();
    }

    // Not found, return default
    std::wstring wdefValue = AsciiToWide(defValue);
    wcsncpy(lpReturnedString, wdefValue.c_str(), nSize - 1);
    lpReturnedString[nSize - 1] = 0;
    return (DWORD)wdefValue.length();
}

BOOL WritePrivateProfileStringW(LPCWSTR lpAppName, LPCWSTR lpKeyName,
                                LPCWSTR lpString, LPCWSTR lpFileName) {
    if (!lpFileName || !lpAppName || !lpKeyName) return FALSE;

    std::string filename = WideToAscii(lpFileName);
    std::string section = WideToAscii(lpAppName);
    std::string key = WideToAscii(lpKeyName);
    std::string value = WideToAscii(lpString ? lpString : L"");

    std::vector<std::string> lines;
    ReadFile(filename, lines);  // OK if file doesn't exist yet

    // Find section
    int sectionIdx = -1;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (IsSection(Trim(lines[i]), section)) {
            sectionIdx = i;
            break;
        }
    }

    if (sectionIdx == -1) {
        // Create new section
        lines.push_back("[" + section + "]");
        sectionIdx = lines.size() - 1;
    }

    // Find key in section
    int keyIdx = -1;
    for (size_t i = sectionIdx + 1; i < lines.size(); ++i) {
        std::string trimmed = Trim(lines[i]);
        if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#') continue;
        if (trimmed[0] == '[') break;  // Next section

        std::string k, v;
        if (ParseKeyValue(trimmed, k, v) && k == key) {
            keyIdx = i;
            break;
        }
    }

    if (keyIdx == -1) {
        // Insert new key after section
        lines.insert(lines.begin() + sectionIdx + 1, key + "=" + value);
    } else {
        // Update existing key
        lines[keyIdx] = key + "=" + value;
    }

    return WriteFile(filename, lines) ? TRUE : FALSE;
}

// GetDIBits stub: no support, return 0
int GetDIBits(HDC hdc, HBITMAP hbm, UINT start, UINT cLines, 
                    LPVOID lpvBits, LPBITMAPINFO lpbmi, UINT usage)
{
    if (!hbm || !lpbmi) return 0;

    BITMAP bmp;
    // 元のビットマップ情報を取得
    if (!GetObject(hbm, sizeof(BITMAP), &bmp)) return 0;

    // -----------------------------------------------------------
    // 1. lpvBits が NULL の場合（情報のクエリモード）
    // -----------------------------------------------------------
    if (lpvBits == NULL)
    {
        lpbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        lpbmi->bmiHeader.biWidth = bmp.bmWidth;
        lpbmi->bmiHeader.biHeight = bmp.bmHeight;
        lpbmi->bmiHeader.biPlanes = 1;
        
        // biBitCountが0の場合は、元のビットマップのビット深度を設定
        if (lpbmi->bmiHeader.biBitCount == 0) {
            lpbmi->bmiHeader.biBitCount = bmp.bmBitsPixel;
        }
        
        lpbmi->bmiHeader.biCompression = BI_RGB;

        // DWORD境界(4バイト)にアライメントされた1行あたりのバイト数を計算
        DWORD stride = ((lpbmi->bmiHeader.biWidth * lpbmi->bmiHeader.biBitCount + 31) / 32) * 4;
        lpbmi->bmiHeader.biSizeImage = stride * bmp.bmHeight;
        
        return 1; // 成功
    }

    // -----------------------------------------------------------
    // 2. lpvBits が指定されている場合（ピクセルデータの取得モード）
    // -----------------------------------------------------------
    int absHeight = abs(lpbmi->bmiHeader.biHeight);
    DWORD stride = ((lpbmi->bmiHeader.biWidth * lpbmi->bmiHeader.biBitCount + 31) / 32) * 4;
    
    // biSizeImageが0の場合は計算して補完
    if (lpbmi->bmiHeader.biSizeImage == 0) {
        lpbmi->bmiHeader.biSizeImage = stride * absHeight;
    }

    // 要求されたフォーマット(lpbmi)で、空のDIBセクションを作成する
    void* pDibBits = NULL;
    HBITMAP hbmDib = CreateDIBSection(hdc, lpbmi, usage, &pDibBits, NULL, 0);
    if (!hbmDib || !pDibBits) return 0;

    // デバイスコンテキストを作成して、それぞれのビットマップを選択
    HDC hdcSrc = CreateCompatibleDC(hdc);
    HDC hdcDst = CreateCompatibleDC(hdc);

    HGDIOBJ hOldSrc = SelectObject(hdcSrc, hbm);
    HGDIOBJ hOldDst = SelectObject(hdcDst, hbmDib);

    // 元のビットマップからDIBセクションへ画像データをコピー
    // ※ここでGDIによって指定のピクセルフォーマット（16bit, 24bit, 32bit等）へ自動変換される
    BitBlt(hdcDst, 0, 0, bmp.bmWidth, bmp.bmHeight, hdcSrc, 0, 0, SRCCOPY);

    // -----------------------------------------------------------
    // 3. 取得したDIBのピクセルデータを要求されたバッファ(lpvBits)へコピー
    // -----------------------------------------------------------
    // コピーするスキャンライン数を決定
    UINT copyLines = (cLines > (UINT)absHeight) ? absHeight : cLines;

    BYTE* pSrcBits = (BYTE*)pDibBits;
    BYTE* pDstBits = (BYTE*)lpvBits;

    // startで指定された開始スキャンラインまでポインタを進める
    pSrcBits += start * stride;

    // メモリコピー (指定された行数 × 1行のバイト数)
    memcpy(pDstBits, pSrcBits, stride * copyLines);

    // 後始末 (GDIリソースの解放)
    SelectObject(hdcSrc, hOldSrc);
    SelectObject(hdcDst, hOldDst);

    DeleteDC(hdcSrc);
    DeleteDC(hdcDst);
    DeleteObject(hbmDib);

    return copyLines; // コピーしたスキャンライン数を返す
}

#endif // _WIN32_WCE
