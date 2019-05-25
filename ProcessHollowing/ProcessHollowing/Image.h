#pragma once
#include <ImageHlp.h>


namespace image
{

    template<typename T = IMAGE_NT_HEADERS>
    DWORD FileImageNtHeader(
        HANDLE aFileHandle,
        T* aNtHeader)
    {
        DWORD vDosError = NOERROR;

        for (;;)
        {
            IMAGE_DOS_HEADER vDosHeader{};

            if (!SetFilePointerEx(aFileHandle, {}, nullptr, FILE_BEGIN))
            {
                vDosError = GetLastError();
                break;
            }

            DWORD vReadBytes = 0;
            if (!ReadFile(aFileHandle, &vDosHeader, sizeof(vDosHeader), &vReadBytes, nullptr))
            {
                vDosError = GetLastError();
                break;
            }

            if (!SetFilePointerEx(aFileHandle, { (DWORD)vDosHeader.e_lfanew }, nullptr, FILE_BEGIN))
            {
                vDosError = GetLastError();
                break;
            }

            if (!ReadFile(aFileHandle, aNtHeader, sizeof(*aNtHeader), &vReadBytes, nullptr))
            {
                vDosError = GetLastError();
                break;
            }

            break;
        }

        return vDosError;
    }


    inline constexpr auto ImageIs64Bit(PIMAGE_NT_HEADERS aNtHeader)
        -> bool
    {
        return (aNtHeader->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC);
    }

    inline constexpr auto ImageIs32Bit(PIMAGE_NT_HEADERS aNtHeader)
        -> bool
    {
        return !ImageIs64Bit(aNtHeader);
    }


#ifndef ImageOptHeaderFieldsX$
#define ImageOptHeaderFieldsX$(aNtHeader, aFields)                          \
            (::image::ImageIs64Bit(aNtHeader)                               \
                ? PIMAGE_NT_HEADERS64(aNtHeader)->OptionalHeader.aFields    \
                : PIMAGE_NT_HEADERS32(aNtHeader)->OptionalHeader.aFields)
#endif

#ifndef ImageLoadConfigDirFieldsX$
#define ImageLoadConfigDirFieldsX$(aNtHeader, aDir, aFields)    \
            (::image::ImageIs64Bit(aNtHeader)                   \
                ? PIMAGE_LOAD_CONFIG_DIRECTORY64(aDir)->aFields \
                : PIMAGE_LOAD_CONFIG_DIRECTORY32(aDir)->aFields)
#endif

    DWORD ImageRelocate(
        PVOID aVABase, 
        DWORD aSuccess,
        DWORD aConflict,
        DWORD aInvalid);

    DWORD ImageRelocateWithBias(
        PVOID   aVABase, 
        INT64   aAdditionalBias,
        DWORD   aSuccess,
        DWORD   aConflict,
        DWORD   aInvalid);
}
