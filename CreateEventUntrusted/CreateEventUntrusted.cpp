#include <Windows.h>
#include <AclAPI.h>
#include <malloc.h>
#include <stdio.h>


namespace Main
{
    HRESULT CreateEventUntrusted(_Out_ PHANDLE Handle)
    {
        HRESULT Result = S_OK;

        PACL  Dacl = nullptr;
        PACL  Sacl = nullptr;

        *Handle = nullptr;

        do
        {
            ULONG Bytes = MAX_SID_SIZE;
            PSID  UntrustedSid = (PSID)_malloca(MAX_SID_SIZE);
            PSID  EveryoneSid = (PSID)_malloca(MAX_SID_SIZE);

            EXPLICIT_ACCESS ExplicitAccess[1]{};
            SECURITY_DESCRIPTOR SecurityDescriptor{};

            // Create a well-known SID for the Everyone group.
            if (!CreateWellKnownSid(WinWorldSid, 0, EveryoneSid, &Bytes))
            {
                Result = HRESULT_FROM_WIN32(GetLastError());
                break;
            }

            // Or 
            //PSID EveryoneSid = nullptr;
            //SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;
            //if (!AllocateAndInitializeSid(
            //    &SIDAuthWorld,
            //    1,
            //    SECURITY_WORLD_RID,
            //    0, 0, 0, 0, 0, 0, 0,
            //    &EveryoneSid))
            //{
            //    Result = HRESULT_FROM_WIN32(GetLastError());
            //    break;
            //}

            // Create a SID for the Untrusted Mandatory Level.
            if (!CreateWellKnownSid(WinUntrustedLabelSid, 0, UntrustedSid, &Bytes))
            {
                Result = HRESULT_FROM_WIN32(GetLastError());
                break;
            }

            ExplicitAccess[0].grfAccessPermissions = EVENT_ALL_ACCESS; // <<< Object Access
            ExplicitAccess[0].grfAccessMode = SET_ACCESS;
            ExplicitAccess[0].grfInheritance = NO_INHERITANCE;
            ExplicitAccess[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
            ExplicitAccess[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
            ExplicitAccess[0].Trustee.ptstrName = (LPTSTR)EveryoneSid;

            Result = HRESULT_FROM_WIN32(SetEntriesInAcl(_countof(ExplicitAccess), ExplicitAccess, nullptr, &Dacl));
            if (FAILED(Result))
            {
                break;
            }

            Bytes = sizeof(ACL) + sizeof(ACE_HEADER) + sizeof(ACCESS_MASK) + GetLengthSid(UntrustedSid);
            Bytes = (Bytes + (sizeof(DWORD) - 1)) & 0xfffffffc; // Align

            Sacl = (PACL)LocalAlloc(LPTR, Bytes);
            if (Sacl == nullptr)
            {
                Result = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
                break;
            }

            InitializeAcl(Sacl, Bytes, ACL_REVISION);

            if (!AddMandatoryAce(Sacl, ACL_REVISION, 0, 0, UntrustedSid))
            {
                Result = HRESULT_FROM_WIN32(GetLastError());
                break;
            }

            if (!InitializeSecurityDescriptor(&SecurityDescriptor, SECURITY_DESCRIPTOR_REVISION))
            {
                Result = HRESULT_FROM_WIN32(GetLastError());
                break;
            }

            if (!SetSecurityDescriptorSacl(&SecurityDescriptor, TRUE, Sacl, FALSE))
            {
                Result = HRESULT_FROM_WIN32(GetLastError());
                break;
            }

            if (!SetSecurityDescriptorDacl(&SecurityDescriptor, TRUE, Dacl, FALSE))
            {
                Result = HRESULT_FROM_WIN32(GetLastError());
                break;
            }

            SECURITY_ATTRIBUTES SecurityAttributes{};
            SecurityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
            SecurityAttributes.bInheritHandle = FALSE;
            SecurityAttributes.lpSecurityDescriptor = &SecurityDescriptor;

            *Handle = CreateEvent(&SecurityAttributes, FALSE, FALSE, L"UntrustedEvent");
            if (*Handle == nullptr)
            {
                Result = HRESULT_FROM_WIN32(GetLastError());
                break;
            }

        } while (false);

        if (Sacl) LocalFree(Sacl);
        if (Dacl) LocalFree(Dacl);

        return Result;
    }

    extern"C" int main()
    {
        HRESULT Result = S_OK;
        HANDLE  Handle = nullptr;

        Result = CreateEventUntrusted(&Handle);
        if (FAILED(Result))
        {
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
