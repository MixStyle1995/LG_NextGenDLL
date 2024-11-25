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
static const DWORD CALL = 0xE8;
static const DWORD JUMP = 0xE9;
static const DWORD NOP = 0x90;
static const DWORD RET = 0xC3;
static const DWORD XOR = 0x33;
static const DWORD CUSTOM = 0;

enum Version
{
	v124e = 6387,
	v126a = 6401,
};

struct w3_version
{
	int v124e;
	int v126a;
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

BOOL WriteBytes(LPVOID pAddr, VOID * pData, DWORD dwLen)
{
	DWORD dwOld;

	if (!VirtualProtect(pAddr, dwLen, PAGE_READWRITE, &dwOld))
		return FALSE;

	memcpy(pAddr, pData, dwLen);
	return VirtualProtect(pAddr, dwLen, dwOld, &dwOld);
}

DWORD VirtualProtectEX(DWORD pAddress, DWORD len, DWORD prot)
{
	DWORD oldprot = 0;
	VirtualProtect((void*)pAddress, len, prot, &oldprot);
	return oldprot;
}

void WriteLocalBYTES(DWORD pAddress, void* buf, int len)
{
	DWORD oldprot = VirtualProtectEX(pAddress, len, PAGE_EXECUTE_READWRITE);
	WriteProcessMemory(GetCurrentProcess(), (void*)pAddress, buf, len, 0);
	VirtualProtectEX(pAddress, len, oldprot);
}

void PatchVALUE(DWORD addr, DWORD param, DWORD len)
{
	WriteLocalBYTES(addr, &param, len);
}

void Patch(BYTE bInst, const char* dll, DWORD pAddr, DWORD pFunc, DWORD dwLen, const char* Type)
{
	if (pAddr == 0) return;
	pAddr = GetDllOffset(dll, pAddr);
	BYTE *bCode = new BYTE[dwLen];
	if (bInst)
	{
		::memset(bCode, 0x90, dwLen);
		bCode[0] = bInst;
		if (pFunc)
		{
			if (bInst == 0xE8 || bInst == 0xE9)
			{
				DWORD dwFunc = pFunc - (pAddr + 5);
				*(DWORD*)&bCode[1] = dwFunc;
			}
			else if (bInst == 0x68 || bInst == 0x05 || bInst == 0x5B)
			{
				*(LPDWORD)&bCode[1] = pFunc;
			}
			else if (bInst == 0x83)
			{
				*(WORD*)&bCode[1] = (WORD)pFunc;
			}
			else
			{
				bCode[1] = (BYTE)pFunc;
			}
		}
	}
	else
	{
		if (dwLen == 6)
		{
			::memset(bCode, 0x00, dwLen);
			*(DWORD*)&bCode[0] = pFunc;
		}
		else if (dwLen == 4)
			*(DWORD*)&bCode[0] = pFunc;
		else if (dwLen == 3)
		{
			PatchVALUE(pAddr, pFunc, dwLen);
		}
		else if (dwLen == 2)
			*(WORD*)&bCode[0] = (WORD)pFunc;
		else if (dwLen == 1)
			*(BYTE*)&bCode[0] = (BYTE)pFunc;
		else {
			memcpy(bCode, (void*)pFunc, dwLen);
		}
	}

	if (!WriteBytes((void*)pAddr, bCode, dwLen))
	{
		delete[] bCode;
	}
	delete[] bCode;

	FlushInstructionCache(GetCurrentProcess(), (void*)pAddr, dwLen);
}

void PatchEx(BYTE bInst, const char* dll, w3_version pAddr, DWORD pFunc, DWORD dwLen, const char* Type)
{
	if (dwVersion <= 0)
		WarcraftVersion();

	if (dwVersion == v124e)
		Patch(bInst, dll, pAddr.v124e, pFunc, dwLen, Type);

	if (dwVersion == v126a)
		Patch(bInst, dll, pAddr.v126a, pFunc, dwLen, Type);
}

#pragma pack(1)
struct DataLG
{
	int ELO;
	int Win;
	int Lose;
};

struct CTextFrame
{
	BYTE						baseControl[0x1E4];		//0x0
	uint32_t					textLength;				//0x1E4
	char*						text;					//0x1E8
};
#pragma pack()

map<string, DataLG*> mName;
bool ismaplgng = false;

#define W3_FUNC(DLL, NAME, RETURN, CONV, ARGS, OFFSET, OFFSET2) typedef RETURN (CONV* DLL##_##NAME##_t) ARGS; __declspec(selectany) extern DLL##_##NAME##_t DLL##_##NAME = (DLL##_##NAME##_t)GetDllOffsetEx(DDLL_##DLL, OFFSET, OFFSET2);   ///
#define W3_VAR(DLL, NAME, TYPE, OFFSET, OFFSET2) typedef TYPE DLL##_##NAME##_vt; __declspec(selectany) extern DLL##_##NAME##_vt * DLL##_##NAME = (DLL##_##NAME##_vt *)GetDllOffsetEx(DDLL_##DLL, OFFSET, OFFSET2);                          ///

W3_FUNC(GAME, ChatSendEvent, int, __fastcall, (int GlobalGlueObjAddr, int zero, int event_vtable), 0x2FD240, 0x2FC700)
W3_FUNC(GAME, GameChatSetState, int, __fastcall, (int chat, int unused, BOOL IsOpened), 0x341FA0, 0x341460)
W3_FUNC(GAME, SetCamera, void, __thiscall, (int a1, int whichField, float Dis, float duration, int a5), 0x3065A0, 0x305A60)
W3_FUNC(GAME, GetPlayerName, char*, __thiscall, (int nPlayerId), 0x2F9AD0, 0x2F8F90)

W3_VAR(GAME, GetHwnd, HWND, 0xAE81F8, 0xAD1398)
W3_VAR(GAME, W3XGlobalClass, int*, 0xACBDD8, 0xAB4F80)
W3_VAR(GAME, IsChatBoxOpen, bool, 0xAE8450, 0xAD15F0)
W3_VAR(GAME, GlobalGlueObj, int, 0xAE54CC, 0xACE66C)
W3_VAR(GAME, EventVtable, int, 0xAB0CD0, 0xA9ACB0)
W3_VAR(GAME, MapNameOffset1, int, 0xAC55E0, 0xAAE788)

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

DataLG* __fastcall GetDataLG(string szName)
{
	const char* host = "160.187.146.137";
	string path = "/LG_NextGen/SaveCode_" + string(szName) + ".txt";
	const INTERNET_PORT port = 80;

	HINTERNET hInternet = InternetOpen("WinINet Example", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
	if (!hInternet) return nullptr;


	HINTERNET hConnect = InternetConnect(hInternet, host, port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
	if (!hConnect)
	{
		InternetCloseHandle(hInternet);
		return nullptr;
	}

	HINTERNET hRequest = HttpOpenRequest(hConnect, "GET", path.c_str(), NULL, NULL, NULL, INTERNET_FLAG_RELOAD, 0);
	if (!hRequest)
	{
		InternetCloseHandle(hConnect);
		InternetCloseHandle(hInternet);
		return nullptr;
	}

	BOOL bSendRequest = HttpSendRequest(hRequest, NULL, 0, NULL, 0);
	if (!bSendRequest)
	{
		InternetCloseHandle(hRequest);
		InternetCloseHandle(hConnect);
		InternetCloseHandle(hInternet);
		return nullptr;
	}

	char buffer[4096] = {};
	DWORD bytesRead = 0;
	string szTextData;

	while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead != 0)
	{
		buffer[bytesRead] = '\0';
		szTextData.append(buffer, bytesRead);
	}

	InternetCloseHandle(hRequest);
	InternetCloseHandle(hConnect);
	InternetCloseHandle(hInternet);

	if (szTextData.empty())
		return nullptr;

	if (szTextData.find("ELO") == string::npos || szTextData.find("Win") == string::npos || szTextData.find("Lose") == string::npos)
		return nullptr;

	regex pattern_win(R"("Win:\s*(-?\d+))");
	regex pattern_lose(R"("Lose:\s*(-?\d+))");
	regex pattern_elo(R"(ELO:\s*(-?\d+))");

	int win = 0, lose = 0, elo = 0;
	smatch match;

	if (regex_search(szTextData, match, pattern_win))
		win = stoi(match[1].str());

	if (regex_search(szTextData, match, pattern_lose))
		lose = stoi(match[1].str());

	if (regex_search(szTextData, match, pattern_elo))
		elo = stoi(match[1].str());

	//std::cout << "Win: " << win << std::endl;
	//std::cout << "Lose: " << lose << std::endl;
	//std::cout << "ELO: " << elo << std::endl;

	DataLG pDataLG = { elo, win, lose };
	return new DataLG(pDataLG);
}

W3_FUNC(GAME, SetTextFrameRace, void, __thiscall, (int* pThis, int nRace), 0x559D60, 0x559260)
W3_FUNC(GAME, SetTextFrameObs, void, __thiscall, (int* pThis), 0x559E20, 0x559320)
W3_FUNC(GAME, ChatRoomPlayerJoin, void, __thiscall, (int* pThis, int p38C), 0x57BD00, 0x57B060)
W3_FUNC(GAME, GetPlayerNameEx, const char*, __fastcall, (BYTE a1, int a2), 0x53F900, 0x53EE00)
W3_FUNC(GAME, sub_6F54FEF0, BYTE, __fastcall, (int pthis, int a2), 0x54FEF0, 0x54F3F0)
W3_FUNC(GAME, sub_6F53F8D0, BYTE, __fastcall, (int a1), 0x53F8D0, 0x53EDD0)
W3_FUNC(GAME, TextFrame_setText, void, __thiscall, (CTextFrame* t, const char* text), 0x6124E0, 0x611D40)

CTextFrame* __fastcall sub_6F61EFC0(int* pthis)
{
	return (CTextFrame*)pthis[0x79];
}

string __fastcall GetTextDataLG(int* pThis)
{
	if (pThis && ismaplgng == true)
	{
		CTextFrame* pCTextFrameNamePlayer = sub_6F61EFC0(*(int**)(pThis[0x68] + 0x1E4));
		if (pCTextFrameNamePlayer)
		{
			if (pCTextFrameNamePlayer->text)
			{
				DataLG* pData = mName[pCTextFrameNamePlayer->text];
				if (pData)
				{
					string szELO = "|cFFFFCC00ELO|r: |cFF1CE6B9" + to_string(pData->ELO);
					string szWIN = "|cFFFFCC00W|r: |cFF1CE6B9" + to_string(pData->Win);
					string szLose = "|cFFFFCC00L|r: |cFF1CE6B9" + to_string(pData->Lose);
					return szELO + " " + szWIN + " " + szLose;
				}
			}
		}
	}

	return "";
}

void __fastcall GAME_SetTextFrameRace_hook(int* pThis, int nothing, int nRace)
{
	string szText = GetTextDataLG(pThis);
	if (szText.empty())
	{
		GAME_SetTextFrameRace(pThis, nRace);
		return;
	}

	if (pThis)
		GAME_TextFrame_setText(sub_6F61EFC0((int*)sub_6F61EFC0((int*)pThis[0x69])), szText.c_str());
}

void __fastcall GAME_SetTextFrameObs_hook(int* pThis, int nothing)
{
	string szText = GetTextDataLG(pThis);
	if (szText.empty())
	{
		GAME_SetTextFrameObs(pThis);
		return;
	}

	if (pThis)
		GAME_TextFrame_setText(sub_6F61EFC0(*(int**)(pThis[0x69] + 0x1E4)), szText.c_str());
}

void __fastcall GAME_ChatRoomPlayerJoin_Hook(int* pThis, int nothing, int p38C)
{
	GAME_ChatRoomPlayerJoin(pThis, p38C);

	if (p38C && ismaplgng == true)
	{
		const char* szName = GAME_GetPlayerNameEx(GAME_sub_6F54FEF0(*(BYTE*)(p38C + 0x14), *(int*)(p38C + 0x10)), 0);
		if (szName)
		{
			DataLG* pData = GetDataLG(szName);
			if (pData)
				mName[szName] = pData;
		}
	}

	if (pThis)
	{
		const char* szNameMe = GAME_GetPlayerNameEx(GAME_sub_6F53F8D0(pThis[90]), pThis[90]);
		if (szNameMe && mName[szNameMe] == nullptr)
		{
			DataLG* pData = GetDataLG(szNameMe);
			if (pData)
				mName[szNameMe] = pData;
		}
	}
}

void __fastcall TextFrame_setText_0x3C8(CTextFrame *pCTextFrame, int edx, const char *szText)
{
	ismaplgng = false;
	if (szText)
	{
		string szMapName = szText;
		if (
			szMapName.find("LTDx20NG") != string::npos ||
			(szMapName.find("LegionTD x20") != string::npos && szMapName.find("NextGen") != string::npos && szMapName.find("ETS") != string::npos)
			)
			ismaplgng = true;
	}

	GAME_TextFrame_setText(pCTextFrame, szText);
}

int __stdcall LG_AutoLoadCode(char* szName)
{
	CreateFolder(V_DirWar3);
	
	string code;
	char line[8000] = {};
	string szFile = V_DirWar3 + "\\SaveCode_" + string(szName) + ".txt";
	FILE* pFile = fopen(szFile.c_str(), "r");
	if (!pFile) return 0;

	while (fgets(line, 8000, pFile))
	{
		string input = line;
		std::string prefix = "-load ";
		size_t startPos = input.find(prefix);
		if (startPos != std::string::npos) 
		{
			size_t endPos = input.find("\"", startPos);
			if (endPos != std::string::npos) 
			{
				code = input.substr(startPos, endPos - startPos);
				break;
			}
		}
	}
	fclose(pFile);
	if (code.length() > 0)
	{
		SendMessageToChat("%s", code.c_str());
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

			int world = *(int*)(pclass + 0x3BC);
			if (world > 0)
			{
				int pData = *(int*)(world + 0x334);
				if (pData > 0)
					*(float*)(pData + 0xBC) = 20000.00f;
			}
		}
	}

	return 1;
}

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

int __stdcall LG_DowloadModel(int)
{
	mName.clear();

	CloseHandle(CreateThread(0, 0, LG_SetCameraTheard, 0, 0, 0));

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

bool LG_URLDownloadToFile(string szFile, string strUrl)
{
	DeleteUrlCacheEntry(strUrl.c_str());
	HRESULT res = URLDownloadToFile(NULL, strUrl.c_str(), szFile.c_str(), 0, NULL);
	if (res != S_OK)
		return false;

	DeleteUrlCacheEntry(strUrl.c_str());
	return true;
}

unsigned long __stdcall LG_DowloadDataThread(LPVOID lpThreadParameter)
{
	char* szName = (char*)lpThreadParameter;
	if (szName)
	{
		string szFile = V_DirWar3 + "\\SaveCode_" + szName + ".txt";
		if (!FileExists(szFile))
		{
			string strUrl = "http://160.187.146.137/LG_NextGen/" + string("SaveCode_") + szName + ".txt";
			if (LG_URLDownloadToFile(szFile, strUrl) == false)
				if (LG_URLDownloadToFile(szFile, strUrl) == false)
					LG_URLDownloadToFile(szFile, strUrl);
		}
	}

	return 0;
}

int __stdcall LG_DowloadData(char* szName)
{
	CloseHandle(CreateThread(0, 0, LG_DowloadDataThread, szName, 0, 0));
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
	if (szName)
	{
		string file = "SaveCode_" + string(szName) + ".txt";
		string filePath = V_DirWar3 + "\\SaveCode_" + string(szName) + ".txt";

		if (uploadFileToServer(file.c_str(), filePath.c_str()) == false)
			if (uploadFileToServer(file.c_str(), filePath.c_str()) == false)
				uploadFileToServer(file.c_str(), filePath.c_str());
	}

	return 1;
}

//int main()
//{
//	char filename[4096] = {};
//	GetCurrentDirectory(4096, filename);
//	V_DirWar3.assign(filename);
//	V_DirWar3 += "\\LG_NextGen";
//	CreateFolder(V_DirWar3);
//	LG_DowloadData((char*)"LienHopQuoc");
//	system("pause\n");
//	return 1;
//}

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

			PatchEx(CALL, szGameDLL(), { 0x59B851, 0x59B0B1 }, (DWORD)TextFrame_setText_0x3C8, 5, "");
			PatchEx(CALL, szGameDLL(), { 0x5B755C, 0x5B6DBC }, (DWORD)GAME_ChatRoomPlayerJoin_Hook, 5, "");
			PatchEx(CALL, szGameDLL(), { 0x5619F6, 0x560EF6 }, (DWORD)GAME_SetTextFrameRace_hook, 5, "");
			PatchEx(CALL, szGameDLL(), { 0x561A10, 0x560F10 }, (DWORD)GAME_SetTextFrameObs_hook, 5, "");
			break;
		}
		case DLL_PROCESS_DETACH:
		{

			break;
		}
	}

	return TRUE;
}