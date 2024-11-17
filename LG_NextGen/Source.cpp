#include "Header.h"

string V_DirWar3;

#define IsKeyPressed(CODE) (GetAsyncKeyState(CODE) & 0x8000) > 0

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
W3_FUNC(GAME, SetCamera, void, __thiscall, (int a1, int whichField, float Dis, float duration, int a5), 0x3065A0, 0x305A60)

W3_VAR(GAME, GetHwnd, HWND, 0xAE81F8, 0xAD1398)
W3_VAR(GAME, W3XGlobalClass, int*, 0xACBDD8, 0xAB4F80)
W3_VAR(GAME, IsChatBoxOpen, bool, 0xAE8450, 0xAD15F0)
W3_VAR(GAME, GlobalGlueObj, int, 0xAE54CC, 0xACE66C)
W3_VAR(GAME, EventVtable, int, 0xAB0CD0, 0xA9ACB0)

streampos fileSize(const char* filePath)
{
	streampos fsize = 0;
	ifstream file(filePath, std::ios::binary);

	fsize = file.tellg();
	file.seekg(0, std::ios::end);
	fsize = file.tellg() - fsize;
	file.close();

	return fsize;
}

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
	CreateFolder(V_DirWar3);
	char line[4096] = {};
	char* str;
	char* str2;

	string szFile = V_DirWar3 + "\\SaveCode_" + string(szName) + ".txt";
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
		DeleteFile(szFile.c_str());
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

HANDLE MPQHandleEx = 0;
unsigned long __stdcall DowFile(LPVOID)
{
	string szFile = V_DirWar3 + "\\LG_Model.mpq";
	string strUrl = "https://github.com/thaison1995/LG-NextGen/raw/main/LG_Model.mpq";
	HRESULT res = URLDownloadToFile(NULL, strUrl.c_str(), szFile.c_str(), 0, NULL);
	if (res != S_OK)
		return 0;

	MPQ::OpenArchive(szFile, &MPQHandleEx);

	return 0;
}

int __stdcall LG_DowloadModel(int)
{
	CreateFolder(V_DirWar3);
	string szFile = V_DirWar3 + "\\LG_Model.mpq";

	if (!FileExists(szFile))
	{
		CloseHandle(CreateThread(0, 0, DowFile, 0, 0, 0));
	}
	else
	{
		MPQ::OpenArchive(szFile, &MPQHandleEx);
		auto nSizeFile = fileSize(szFile.c_str());
		if (nSizeFile < 10000000)
		{
			if (MPQHandleEx)
			{
				SFileCloseArchive(MPQHandleEx);
				MPQHandleEx = 0;
			}

			CloseHandle(CreateThread(0, 0, DowFile, 0, 0, 0));
		}
	}

	return 1;
}

string pszname;
unsigned long __stdcall LG_DowloadDataThread(LPVOID)
{
	string szFile = V_DirWar3 + "\\SaveCode_" + pszname + ".txt";

	if (FileExists(V_DirWar3 + "\\SaveCode.txt"))
		rename(string(V_DirWar3 + "\\SaveCode.txt").c_str(), szFile.c_str());

	if (!FileExists(szFile))
	{
		string strUrl = "http://160.187.146.137/LG_NextGen/" + string("SaveCode_") + pszname + ".txt";
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

bool uploadFileToServer(const char *filename, const char *filepath)
{
	const char *type = "text/plain";
	char boundary[] = "---------------------------1BsFuekqmX38dmi1e82t4XFLWRKJSY9MPL"; 
	char nameForm[] = "uploaded_file";
	char iaddr[] = "160.187.146.137";
	char url[] = "upload.php";
	char conn_type[] = "Connection: close";

	char hdrs[512] = { '-' }; 
	FILE* pFile = fopen(filepath, "rb");
	if (!pFile) return false; 

	fseek(pFile, 0, SEEK_END);
	long lSize = ftell(pFile);
	rewind(pFile);

	char* content = (char*)malloc(sizeof(char)*(lSize + 1));
	if (!content) return false;

	size_t result = fread(content, 1, lSize, pFile);
	if (result != lSize)
		return false;

	content[lSize] = '\0';

	fclose(pFile);

	char* buffer = (char*)malloc(sizeof(char)*lSize + 2048);

	sprintf(hdrs, "Content-Type: multipart/form-data; boundary=%s\r\n%s", boundary, conn_type);
	sprintf(buffer, "--%s\r\nContent-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n", boundary, nameForm, filename);
	sprintf(buffer, "%sContent-Type: %s\r\n", buffer, type);
	sprintf(buffer, "%s\r\n%s", buffer, content);
	sprintf(buffer, "%s\r\n\n--%s--\r\n", buffer, boundary);

	HINTERNET hSession = InternetOpen("Mozilla/5.0 (Windows; U; Windows NT 6.1; en-US; rv:1.9.1.5) Gecko/20091102 Firefox/3.5.5 (.NET CLR 3.5.30729)", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (hSession == NULL)
		return false;

	HINTERNET hConnect = InternetConnect(hSession, iaddr, INTERNET_DEFAULT_HTTP_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 1);
	if (hConnect == NULL)
		return false;

	HINTERNET hRequest = HttpOpenRequest(hConnect, (const char*)"POST", _T(url), NULL, NULL, NULL, INTERNET_FLAG_RELOAD, 1);
	BOOL sent = HttpSendRequest(hRequest, hdrs, strlen(hdrs), buffer, strlen(buffer));
	if (!sent)
	{
		InternetCloseHandle(hSession);
		InternetCloseHandle(hConnect);
		InternetCloseHandle(hRequest);
		return false;
	}

	DeleteFile(filepath);
	InternetCloseHandle(hSession);
	InternetCloseHandle(hConnect);
	InternetCloseHandle(hRequest);
	return true;
}

int __stdcall LG_UploadData(char* szName)
{
	string file = "SaveCode_" + string(szName) + ".txt";
	string filePath = V_DirWar3 + "\\SaveCode_" + string(szName) + ".txt";

	if (uploadFileToServer(file.c_str(), filePath.c_str()) == false)
		if (uploadFileToServer(file.c_str(), filePath.c_str()) == false)
			uploadFileToServer(file.c_str(), filePath.c_str());

	return 1;
}

float camera = 2000;
int __stdcall LG_SetCamera(float fcam)
{
	if (!(*GAME_IsChatBoxOpen))
	{
		int pclass = *(int*)GAME_W3XGlobalClass;
		if (pclass > 0)
		{
			int ptrcam = *(int*)(pclass + 0x254);
			if (ptrcam > 0)
			{
				GAME_SetCamera(ptrcam, 0, fcam, 0, 1);
				GAME_SetCamera(ptrcam, 1, 10000.0f, 0, 0);
			}
		}
	}

	return 1;
}

//void CreateFileText(int nCount)
//{
//	string filename = "LG_NextGenDataTest\\SaveCode_" + to_string(nCount) + ".txt";
//
//	ofstream file(filename);
//	if (!file) return;
//	file << nCount << endl;
//	file.close();
//
//	cout << nCount << endl;
//}

//int main()
//{
//	LG_UploadData((char*)"LienHopQuoc");
//	system("pause\n");
//	return 1;
//}

unsigned long __stdcall LG_SetCameraTheard(LPVOID)
{
	while (TRUE)
	{
		try
		{
			if (*GAME_GetHwnd == GetForegroundWindow())
			{
				if (!(*GAME_IsChatBoxOpen))
				{
					if (IsKeyPressed(VK_SUBTRACT) || IsKeyPressed(VK_OEM_MINUS))
					{
						camera -= 60.0f;
						LG_SetCamera(camera);
					}

					if (IsKeyPressed(VK_ADD) || IsKeyPressed(VK_OEM_PLUS))
					{
						camera += 60.0f;
						LG_SetCamera(camera);
					}
				}
			}
		}
		catch (...)
		{

		}
		Sleep(50);
	}
}

BOOL __stdcall DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hinstDLL);
			char filename[4096] = {};
			GetCurrentDirectory(4096, filename);
			V_DirWar3.assign(filename);
			V_DirWar3 += "\\LG_NextGen";
			CreateFolder(V_DirWar3);
			if (dwVersion <= 0)
				WarcraftVersion();
			CloseHandle(CreateThread(0, 0, LG_SetCameraTheard, 0, 0, 0));
			break;
		}
		case DLL_PROCESS_DETACH:
		{

			break;
		}
	}

	return TRUE;
}