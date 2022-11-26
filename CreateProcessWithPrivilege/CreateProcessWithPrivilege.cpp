#define WIN32_LEAN_AND_MEAN
#define UMDF_USING_NTSTATUS

#include <Windows.h>

#include <Objbase.h>
#include <shellapi.h>

#include <WinSafer.h>
#include <wtsapi32.h>
#pragma comment(lib, "Wtsapi32.lib")


HRESULT SetTokenPrivilege(
    _In_ HANDLE  token,      // access token handle
    _In_ LPCWSTR privilege,  // name of privilege to enable/disable
    _In_ BOOL    enable      // to enable or disable privilege
)
{
    TOKEN_PRIVILEGES TokenPrivileges = { 0 };

    TokenPrivileges.PrivilegeCount = 1;
    TokenPrivileges.Privileges[0].Attributes = enable ? SE_PRIVILEGE_ENABLED : 0;

    if (!::LookupPrivilegeValue(
        nullptr,
        privilege,
        &TokenPrivileges.Privileges[0].Luid))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Enable the privilege or disable all privileges.
    ::AdjustTokenPrivileges(
        token,
        FALSE,
        &TokenPrivileges,
        sizeof TOKEN_PRIVILEGES,
        nullptr,
        nullptr);

    return HRESULT_FROM_WIN32(GetLastError());
}

HRESULT SetProcessPrivilege(
    _In_  HANDLE  process,    // access process handle
    _In_  LPCWSTR privilege,  // name of privilege to enable/disable
    _In_  BOOL    enable      // to enable or disable privilege
)
{
    HRESULT Result = S_OK;
    HANDLE  Token  = nullptr;

    if (!::OpenProcessToken(process, TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &Token))
    {
        Result = HRESULT_FROM_WIN32(GetLastError());
        return Result;
    }

    Result = SetTokenPrivilege(Token, privilege, enable);

    if (Token)
    {
        ::CloseHandle(Token);
    }

    return Result;
}

BOOLEAN IsProcessElevated(
    _In_ HANDLE process
)
{
    BOOLEAN Result = FALSE;
    HANDLE  Token  = nullptr;

    do
    {
        DWORD ReturnLength = 0;

        if (!::OpenProcessToken(process, TOKEN_QUERY, &Token))
        {
            break;
        }

        // Unlike TOKEN_ELEVATION_TYPE which returns TokenElevationTypeDefault when
        // UAC is turned off, TOKEN_ELEVATION returns whether the process is elevated.

        TOKEN_ELEVATION elevation = { 0 };
        if (!::GetTokenInformation(Token, TokenElevation, &elevation, sizeof(elevation), &ReturnLength))
        {
            break;
        }

        Result = !!elevation.TokenIsElevated;

    } while (false);

    if (Token)
    {
        ::CloseHandle(Token);
    }

    return Result;
}


enum class IntegrityLevel {
    INTEGRITY_UNKNOWN,
    UNTRUSTED_INTEGRITY,
    LOW_INTEGRITY,
    MEDIUM_INTEGRITY,
    MEDIUM_PLUS_INTEGRITY,
    HIGH_INTEGRITY,
    SYSTEM_INTEGRITY,
    PROTECTED_PROCESS_INTEGRITY,
};

IntegrityLevel GetProcessIntegrityLevel(HANDLE process)
{
    HANDLE          Token = nullptr;
    IntegrityLevel  Level = IntegrityLevel::INTEGRITY_UNKNOWN;
    void* TokenLabelBytes = nullptr;

    do
    {
        if (!::OpenProcessToken(process, TOKEN_QUERY, &Token))
        {
            Level = IntegrityLevel::INTEGRITY_UNKNOWN;
            break;
        }

        DWORD ReturnLength = 0ul;
        if (::GetTokenInformation(Token, TokenIntegrityLevel, nullptr, 0, &ReturnLength) ||
            ::GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        {
            Level = IntegrityLevel::INTEGRITY_UNKNOWN;
            break;
        }

        TokenLabelBytes = malloc(ReturnLength);
        auto TokenLabel = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(TokenLabelBytes);
        if (TokenLabel == nullptr)
        {
            Level = IntegrityLevel::INTEGRITY_UNKNOWN;
            break;
        }


        if (!::GetTokenInformation(Token, TokenIntegrityLevel, TokenLabel, ReturnLength, &ReturnLength))
        {
            Level = IntegrityLevel::INTEGRITY_UNKNOWN;
            break;
        }

        DWORD LevelValue = *::GetSidSubAuthority(
            TokenLabel->Label.Sid,
            static_cast<DWORD>(*::GetSidSubAuthorityCount(TokenLabel->Label.Sid) - 1));

        if (LevelValue < SECURITY_MANDATORY_LOW_RID)
        {
            Level = IntegrityLevel::UNTRUSTED_INTEGRITY;
            break;
        }

        if (LevelValue < SECURITY_MANDATORY_MEDIUM_RID)
        {
            Level = IntegrityLevel::LOW_INTEGRITY;
            break;
        }

        if (LevelValue < SECURITY_MANDATORY_MEDIUM_PLUS_RID)
        {
            Level = IntegrityLevel::MEDIUM_INTEGRITY;
            break;
        }

        if (LevelValue < SECURITY_MANDATORY_HIGH_RID)
        {
            Level = IntegrityLevel::MEDIUM_PLUS_INTEGRITY;
            break;
        }

        if (LevelValue < SECURITY_MANDATORY_SYSTEM_RID)
        {
            Level = IntegrityLevel::HIGH_INTEGRITY;
            break;
        }

        if (LevelValue < SECURITY_MANDATORY_PROTECTED_PROCESS_RID)
        {
            Level = IntegrityLevel::SYSTEM_INTEGRITY;
            break;
        }
        else
        {
            Level = IntegrityLevel::PROTECTED_PROCESS_INTEGRITY;
            break;
        }

    } while (false);

    if (Token)
    {
        CloseHandle(Token);
    }

    if (TokenLabelBytes)
    {
        free(TokenLabelBytes);
    }

    return Level;
}

HRESULT CreateSystemToken(
    _In_  DWORD   access,
    _Out_ PHANDLE token
)
{
    HRESULT Result = S_OK;
    HANDLE SystemProcessHandle = nullptr;
    HANDLE SystemTokenHandle = nullptr;

    do
    {
        auto SessionID = WTSGetActiveConsoleSessionId();
        if (SessionID == -1)
        {
            Result = HRESULT_FROM_WIN32(GetLastError());
            break;
        }

        DWORD Count = 0;
        PWTS_PROCESS_INFO ProcessesInformation = nullptr;

        if (!WTSEnumerateProcesses(WTS_CURRENT_SERVER_HANDLE, 0, 1, &ProcessesInformation, &Count))
        {
            Result = HRESULT_FROM_WIN32(GetLastError());
            break;
        }

        DWORD LsassPID = 0;
        DWORD WinLogonPID = 0;

        for (size_t i = 0; i < Count; ++i)
        {
            auto& ProcessInformation = ProcessesInformation[i];

            if ((!ProcessInformation.pProcessName) ||
                (!ProcessInformation.pUserSid) ||
                (!::IsWellKnownSid(ProcessInformation.pUserSid, WELL_KNOWN_SID_TYPE::WinLocalSystemSid)))
            {
                continue;
            }

            if ((0 == LsassPID) &&
                (0 == ProcessInformation.SessionId) &&
                (0 == ::_wcsicmp(L"lsass.exe", ProcessInformation.pProcessName)))
            {
                LsassPID = ProcessInformation.ProcessId;
                continue;
            }

            if ((0 == WinLogonPID) &&
                (SessionID == ProcessInformation.SessionId) &&
                (0 == ::_wcsicmp(L"winlogon.exe", ProcessInformation.pProcessName)))
            {
                WinLogonPID = ProcessInformation.ProcessId;
                continue;
            }

            if (WinLogonPID && LsassPID)
            {
                break;
            }
        }

        WTSFreeMemory(ProcessesInformation);

        SystemProcessHandle = ::OpenProcess(
            PROCESS_QUERY_INFORMATION,
            FALSE,
            LsassPID);
        if (!SystemProcessHandle)
        {
            SystemProcessHandle = ::OpenProcess(
                PROCESS_QUERY_INFORMATION,
                FALSE,
                WinLogonPID);
        }

        if (SystemProcessHandle == nullptr)
        {
            Result = HRESULT_FROM_WIN32(GetLastError());
            break;
        }

        if (!::OpenProcessToken(
            SystemProcessHandle,
            TOKEN_DUPLICATE,
            &SystemTokenHandle))
        {
            Result = HRESULT_FROM_WIN32(GetLastError());
            break;
        }

        if (!::DuplicateTokenEx(
            SystemTokenHandle,
            access,
            nullptr,
            SecurityIdentification,
            TokenPrimary,
            token))
        {
            Result = HRESULT_FROM_WIN32(GetLastError());
            break;
        }

    } while (false);

    if (SystemTokenHandle)
    {
        ::CloseHandle(SystemTokenHandle);
    }

    if (SystemProcessHandle)
    {
        ::CloseHandle(SystemProcessHandle);
    }

    return Result;
}

HRESULT CreateSessionToken(
    _In_  DWORD   session_id,
    _Out_ PHANDLE token
)
{
    if (!WTSQueryUserToken(session_id, token))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    return S_OK;
}

HRESULT CreateProcessWithPrivilege(
    _In_opt_ LPCWSTR ApplicationName,
    _Inout_opt_ LPWSTR CommandLine,
    _In_opt_ LPSECURITY_ATTRIBUTES ProcessAttributes,
    _In_opt_ LPSECURITY_ATTRIBUTES ThreadAttributes,
    _In_ BOOL InheritHandles,
    _In_ DWORD CreationFlags,
    _In_opt_ LPVOID Environment,
    _In_opt_ LPCWSTR CurrentDirectory,
    _In_ LPSTARTUPINFOW StartupInfo,
    _Out_ LPPROCESS_INFORMATION ProcessInformation
)
{
    HRESULT Result      = S_OK;
    HANDLE  UserToken   = nullptr;
    HANDLE  SysTokenPrimary         = nullptr;
    HANDLE  SysTokenImpersonation   = nullptr;

    do
    {
        Result = CreateSystemToken(MAXIMUM_ALLOWED, &SysTokenPrimary);
        if (FAILED(Result))
        {
            break;
        }

        if (!DuplicateTokenEx(SysTokenPrimary, MAXIMUM_ALLOWED, nullptr,
            DEFAULT_IMPERSONATION_LEVEL, TokenImpersonation, &SysTokenImpersonation))
        {
            Result = HRESULT_FROM_WIN32(GetLastError());
            break;
        }

        // For get user token
        Result = SetTokenPrivilege(SysTokenImpersonation, SE_TCB_NAME, TRUE);
        if (FAILED(Result))
        {
            break;
        }

        // For calling CreateProcessAsUser
        Result = SetTokenPrivilege(SysTokenImpersonation, SE_ASSIGNPRIMARYTOKEN_NAME, TRUE);
        if (FAILED(Result))
        {
            break;
        }

        if (!SetThreadToken(nullptr, SysTokenImpersonation))
        {
            Result = HRESULT_FROM_WIN32(GetLastError());
            break;
        }

        Result = CreateSessionToken(WTSGetActiveConsoleSessionId(), &UserToken);
        if (FAILED(Result))
        {
            break;
        }

        if (!CreateProcessAsUser(UserToken, ApplicationName, CommandLine, ProcessAttributes, ThreadAttributes,
            InheritHandles, CreationFlags | CREATE_NEW_CONSOLE, Environment, CurrentDirectory, StartupInfo, ProcessInformation))
        {
            Result = HRESULT_FROM_WIN32(GetLastError());
            break;
        }

    } while (false);

    if (UserToken)
    {
        CloseHandle(UserToken);
    }

    if (SysTokenImpersonation)
    {
        RevertToSelf();
        CloseHandle(SysTokenImpersonation);
    }

    if (SysTokenPrimary)
    {
        CloseHandle(SysTokenPrimary);
    }

    return Result;
}

int main()
{
    HRESULT Result = S_OK;

    do
    {
        // Adjust to administrator
        if (!IsProcessElevated(GetCurrentProcess()))
        {
            wchar_t SelfPath[MAX_PATH]{};
            if (!GetModuleFileNameW(nullptr, SelfPath, _countof(SelfPath)))
            {
                Result = HRESULT_FROM_WIN32(GetLastError());
                break;
            }

            SHELLEXECUTEINFO ShellExec{ sizeof SHELLEXECUTEINFO };
            ShellExec.lpVerb = L"runas";
            ShellExec.lpFile = SelfPath;
            ShellExec.lpParameters = GetCommandLine();

            Result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
            if (Result == RPC_E_CHANGED_MODE) {
                Result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            }

            /* S_FALSE means success, but someone else already initialized. */
            /* You still need to call CoUninitialize in this case! */
            if (Result == S_FALSE) {
                Result = S_OK;
            }

            if (FAILED(Result)) {

                break;
            }

            if (!ShellExecuteEx(&ShellExec))
            {
                Result = HRESULT_FROM_WIN32(GetLastError());
            }

            break;
        }

        // For enumerate service processes (calling CreateSystemToken)
        Result = SetProcessPrivilege(GetCurrentProcess(), SE_DEBUG_NAME, TRUE);
        if (FAILED(Result))
        {
            break;
        }

        wchar_t CommandLine[] = LR"(C:\Program Files\Windows NT\Accessories\wordpad.exe)";

        STARTUPINFO         StartupConfig  = { sizeof(STARTUPINFO) };
        PROCESS_INFORMATION ProcessHandles = { nullptr };

        Result = CreateProcessWithPrivilege(
            nullptr,
            CommandLine,
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            nullptr,
            &StartupConfig,
            &ProcessHandles);
        if (FAILED(Result))
        {
            break;
        }

        CloseHandle(ProcessHandles.hThread);
        CloseHandle(ProcessHandles.hProcess);

    } while (false);

    return Result;
}
