// SimpleLauncher.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "SimpleLauncher.h"

#include "stb_image.h"

#include <iostream>
#include <string>

// Global Variables:
HINSTANCE hInst;// current instance

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

HICON LoadIconFromFile(std::wstring str) {
	return (HICON)LoadImageW(NULL, str.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
}

#define LAUNCH_BUTTON_ID 0x101

int main()
{

	HINSTANCE hInstance = GetModuleHandle(nullptr);
	int       nCmdShow = SW_SHOW;

	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow)) {
		return FALSE;
	}

	MSG msg;

	// Main message loop:
	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}


//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIconFromFile(L"icon.ico");
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_SIMPLELAUNCHER);
	wcex.lpszClassName = L"SIMPLELAUNCHER";
	wcex.hIconSm = LoadIconFromFile(L"icon.ico");

	return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//

#include <Windows.h>
#include <dwmapi.h>
#pragma comment(lib, "Dwmapi.lib")

#include <vector>
#include <filesystem>
#include <fstream>
#include <locale>
#include <codecvt>
#include <string>
#include <thread>

std::wstring appName(L"SimpleLauncher");
std::string downloadURL("");
std::string versionURL("");
std::string fileToRun("");
std::string titleImage("title.jpg");
bool doUnzip = true;
uint32_t textColor = 0xffffff; // bgr
int waitingMillis = 1500;
bool forceUpdate = false;

std::wstring status(L"");


int progressBarHeight = 30;
int buttonHeight = 30;

HWND hWnd = nullptr;
HWND hProgressBar = nullptr;
HWND hLaunchButton = nullptr;
HBITMAP hImage = nullptr;
int hImageW = 0, hImageH = 0;
uint32_t* rawImagePixels;


// Function to create a bitmap from ARGB data
HBITMAP CreateARGBBitmap(int width, int height, uint32_t* data)
{
	BITMAPINFO bmi;
	ZeroMemory(&bmi, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = width;
	bmi.bmiHeader.biHeight = -height; // Negative height to indicate a top-down bitmap
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32; // 32 bits per pixel (ARGB format)

	HDC hdcScreen = GetDC(NULL);
	void* pBits; // Pointer to the bitmap bits

	// Create a DIB section to hold the ARGB data
	HBITMAP hBitmap = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
	uint32_t* pBits1 = (uint32_t*)pBits;
	if (hBitmap) {
		// Copy the ARGB data into the bitmap
		for (int i = 0, s = width * height; i < s; i++) {
			uint32_t color = data[i];
			// rgba -> argb
			uint8_t c0 = color, c1 = color >> 8, c2 = color >> 16, c3 = color >> 24;
			pBits1[i] = data[i] = (c0 << 16) | (c1 << 8) | c2;
		}
	}

	ReleaseDC(NULL, hdcScreen);
	return hBitmap;
}

// Function to trim leading and trailing whitespace from a string.
std::string Trim(const std::string& str) {
	std::string trimmed = str;

	// Trim leading whitespace.
	trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](int ch) {
		return !std::isspace(ch);
				  }));

	// Trim trailing whitespace.
	trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), [](int ch) {
		return !std::isspace(ch);
				  }).base(), trimmed.end());

	return trimmed;
}

std::wstring StrToWStr(std::string src) {
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	return converter.from_bytes(src);
}

bool parseBool(std::string value) {
	return value == "1" || value == "true" || value == "True" || value == "TRUE";
}

void LoadConfig() {
	std::ifstream file("config.txt");
	if (file.is_open()) {
		std::string line;
		while (std::getline(file, line)) {
			size_t sep = line.find('=');
			if (sep > 0 && sep != std::string::npos) {
				std::string key = Trim(line.substr(0, sep));
				std::string value = Trim(line.substr(sep + 1));
				if (key == "APPLICATION_NAME") {
					appName = StrToWStr(value);
				}
				else if (key == "DOWNLOAD_URL") {
					downloadURL = value;
				}
				else if (key == "VERSION_URL") {
					versionURL = value;
				}
				else if (key == "FILE_TO_RUN") {
					fileToRun = value;
				}
				else if (key == "TITLE_IMAGE") {
					titleImage = value;
				}
				else if (key == "ENABLE_UNZIP") {
					doUnzip = parseBool(value);
				}
				else if (key == "WAITING_MILLIS") {
					waitingMillis = std::stoi(value);
				}
				else if (key == "TEXT_COLOR") {
					textColor = std::stoi(value, nullptr, 16);
				}
				else if (key == "FORCE_UPDATE") {
					forceUpdate = parseBool(value);
				}
			}
		}
		file.close();
	}
	else {
		std::cerr << "Error opening config.txt" << std::endl;
	}
}

std::string LoadVersion(std::string fileName) {
	std::ifstream file(fileName);
	if (file.is_open()) {
		std::string line;
		while (std::getline(file, line)) {
			if (!line.empty()) {
				file.close();
				return line;
			}
		}
		file.close();
	}
	return "";
}

#include <Urlmon.h>
// tells the linker to include this dll at runtime
#pragma comment(lib, "urlmon.lib")

class DownloadStatusCallback : public IBindStatusCallback
{
public:
	// IUnknown methods
	STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
		if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IBindStatusCallback)) {
			*ppv = static_cast<IBindStatusCallback*>(this);
			return S_OK;
		}
		*ppv = nullptr;
		return E_NOINTERFACE;
	}

	STDMETHODIMP_(ULONG) AddRef() override {
		return 1;
	}

	STDMETHODIMP_(ULONG) Release() override {
		return 1;
	}

	// IBindStatusCallback methods
	STDMETHODIMP OnStartBinding(DWORD dwReserved, IBinding* pib) override {
		return E_NOTIMPL;
	}

	STDMETHODIMP GetPriority(LONG* pnPriority) override {
		return E_NOTIMPL;
	}

	STDMETHODIMP OnLowResource(DWORD reserved) override {
		return E_NOTIMPL;
	}

	STDMETHODIMP OnProgress(ULONG ulProgress, ULONG ulProgressMax, ULONG ulStatusCode, LPCWSTR szStatusText) override {
		SendMessage(hProgressBar, PBM_SETPOS, ulProgress * 65535.0 / ulProgressMax, 0);
		if (ulStatusCode == BINDSTATUS_BEGINDOWNLOADDATA) {
			std::wcout << L"Downloading..." << std::endl;
		}
		else if (ulStatusCode == BINDSTATUS_ENDDOWNLOADDATA) {
			std::wcout << L"Download completed." << std::endl;
		}
		return S_OK;
	}

	STDMETHODIMP OnStopBinding(HRESULT hresult, LPCWSTR szError) override {
		return E_NOTIMPL;
	}

	STDMETHODIMP GetBindInfo(DWORD* grfBINDF, BINDINFO* pbindinfo) override {
		return E_NOTIMPL;
	}

	STDMETHODIMP OnDataAvailable(DWORD grfBSCF, DWORD dwSize, FORMATETC* pformatetc, STGMEDIUM* pstgmed) override {
		return E_NOTIMPL;
	}

	STDMETHODIMP OnObjectAvailable(REFIID riid, IUnknown* punk) override {
		return E_NOTIMPL;
	}
};

HRESULT Download(std::wstring url, std::wstring dstPath) {
	DownloadStatusCallback callback;
	HRESULT hr = URLDownloadToFile(NULL, url.c_str(), dstPath.c_str(), 0, &callback);
	if (SUCCEEDED(hr)) {
		std::cout << "File downloaded successfully." << std::endl;
	}
	else {
		std::cout << "Download failed. Error code: " << hr << std::endl;
	}
	return hr;
}

void SetStatus(std::wstring newStatus) {
	status = newStatus;
	InvalidateRect(hWnd, NULL, TRUE);
}

void LaunchGame(bool wait) {

	// wait a little to not be completely instant lol, can be removed ofc without any downsides
	if(wait) std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(waitingMillis));

	SendMessage(hProgressBar, PBM_SETPOS, 65535, 0);

	// Create process information
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	std::cout << "Starting program " << fileToRun << std::endl;

	std::string filePath("Data/Application/");
	filePath += fileToRun;

	// Start the new process
	if (!CreateProcessA(NULL,   // No module name (use the command line)
		(LPSTR) filePath.c_str(), // Command line
		NULL,   // Process handle not inheritable
		NULL,   // Thread handle not inheritable
		FALSE,  // Set handle inheritance to FALSE
		0,      // No creation flags
		NULL,   // Use parent's environment block
		NULL,   // Use parent's starting directory
		&si,    // Pointer to STARTUPINFO structure
		&pi))   // Pointer to PROCESS_INFORMATION structure
	{
		// Failed to start the new process
		SetStatus(L"Error creating process :/");
		return;
	}

	// Close handles to the new process and its primary thread.
	// You can continue with your work here without waiting for helloworld.exe to finish.
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	// Exit the current process
	ExitProcess(0);

}

#include <Windows.h>
#include <ShellAPI.h> // Include the ShellAPI header
#pragma comment(lib, "Shell32.lib") // Link against the Shell32 library

// Function to delete a folder recursively
bool DeleteFolderRecursively(const std::wstring& folderPath) {
	// Double null-terminate the folder path as required by SHFILEOPSTRUCT
	wchar_t szFrom[MAX_PATH];
	wcscpy_s(szFrom, folderPath.c_str());
	szFrom[folderPath.length() + 1] = '\0';

	SHFILEOPSTRUCT fileOp;
	ZeroMemory(&fileOp, sizeof(fileOp));
	fileOp.wFunc = FO_DELETE;
	fileOp.pFrom = szFrom;
	fileOp.fFlags = FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NO_UI;

	int result = SHFileOperation(&fileOp);
	return (result == 0 && !fileOp.fAnyOperationsAborted);
}

#include "miniz/miniz.h"
#include <direct.h>
#include <filesystem>

void UpdateAndStart() {
	(void) _mkdir("Data");
	std::string oldVersion = LoadVersion("Data/version.txt");
	std::remove("Data/version.tmp");
	if (SUCCEEDED(Download(StrToWStr(versionURL), L"Data/version.tmp"))) {
		std::string newVersion = LoadVersion("Data/version.tmp");
		std::cout << "Old vs New Version: " << oldVersion << " vs " << newVersion << std::endl;
		if (newVersion != oldVersion) {

			SetStatus(oldVersion.empty() ? L"Downloading game files" : L"Updating game files");

			if (SUCCEEDED(Download(StrToWStr(downloadURL), L"Data/download.zip"))) {

				const char* extractDir = "Data/Application/";
				DeleteFolderRecursively(L"Data/Application");
				(void)_mkdir(extractDir);

				if (doUnzip) {
					// unzip files

					SetStatus(L"Unpacking game files!");

					mz_zip_archive zip_archive;
					ZeroMemory(&zip_archive, sizeof(mz_zip_archive));
					mz_bool status = mz_zip_reader_init_file(&zip_archive, "Data/download.zip", 0);
					if (!status) {
						std::cerr << "Failed to open the zip file download.zip" << std::endl;
						SetStatus(L"Unpacking failed #0");
						return;
					}

					int numFiles = mz_zip_reader_get_num_files(&zip_archive);
					for (int i = 0; i < numFiles; i++) {
						mz_zip_archive_file_stat file_stat{};
						ZeroMemory(&file_stat, sizeof(mz_zip_archive_file_stat));
						if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
							std::cerr << "Failed to read file info from the zip file." << std::endl;
							mz_zip_reader_end(&zip_archive);
							SetStatus(L"Unpacking failed #1");
							return;
						}

						// Construct the full path for the extracted file
						std::string extractPath(extractDir);
						std::string fileName = std::string(file_stat.m_filename);
						if (fileName.find("..") != std::string::npos || fileName.find(":") != std::string::npos) {
							std::cerr << "Ignored invalid file with .." << std::endl;
							mz_zip_reader_end(&zip_archive);
							SetStatus(L"Unpacking failed #2");
							return;
						}
						extractPath += fileName;

						// Create the directories if they don't exist
						if (file_stat.m_is_directory) {

							std::cout << "Creating dir  " << fileName << std::endl;

							int code = _mkdir(extractPath.c_str());
							if (code != 0) std::cerr << "Failed to mkdir " << extractPath << std::endl;
						}
						else {

							std::cout << "Extracing     " << fileName << std::endl;

							// Extract the file
							if (!mz_zip_reader_extract_to_file(&zip_archive, i, extractPath.c_str(), 0)) {
								std::cout << "Failed to extract " << fileName << std::endl;
								mz_zip_reader_end(&zip_archive);
								SetStatus(L"Unpacking failed #3");
								return;
							}

						}

						// Update the progress bar after each file extraction
						SendMessage(hProgressBar, PBM_SETPOS, (i + 1) * 65535.0 / numFiles, 0);

					}
				}
				else {
					std::string fileName = "Data/Application/" + fileToRun;
					(void)std::rename("Data/download.zip", fileName.c_str());
				}

				SetStatus(L"Updated successfully!");

				// after unzip completed, 
				(void)std::remove("Data/download.zip");
				(void)std::remove("Data/version.txt");
				(void)std::rename("Data/version.tmp", "Data/version.txt");

				// then start game
				LaunchGame(true);
			
			} else {
				SetStatus(L"Game Download Failed");
				if(!forceUpdate) LaunchGame(true);
			}
		}
		else {
			SetStatus(L"Launching game");
			LaunchGame(true);
		}
	}
	else {
		SetStatus(L"Failed to query version");
		if (!forceUpdate) LaunchGame(true);
	}
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable
	
	LoadConfig();

	hWnd = CreateWindowW(L"SimpleLauncher", appName.c_str(), WS_OVERLAPPEDWINDOW,
						CW_USEDEFAULT, 0, 800, 600, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd) return FALSE;

	hProgressBar = CreateWindowEx(
		0,                                  // Optional window styles.
		PROGRESS_CLASS,                     // Progress bar control class.
		NULL,                               // Text for progress bar. 
		WS_CHILD | WS_VISIBLE | PBS_SMOOTH, // Visible and child window styles.
		0, 0,                               // Position of the progress bar.
		600, progressBarHeight,			    // Width and height of the progress bar.
		hWnd,                               // Parent window.
		NULL,                               // No menu.
		GetModuleHandle(NULL),              // Instance of the module.
		NULL                                // Pointer not needed.
	);

	hLaunchButton = CreateWindowEx(
		0,                           // Optional window styles.
		L"BUTTON",                   // Button control class.
		L"Launch Game",              // Text for the button.
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, // Visible and child window styles for a push button.
		0, 600 - buttonHeight,       // Position of the button (left-top coordinates).
		800, buttonHeight,           // Width and height of the button.
		hWnd,                        // Parent window.
		NULL,                        // No menu.
		GetModuleHandle(NULL),       // Instance of the module.
		NULL                         // Pointer not needed.
	);
	SetWindowLongPtr(hLaunchButton, GWLP_ID, LAUNCH_BUTTON_ID);

	if (versionURL.empty()) {
		SetStatus(L"Missing version URL");
	}
	else if (downloadURL.empty()) {
		SetStatus(L"Missing download URL");
	}
	else if (fileToRun.empty()) {
		SetStatus(L"Missing name of file to run");
	}
	else {
		SetStatus(L"Requesting version");
		new std::thread(UpdateAndStart);
	}

	int w = 2, h = 2;
	int planes = 1, bitCount = 32;
	uint32_t data[] = { 0xff00ff, 0, 0, 0xff00ff };

	int x,y,n;
	unsigned char* data1 = stbi_load(titleImage.c_str(), &x, &y, &n, 4);
	if (data1) { w = x; h = y; }
	else std::cerr << "Missing title image!" << std::endl;
	hImageW = w;
	hImageH = h;

	hImage = CreateARGBBitmap(w, h, rawImagePixels = data1 ? (uint32_t*) data1 : data);

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		int notificationCode = HIWORD(wParam);
		switch (wmId)
		{
		case LAUNCH_BUTTON_ID:
			if (notificationCode == BN_CLICKED) {
				LaunchGame(false);
			}
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_SIZE:
	{
		// Get the new client width after resizing
		int newClientWidth = LOWORD(lParam);
		int newClientHeight = HIWORD(lParam);
		// Update the progress bar's width based on the new client width
		SetWindowPos(hProgressBar, NULL, 0, 0, newClientWidth, progressBarHeight, SWP_NOZORDER);
		SetWindowPos(hLaunchButton, NULL, 0, newClientHeight - buttonHeight, newClientWidth, buttonHeight, SWP_NOZORDER);
		// Ensure the progress bar is repainted with its new size
		InvalidateRect(hProgressBar, NULL, TRUE);
		InvalidateRect(hLaunchButton, NULL, TRUE);
		break;
	}
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code that uses hdc here...

		RECT clientRect;
		GetClientRect(hWnd, &clientRect);
		auto m_clientWidth = clientRect.right - clientRect.left;
		auto m_clientHeight = clientRect.bottom - clientRect.top;

		SendMessage(hProgressBar, PBM_SETRANGE, 0, (65535 << 16) | 0);
		SendMessage(hProgressBar, PBM_SETPOS, 35000, 0);

		if (hImage) {

			// scaling: crop
			int displayWidth = m_clientWidth;
			int displayHeight = m_clientHeight - progressBarHeight - buttonHeight;
			int xPos = 0, yPos = progressBarHeight;

			int dx = 0, dy = 0;
			if (displayWidth * hImageH > hImageW * displayHeight) {
				dy = (hImageH - (displayHeight * hImageW) / displayWidth) >> 1;
			} else {
				dx = (hImageW - (displayWidth * hImageH) / displayHeight) >> 1;
			}

			// Draw the image using StretchBlt
			HDC hdcMem = CreateCompatibleDC(hdc);
			HGDIOBJ hOld = SelectObject(hdcMem, hImage);
			SetStretchBltMode(hdc, STRETCH_HALFTONE);
			StretchBlt(hdc, xPos, yPos, displayWidth, displayHeight, hdcMem, dx, dy, hImageW-dx*2, hImageH-dy*2, SRCCOPY);
			SelectObject(hdcMem, hOld);
			DeleteDC(hdcMem);

		}

		// Create and set up the centered text
		RECT textRect;
		textRect.left = 0;
		textRect.top = 30;
		textRect.right = m_clientWidth;
		textRect.bottom = 60;
		SetBkMode(hdc, TRANSPARENT);
		SetTextColor(hdc, textColor);
		DrawText(hdc, status.c_str(), -1, &textRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
		SetBkMode(hdc, OPAQUE);

		EndPaint(hWnd, &ps);
	}
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
