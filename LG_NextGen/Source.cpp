#include "Header.h"

void LogItem(const char* Msg...)
{
	va_list arguments;
	va_start(arguments, Msg);

	int len = _vscprintf(Msg, arguments) + 1;
	char* text = new char[len];
	vsprintf_s(text, len, Msg, arguments);
	va_end(arguments);

	FILE* plik = nullptr;
	fopen_s(&plik, "LG-Debug.log", "a");
	if (plik)
	{
		fprintf(plik, "%s\n", text);
		fclose(plik);
	}
	delete[] text;
}

DWORD dwVersion = 0;

enum Version
{
	v124e = 6387,
	v126a = 6401,
};

enum FileDLL
{
	DDLL_GAME = 0,
	DDLL_STORM = 1,
	DDLL_D3D8 = 2,
	DDLL_D3D9 = 3,
	DDLL_LIB7B = 4,
	DDLL_LIB7C = 5,
	DDLL_LIB7E = 6,
};

const char* szGameDLL()
{
	if (dwVersion == v124e)
	{
		HMODULE hmod = GetModuleHandle("lib7C.dll");
		if (hmod)
			return "lib7C.dll";
	}

	if (dwVersion == v126a)
	{
		HMODULE hmod = GetModuleHandle("lib7E.dll");
		if (hmod)
			return "lib7E.dll";
	}

	return "Game.dll";
}

void WarcraftVersion()
{
	DWORD dwHandle = 0;
	DWORD dwLen = GetFileVersionInfoSize(szGameDLL(), &dwHandle);
	if (dwLen == 0)
		return;

	char* lpData = new char[dwLen];
	if (!GetFileVersionInfo(szGameDLL(), dwHandle, dwLen, lpData))
	{
		delete lpData;
		return;
	}

	VS_FIXEDFILEINFO* Version;
	UINT uLen = sizeof(VS_FIXEDFILEINFO);
	if (!VerQueryValue(lpData, "\\", (LPVOID *)&Version, &uLen))
	{
		delete lpData;
		return;
	}
	delete lpData;
	dwVersion = LOWORD(Version->dwFileVersionLS);
}

DWORD GetDllOffset(const char* dll, int offset)
{
	HMODULE hmod = GetModuleHandle(dll);
	if (!hmod)
		hmod = LoadLibrary(dll);

	if (!hmod) return 0;
	if (offset < 0)
		return (DWORD)GetProcAddress(hmod, (LPCSTR)(-offset));

	return ((DWORD)hmod) + offset;
}

DWORD GetDllOffsetEx(int num, int offset, int offset2)
{
	if (dwVersion <= 0)
		WarcraftVersion();

	if (dwVersion == v124e)
		return GetDllOffset(szGameDLL(), offset);

	if (dwVersion == v126a)
		return GetDllOffset(szGameDLL(), offset2);

	return 0;
}

#define W3_FUNC(DLL, NAME, RETURN, CONV, ARGS, OFFSET, OFFSET2) typedef RETURN (CONV* DLL##_##NAME##_t) ARGS; __declspec(selectany) extern DLL##_##NAME##_t DLL##_##NAME = (DLL##_##NAME##_t)GetDllOffsetEx(DDLL_##DLL, OFFSET, OFFSET2);   ///
#define W3_VAR(DLL, NAME, TYPE, OFFSET, OFFSET2) typedef TYPE DLL##_##NAME##_vt; __declspec(selectany) extern DLL##_##NAME##_vt * DLL##_##NAME = (DLL##_##NAME##_vt *)GetDllOffsetEx(DDLL_##DLL, OFFSET, OFFSET2);                          ///

W3_FUNC(GAME, ChatSendEvent, int, __fastcall, (int GlobalGlueObjAddr, int zero, int event_vtable), 0x2FD240, 0x2FC700)
W3_FUNC(GAME, GameChatSetState, int, __fastcall, (int chat, int unused, BOOL IsOpened), 0x341FA0, 0x341460)
W3_VAR(GAME, W3XGlobalClass, int*, 0xACBDD8, 0xAB4F80)
W3_VAR(GAME, IsChatBoxOpen, bool, 0xAE8450, 0xAD15F0)
W3_VAR(GAME, GlobalGlueObj, int, 0xAE54CC, 0xACE66C)
W3_VAR(GAME, EventVtable, int, 0xAB0CD0, 0xA9ACB0)

void CreateFolder(string path)
{
	if (!CreateDirectory(path.c_str(), NULL))
	{
		return;
	}
}

bool FileExists(const string& fileName)
{
	const DWORD fileAttributes = GetFileAttributesA(fileName.c_str());
	return (fileAttributes != INVALID_FILE_ATTRIBUTES) && (fileAttributes != FILE_ATTRIBUTE_DIRECTORY);
}

char* substr(char* arr, int begin, int len)
{
	char* res = new char[len + 1];
	for (int i = 0; i < len; i++)
		res[i] = *(arr + begin + i);
	res[len] = 0;
	return res;
}

int GetChatOffset()
{
	int pclass = *(int*)GAME_W3XGlobalClass;
	if (pclass > 0)
		return *(int*)(pclass + 0x3FC);

	return 0;
}

char* GetChatString()
{
	int pChatOffset = GetChatOffset();
	if (pChatOffset > 0)
	{
		pChatOffset = *(int*)(pChatOffset + 0x1E0);
		if (pChatOffset > 0)
		{
			pChatOffset = *(int*)(pChatOffset + 0x1E4);
			return (char*)pChatOffset;
		}
	}

	return 0;
}

void __stdcall SendMessageToChat(const char* msg, ...)
{
	if (!msg || msg[0] == '\0')
		return;

	char szBuffer[8192] = {};
	va_list Args;

	va_start(Args, msg);
	vsprintf(szBuffer, msg, Args);
	va_end(Args);

	int ChatOffset = GetChatOffset();
	if (!ChatOffset) return;

	char* pChatString = GetChatString();
	if (!pChatString) return;

	BlockInput(TRUE);


	if (*GAME_IsChatBoxOpen)
	{
		GAME_GameChatSetState(ChatOffset, 0, 0);
		GAME_GameChatSetState(ChatOffset, 0, 1);
	}
	else
	{
		GAME_GameChatSetState(ChatOffset, 0, 1);
	}

	sprintf(pChatString, "%.128s", szBuffer);
	GAME_ChatSendEvent(*GAME_GlobalGlueObj, 0, *GAME_EventVtable);

	BlockInput(FALSE);
}

int __stdcall LG_AutoLoadCode(char* szName)
{
	CreateFolder("LG_NextGen\\");
	char line[4096] = {};
	char* str;
	char* str2;

	string szFile = "LG_NextGen\\SaveCode_" + string(szName) + ".txt";
	FILE* pFile = fopen(szFile.c_str(), "r");
	if (!pFile) return 0;

	while (fgets(line, 4096, pFile))
	{
		str = strstr(line, "-load ");
		if (str)
		{
			str2 = str + strlen("-load ");
			break;
		}
	}

	fclose(pFile);
	char* a = substr(str2, 0, strlen(str2) - 4);
	if (strlen(a) > 0)
	{
		string autoload = string("-load ") + string(a);
		SendMessageToChat("%s", autoload.c_str());
	}

	return 1;
}

namespace MPQ
{
	DWORD MpqLoadPriority = 15;
	bool OpenArchive(const string& fileName, HANDLE* mpqHandle)
	{
		if (!SFileOpenArchive(fileName.c_str(), MpqLoadPriority, 0, mpqHandle))
			return false;

		MpqLoadPriority++;

		return true;
	}
}

int checkerror = 2;
unsigned long __stdcall DowFile(LPVOID)
{
	string szFile = "LG_NextGen\\LG_Model.mpq";
	string strUrl = "https://github.com/thaison1995/LG-NextGen/raw/main/LG_Model.mpq";
	HRESULT res = URLDownloadToFile(NULL, strUrl.c_str(), szFile.c_str(), 0, NULL);
	if (res != S_OK)
		checkerror = 0;

	HANDLE MPQHandleEx = 0;
	if (!MPQ::OpenArchive(szFile, &MPQHandleEx))
		checkerror = 1;

	return 0;
}

int __stdcall LG_DowloadModel(int)
{
	CreateFolder("LG_NextGen\\");
	string szFile = "LG_NextGen\\LG_Model.mpq";
	if (!FileExists(szFile))
	{
		CloseHandle(CreateThread(0, 0, DowFile, 0, 0, 0));
	}

	HANDLE MPQHandleEx = 0;
	if (!MPQ::OpenArchive(szFile, &MPQHandleEx))
		checkerror = 1;

	return checkerror;
}

string pszname;
unsigned long __stdcall LG_DowloadDataThread(LPVOID)
{
	string szFile = "LG_NextGen\\SaveCode_" + pszname + ".txt";
	if (!FileExists(szFile))
	{
		string strUrl = "https://github.com/thaison1995/LG-NextGen/raw/main/LG_NextGenData/" + string("SaveCode_") + pszname + ".txt";
		HRESULT res = URLDownloadToFile(NULL, strUrl.c_str(), szFile.c_str(), 0, NULL);
		if (res != S_OK)
			return 0;
	}

	return 0;
}

int __stdcall LG_DowloadData(char* szName)
{
	pszname = szName;
	CloseHandle(CreateThread(0, 0, LG_DowloadDataThread, 0, 0, 0));
	return 1;
}

std::string readFile(const std::string& filePath) 
{
	std::ifstream file(filePath);
	std::ostringstream oss;
	oss << file.rdbuf();
	return oss.str();
}

string encodeBase64(const string& data) 
{
	DWORD encodedSize = 0;
	if (Base64EncodeGetRequiredLength(data.size(), ATL_BASE64_FLAG_NOCRLF) > encodedSize)
	{
		encodedSize = Base64EncodeGetRequiredLength(data.size(), ATL_BASE64_FLAG_NOCRLF);
	}
	vector<char> encodedData(encodedSize);
	Base64Encode(reinterpret_cast<const BYTE*>(data.c_str()), data.size(), encodedData.data(), (int*)&encodedSize, ATL_BASE64_FLAG_NOCRLF);
	return string(encodedData.data(), encodedSize);
}

string getFileSha(const wstring& token, const wstring& repo, const wstring& path)
{
	HINTERNET hSession = WinHttpOpen(L"GitHub Uploader/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!hSession) return "";

	HINTERNET hConnect = WinHttpConnect(hSession, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
	if (!hConnect)
	{
		WinHttpCloseHandle(hSession);
		return "";
	}

	wstring url = L"/repos/" + repo + L"/contents/" + path;
	HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", url.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
	if (!hRequest)
	{
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return "";
	}

	wstring headers = L"Authorization: token " + token;
	WinHttpAddRequestHeaders(hRequest, headers.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);

	BOOL bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
	if (bResults)
	{
		bResults = WinHttpReceiveResponse(hRequest, nullptr);
		if (bResults)
		{
			DWORD statusCode = 0;
			DWORD statusCodeSize = sizeof(statusCode);
			WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &statusCode, &statusCodeSize, nullptr);
			if (statusCode == 200)
			{
				string response;
				DWORD dwSize = 0;
				DWORD dwDownloaded = 0;
				do
				{
					char buffer[4096];
					memset(buffer, 0, sizeof(buffer));
					bResults = WinHttpReadData(hRequest, (LPVOID)buffer, sizeof(buffer) - 1, &dwSize);
					if (dwSize > 0)
					{
						response.append(buffer, dwSize);
					}
				} while (bResults && dwSize > 0);

				size_t shaPos = response.find("\"sha\":\"");
				if (shaPos != string::npos)
				{
					shaPos += 7;
					size_t endPos = response.find("\"", shaPos);
					return response.substr(shaPos, endPos - shaPos);
				}
			}
		}
	}

	WinHttpCloseHandle(hRequest);
	WinHttpCloseHandle(hConnect);
	WinHttpCloseHandle(hSession);
	return "";
}

void uploadFileToGitHub(const wstring& token, const wstring& repo, const wstring& path, const string& filePath) 
{
	string sha = getFileSha(token, repo, path);
	string content = readFile(filePath);
	string encodedData = encodeBase64(content);

	string payload = R"({"message": "Upload file via WinHttp C++", "content": ")" + encodedData + R"(", "sha": ")" + sha + R"("})";

	HINTERNET hSession = WinHttpOpen(L"GitHub Uploader/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!hSession) return;

	HINTERNET hConnect = WinHttpConnect(hSession, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
	if (!hConnect) 
	{
		WinHttpCloseHandle(hSession);
		return;
	}

	wstring url = L"/repos/" + repo + L"/contents/" + path;
	HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"PUT", url.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
	if (!hRequest) 
	{
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return;
	}

	wstring headers = L"Content-Type: application/json\r\nAuthorization: token " + token;
	WinHttpAddRequestHeaders(hRequest, headers.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);

	BOOL bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)payload.c_str(), payload.size(), payload.size(), 0);
	if (bResults) 
	{
		bResults = WinHttpReceiveResponse(hRequest, nullptr);
		if (bResults) 
		{
			DWORD statusCode = 0;
			DWORD statusCodeSize = sizeof(statusCode);
			WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &statusCode, &statusCodeSize, nullptr);
			if (statusCode == 201 || statusCode == 200) 
			{
				wcout << L"File uploaded successfully!" << endl;
			}
			else 
			{
				wcerr << L"Failed to upload. Status code: " << statusCode << endl;
			}
		}
	}

	WinHttpCloseHandle(hRequest);
	WinHttpCloseHandle(hConnect);
	WinHttpCloseHandle(hSession);
}

unsigned long __stdcall LG_UploadDataThread(LPVOID)
{
	string token = _xor_("TOKEN_API_GITHUB");
	string repo = "thaison1995/LG-NextGen";
	string path = "LG_NextGenData/SaveCode_" + string(pszname) + ".txt";
	string filePath = "LG_NextGen/SaveCode_" + string(pszname) + ".txt";

	uploadFileToGitHub(wstring(token.begin(), token.end()), wstring(repo.begin(), repo.end()), wstring(path.begin(), path.end()), filePath);

	return 0;
}

int __stdcall LG_UploadData(char* szName)
{
	pszname = szName;
	CloseHandle(CreateThread(0, 0, LG_UploadDataThread, 0, 0, 0));
	return 1;
}

//int main()
//{
//	string token = _xor_(TOKEN_API_GITHUB);
//	string repo = "thaison1995/LG-NextGen";
//	string path = "LG_NextGenData/SaveCode_" + string("sdvsdv") + ".txt";
//	string filePath = "LG_NextGen/SaveCode_" + string("sdvsdv") + ".txt";
//
//	uploadFileToGitHub(wstring(token.begin(), token.end()), wstring(repo.begin(), repo.end()), wstring(path.begin(), path.end()), filePath);
//
//	system("pause");
//	return 1;
//}

BOOL __stdcall DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hinstDLL);
			CreateFolder("LG_NextGen\\");
			if (dwVersion <= 0)
				WarcraftVersion();

			break;
		}

		case DLL_PROCESS_DETACH:
		{

			break;
		}
	}

	return TRUE;
}