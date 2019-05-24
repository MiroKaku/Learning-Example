#include <stdio.h>
#include <Windows.h>
#include <psapi.h>
#include <DbgHelp.h>
#pragma comment(lib, "ImageHlp.lib")

#include "Native.h"


auto NtSetInformationProcess = (sdkext::$NtSetInformationProcess)GetProcAddress(
    GetModuleHandleA("ntdll"), "NtSetInformationProcess");

auto RtlRandomEx = (sdkext::$RtlRandomEx)GetProcAddress(
    GetModuleHandleA("ntdll"), "RtlRandomEx");

auto RtlGetVersion = (sdkext::$RtlGetVersion)GetProcAddress(
    GetModuleHandleA("ntdll"), "RtlGetVersion");


extern"C"
{
    void InstrumentationCallbackShim();
    void InstrumentationCallback(void* aReturnAddress, size_t aReturnValue);
}


extern"C" 
void InstrumentationCallback(void* aReturnAddress, size_t aReturnValue)
{
    thread_local volatile bool t_state = false;
    thread_local volatile char t_buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = { 0 };

    if (!t_state)
    {
        t_state = true;

        RtlSecureZeroMemory((void*)t_buffer, sizeof(t_buffer));
        
        auto vSymbol            = static_cast<PSYMBOL_INFO>((void*)t_buffer);
        vSymbol->SizeOfStruct   = sizeof(SYMBOL_INFO);
        vSymbol->MaxNameLen     = MAX_SYM_NAME;        
        if (SymFromAddr(GetCurrentProcess(), (DWORD64)aReturnAddress, nullptr, vSymbol))
        {
            printf("%p : %s, Return : 0x%IX \n", aReturnAddress, vSymbol->Name, aReturnValue);
        }

        t_state = false;
    }
}

bool IsWindows10OrGreater() 
{
    if (!RtlGetVersion)
    {
        return false;
    }

    auto vOSVI = RTL_OSVERSIONINFOW{ sizeof(RTL_OSVERSIONINFOW) };
    if (RtlGetVersion(&vOSVI) != 0) 
    {
        return false;
    }

    return vOSVI.dwMajorVersion >= 10;
}

void RaiseToDebug()
{
    HANDLE vToken = nullptr;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &vToken))
    {
        TOKEN_PRIVILEGES vTokenPrivileges = TOKEN_PRIVILEGES();
        if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &vTokenPrivileges.Privileges[0].Luid))
        {
            vTokenPrivileges.PrivilegeCount              = 1;
            vTokenPrivileges.Privileges[0].Attributes    = SE_PRIVILEGE_ENABLED;

            AdjustTokenPrivileges(vToken, FALSE, &vTokenPrivileges, 0, nullptr, 0);
        }
        CloseHandle(vToken);
    }
}

int main(int /*argc*/, char* /*argv*/[])
{
    (void)getchar();

    SymSetOptions(SYMOPT_UNDNAME);
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);

    auto vCommonArg = sdkext::PROCESS_INSTRUMENTATION_CALLBACK_INFORMATION
    {
        InstrumentationCallbackShim
    };
    auto vWin10Arg  = sdkext::PROCESS_INSTRUMENTATION_CALLBACK_INFORMATION_EX
    {
        0, 0,
        InstrumentationCallbackShim
    };

    auto vInformation       = (void*)nullptr;
    auto vInformationLength = 0u;

    if (IsWindows10OrGreater())
    {
        vInformation        = &vWin10Arg;
        vInformationLength  = sizeof(vWin10Arg);
    }
    else
    {
        RaiseToDebug();

        vInformation        = &vCommonArg;
        vInformationLength  = sizeof(vCommonArg);
    }

    // Enable
    NtSetInformationProcess(
        GetCurrentProcess(),
        sdkext::PROCESSINFOCLASS::ProcessInstrumentationCallback,
        vInformation,
        vInformationLength);

    // Do anything
    DWORD   vProcesses[1024] = { 0 };
    DWORD   vNeeded          = 0ul;
    if (!EnumProcesses(vProcesses, sizeof(vProcesses), &vNeeded))
    {
        return 1;
    }

    // We can not hook RtlRandomEx, it does not crosses kernel
    auto vSeed       = 1ul;
    (void)RtlRandomEx(&vSeed);
    
    // Disable
    RtlSecureZeroMemory(vInformation, vInformationLength);
    NtSetInformationProcess(
        GetCurrentProcess(),
        sdkext::PROCESSINFOCLASS::ProcessInstrumentationCallback,
        vInformation,
        vInformationLength);

    (void)getchar();
    return 0;
}
