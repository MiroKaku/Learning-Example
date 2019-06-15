#pragma once

#define POOL_NX_OPTIN 1

#include <stddef.h>
#include <stdlib.h>
#include <ntddk.h>
#include <wdm.h>
#include <ntstrsafe.h>


#include "iocpctl.h"


inline auto __cdecl DebugPrint(ULONG aLevel, PCSTR aFormat, ...) -> ULONG
{
    auto vResult    = 0ul;
    auto vArgs      = va_list();

    va_start(vArgs, aFormat);
    vResult = vDbgPrintExWithPrefix("[IOCPCTL] ", DPFLTR_IHVDRIVER_ID, aLevel, aFormat, vArgs);
    va_end(vArgs);

    return vResult;
}
