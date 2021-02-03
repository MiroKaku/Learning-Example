#include <Windows.h>
#include <AclAPI.h>
#include <malloc.h>
#include <stdio.h>


namespace Main
{
    HRESULT SetProcessUntrusted(HANDLE hProcess)
    {
        HRESULT Result = NOERROR;
        ULONG   Bytes  = MAX_SID_SIZE;
        HANDLE  Token  = nullptr;

        TOKEN_MANDATORY_LABEL MandatoryLabel = { { (PSID)_malloca(MAX_SID_SIZE), SE_GROUP_INTEGRITY } };

        if (!CreateWellKnownSid(WinUntrustedLabelSid, nullptr, MandatoryLabel.Label.Sid, &Bytes) ||
            !OpenProcessToken(hProcess, TOKEN_ADJUST_DEFAULT, &Token))
        {
            Result = HRESULT_FROM_WIN32(GetLastError());
            return Result;
        }

        if (!SetTokenInformation(Token, TokenIntegrityLevel, &MandatoryLabel, sizeof(MandatoryLabel)))
        {
            Result = HRESULT_FROM_WIN32(GetLastError());
        }

        CloseHandle(Token);

        return Result;
    }

    extern"C" int main()
    {
        HRESULT Result = S_OK;
        HANDLE  Handle = nullptr;

        SetProcessUntrusted(GetCurrentProcess());

        Handle = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"UntrustedEvent");
        if (Handle == nullptr)
        {
            Result = HRESULT_FROM_WIN32(GetLastError());

            printf(__FUNCTION__ "() CreateEventUntrusted() Error: 0x%08X\n",
                Result);

            (void)getchar();
            return Result;
        }

        printf("Untrusted Event: %p\n", Handle);
        printf("Wait any key...\n");

        (void)getchar();

        CloseHandle(Handle);

        return Result;
    }
}
