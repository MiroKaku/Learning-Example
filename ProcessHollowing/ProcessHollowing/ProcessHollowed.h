#pragma once


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
    LPPROCESS_INFORMATION   aProcessInformation);
