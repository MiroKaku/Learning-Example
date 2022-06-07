#include "stdafx.h"


template<typename T, typename D>
inline auto scope_guard(T* aValue, D aDeleter)
{
    return std::unique_ptr<T, D>(aValue, aDeleter);
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

IntegrityLevel GetProcessIntegrityLevel(HANDLE process) {
    HANDLE process_token = nullptr;

    if (!::OpenProcessToken(process, TOKEN_QUERY, &process_token)) {
        return IntegrityLevel::INTEGRITY_UNKNOWN;
    }

    auto process_token_guard = scope_guard(process_token,
        [](HANDLE token) { CloseHandle(token); });

    DWORD token_info_length = 0;
    if (::GetTokenInformation(process_token, TokenIntegrityLevel, nullptr, 0, &token_info_length) ||
        ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return IntegrityLevel::INTEGRITY_UNKNOWN;
    }

    auto token_label_bytes = std::make_unique<char[]>(token_info_length);
    auto token_label = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(token_label_bytes.get());
    if (!::GetTokenInformation(process_token, TokenIntegrityLevel, token_label,
        token_info_length, &token_info_length)) {
        return IntegrityLevel::INTEGRITY_UNKNOWN;
    }

    DWORD integrity_level = *::GetSidSubAuthority(
        token_label->Label.Sid,
        static_cast<DWORD>(*::GetSidSubAuthorityCount(token_label->Label.Sid) - 1));

    if (integrity_level < SECURITY_MANDATORY_LOW_RID)
        return IntegrityLevel::UNTRUSTED_INTEGRITY;

    if (integrity_level < SECURITY_MANDATORY_MEDIUM_RID)
        return IntegrityLevel::LOW_INTEGRITY;

    if (integrity_level < SECURITY_MANDATORY_MEDIUM_PLUS_RID)
        return IntegrityLevel::MEDIUM_INTEGRITY;

    if (integrity_level < SECURITY_MANDATORY_HIGH_RID)
        return IntegrityLevel::MEDIUM_PLUS_INTEGRITY;

    if (integrity_level < SECURITY_MANDATORY_SYSTEM_RID)
        return IntegrityLevel::HIGH_INTEGRITY;

    if (integrity_level < SECURITY_MANDATORY_PROTECTED_PROCESS_RID)
        return IntegrityLevel::SYSTEM_INTEGRITY;
    else
        return IntegrityLevel::PROTECTED_PROCESS_INTEGRITY;

    return IntegrityLevel::INTEGRITY_UNKNOWN;
}

bool IsProcessElevated(HANDLE process) {
    HANDLE process_token = nullptr;

    if (!::OpenProcessToken(process, TOKEN_QUERY, &process_token)) {
        return false;
    }

    auto process_token_guard = scope_guard(process_token,
        [](HANDLE token) { CloseHandle(token); });

    // Unlike TOKEN_ELEVATION_TYPE which returns TokenElevationTypeDefault when
    // UAC is turned off, TOKEN_ELEVATION returns whether the process is elevated.
    DWORD size = 0;
    TOKEN_ELEVATION elevation = { 0 };
    if (!GetTokenInformation(process_token, TokenElevation, &elevation,
        sizeof(elevation), &size)) {
        return false;
    }
    return !!elevation.TokenIsElevated;
}

bool SetPrivilege(
    HANDLE  token,      // access token handle
    LPCWSTR privilege,  // name of privilege to enable/disable
    BOOL    enable      // to enable or disable privilege
)
{
    TOKEN_PRIVILEGES tp = { 0 };
    LUID luid = { 0 };

    if (!::LookupPrivilegeValue(
        nullptr,        // lookup privilege on local system
        privilege,      // privilege to lookup 
        &luid))         // receives LUID of privilege
    {
        return false;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    if (enable)
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    else
        tp.Privileges[0].Attributes = 0;

    // Enable the privilege or disable all privileges.
    if (!::AdjustTokenPrivileges(
        token,
        FALSE,
        &tp,
        sizeof(TOKEN_PRIVILEGES),
        nullptr,
        nullptr))
    {
        return false;
    }

    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)
    {
        return false;
    }

    return true;
}

BOOL ServiceCreateProcessAsPrivilege(
    _In_opt_ BOOL bAdministrator,
    _In_opt_ LPCWSTR lpApplicationName,
    _Inout_opt_ LPWSTR lpCommandLine,
    _In_opt_ LPSECURITY_ATTRIBUTES lpProcessAttributes,
    _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
    _In_ BOOL bInheritHandles,
    _In_ DWORD dwCreationFlags,
    _In_opt_ LPVOID /*lpEnvironment*/,
    _In_opt_ LPCWSTR lpCurrentDirectory,
    _In_ LPSTARTUPINFOW lpStartupInfo,
    _Out_ LPPROCESS_INFORMATION lpProcessInformation
)
{
    DWORD vResult = S_OK;

    for (;;)
    {
        DWORD vSessionCount = 0u;
        PWTS_SESSION_INFO vSessionInfos = nullptr;

        if (!WTSEnumerateSessions(
            WTS_CURRENT_SERVER_HANDLE,
            0, 1,
            &vSessionInfos,
            &vSessionCount))
        {
            vResult = GetLastError();
            break;
        }

        auto vSessionInfosGuard = scope_guard(vSessionInfos,
            [](PWTS_SESSION_INFO aSessionInfos) { WTSFreeMemory(aSessionInfos); });

        // Query User Token
        HANDLE vNewToken = nullptr;
        for (auto i = 0u; i < vSessionCount; ++i)
        {
            if (vSessionInfos[i].State != WTSActive)
            {
                continue;
            }

            HANDLE vUserToken = nullptr;
            if (!WTSQueryUserToken(vSessionInfos[i].SessionId, &vUserToken))
            {
                // Need SeTcbPrivilege

                vResult = GetLastError();
                continue;
            }

            auto vUserTokenGuard = scope_guard(vUserToken,
                [](HANDLE aHandle) { CloseHandle(aHandle); });

            if (bAdministrator)
            {
                DWORD vReturnLength = 0u;
                TOKEN_LINKED_TOKEN vLinkedToken = { nullptr };
                if (!GetTokenInformation(
                    vUserToken,
                    TokenLinkedToken,
                    &vLinkedToken, sizeof(vLinkedToken),
                    &vReturnLength))
                {
                    if (!DuplicateTokenEx(
                        vUserToken,
                        MAXIMUM_ALLOWED,
                        nullptr,
                        SecurityIdentification,
                        TokenPrimary,
                        &vLinkedToken.LinkedToken))
                    {
                        vResult = GetLastError();
                        continue;
                    }
                }

                vNewToken = vLinkedToken.LinkedToken;
            }
            else
            {
                vNewToken = vUserTokenGuard.release();
            }

            break;
        }
        if (vNewToken == nullptr)
        {
            if (vResult == ERROR_SUCCESS)
            {
                vResult = ERROR_NO_TOKEN;
            }
            break;
        }

        auto vNewTokenGuard = scope_guard(vNewToken,
            [](HANDLE aHandle) { CloseHandle(aHandle); });

        vResult = ERROR_SUCCESS;

        // Create User Environment Block
        LPVOID vEnvironmentBlock = nullptr;
        if (!CreateEnvironmentBlock(&vEnvironmentBlock, vNewToken, TRUE))
        {
            vResult = GetLastError();
            break;
        }

        auto vEnvironmentBlockGuard = scope_guard(vEnvironmentBlock,
            [](LPVOID aEnvironmentBlock) { DestroyEnvironmentBlock(aEnvironmentBlock); });

        // Create Process
        if (!CreateProcessAsUser(
            vNewToken,
            lpApplicationName,
            lpCommandLine, 
            lpProcessAttributes, 
            lpThreadAttributes,
            bInheritHandles,
            dwCreationFlags | CREATE_NEW_CONSOLE | CREATE_UNICODE_ENVIRONMENT,
            vEnvironmentBlock,
            lpCurrentDirectory,
            lpStartupInfo,
            lpProcessInformation))
        {
            vResult = GetLastError();
            break;
        }

        break;
    }

    return SetLastError(vResult), vResult == ERROR_SUCCESS;
}

BOOL ServiceForkProcessAsPrivilege(
    _In_opt_ BOOL bAdministrator
)
{
    // Current Directory
    WCHAR vCurrentDirectory[MAX_PATH] = { 0 };
    GetCurrentDirectory(_countof(vCurrentDirectory), vCurrentDirectory);

    // Startup Info
    STARTUPINFO vStartupInfo = { sizeof(STARTUPINFO) };
    GetStartupInfo(&vStartupInfo);

    // Process Info
    PROCESS_INFORMATION vProcessInfo = { nullptr };

    // Fork
    if (ServiceCreateProcessAsPrivilege(
        bAdministrator,
        nullptr,
        GetCommandLineW(),
        nullptr,
        nullptr,
        TRUE,
        0,
        nullptr,
        vCurrentDirectory,
        &vStartupInfo,
        &vProcessInfo))
    {
        CloseHandle(vProcessInfo.hThread);
        CloseHandle(vProcessInfo.hProcess);

        return SetLastError(ERROR_SUCCESS), TRUE;
    }

    return FALSE;
}

int main()
{
    auto vResult = ERROR_SUCCESS;

    if (GetProcessIntegrityLevel(GetCurrentProcess()) == IntegrityLevel::SYSTEM_INTEGRITY)
    {
        if (!ServiceForkProcessAsPrivilege(TRUE))
        {
            printf("ServiceForkProcessAsPrivilege() failed! Result:0x%08X\n", GetLastError());
        }
    }
    else
    {
        wchar_t cmd[] = LR"(C:\Windows\System32\notepad.exe)";

        STARTUPINFO vStartInfo = { sizeof(STARTUPINFO) };
        PROCESS_INFORMATION vProcessInfo = { nullptr };

        if (CreateProcess(nullptr,
            cmd,
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            nullptr,
            &vStartInfo,
            &vProcessInfo))
        {
            CloseHandle(vProcessInfo.hThread);
            CloseHandle(vProcessInfo.hProcess);
        }
    }

    system("whoami && pause");
    return vResult;
}
