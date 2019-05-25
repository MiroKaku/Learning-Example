/*++

Copyright (c) Microsoft Corporation. All rights reserved.

You may only use this code if you agree to the terms of the Windows Research Kernel Source Code License agreement (see License.txt).
If you do not agree to the terms, do not use the code.


Module Name:

ldrreloc.c

Abstract:

This module contains the code to relocate an image when
the preferred base isn't available. This is called by the
boot loader, device driver loader, and system loader.

--*/
#include "stdafx.h"

#include "Image.h"
#pragma comment(lib, "ImageHlp.lib")


namespace image
{

#ifndef RVA$
#define RVA$(r, o) (PVOID(ULONG_PTR(r) + ULONG_PTR(o)))
#endif


    enum : UINT32
    {
        // Mark a HIGHADJ entry as needing an increment if reprocessing.
        HighAdjustIncrement = 0x1,

        // Mark a HIGHADJ entry as not suitable for reprocessing.
        HighAdjustFinal = 0x2,
    };

    struct IMAGE_BASE_RELOCATION_ENTRY
    {
        USHORT Offset : 12;
        USHORT Type : 04;
    };
    using PIMAGE_BASE_RELOCATION_ENTRY = IMAGE_BASE_RELOCATION_ENTRY* ;
        

    //PVOID ImageDirectoryEntryToData(
    //    PVOID               aBase,
    //    PIMAGE_NT_HEADERS   aNtHeader,
    //    UINT16              aDirectoryIndex,
    //    UINT32*             aSize)
    //{
    //    if (aDirectoryIndex >= IMAGE_NUMBEROF_DIRECTORY_ENTRIES)
    //    {
    //        return nullptr;
    //    }
    //
    //    if (nullptr == aSize)
    //    {
    //        return nullptr;
    //    }
    //
    //    auto vDataDirectory = aNtHeader->OptionalHeader.DataDirectory;
    //    if (nullptr == vDataDirectory)
    //    {
    //        return nullptr;
    //    }
    //
    //    *aSize = vDataDirectory[aDirectoryIndex].Size;
    //
    //    auto vRva = vDataDirectory[aDirectoryIndex].VirtualAddress;
    //
    //    return static_cast<void*>(static_cast<byte*>(aBase) + vRva);
    //}


    static auto ProcessRelocationBlockLonglong(
        ULONG_PTR   aVA,
        ULONG       aSizeOfBlock,
        USHORT*     aNextOffset,
        INT64       aDiff)
        -> PIMAGE_BASE_RELOCATION
    {
        while (aSizeOfBlock)
        {
            --aSizeOfBlock;

            auto vOffset = static_cast<USHORT>(*aNextOffset & 0x0FFFui16);
            auto vFixupVA = RVA$(aVA, vOffset);

            switch ((*aNextOffset) >> 12)
            {
            case IMAGE_REL_BASED_HIGHLOW:
            {
                //
                // HighLow - (32-bits) relocate the high and low half of an address.
                //
                auto vLongPtr = static_cast<ULONG UNALIGNED *>(vFixupVA);
                *vLongPtr += static_cast<ULONG>(aDiff & UINT32_MAX);
                break;
            }

            case IMAGE_REL_BASED_HIGH:
            {
                //
                // High - (16-bits) relocate the high half of an address.
                //
                auto vShortPtr = static_cast<USHORT*>(vFixupVA);
                auto vDiff = static_cast<USHORT*>(static_cast<void*>(&aDiff));
                *vShortPtr += vDiff[1];
                break;
            }

            case IMAGE_REL_BASED_LOW:
            {
                //
                // Low - (16-bit) relocate the low half of an address.
                //

                auto vShortPtr = static_cast<USHORT*>(vFixupVA);
                auto vDiff = static_cast<USHORT*>(static_cast<void*>(&aDiff));
                *vShortPtr += vDiff[0];
                break;
            }

            case IMAGE_REL_BASED_HIGHADJ:
            {
                //
                // Adjust high - (16-bits) relocate the high half of an
                //      address and adjust for sign extension of low half.
                //

                //
                // If the address has already been relocated then don't
                // process it again now or information will be lost.
                //
                if (vOffset & HighAdjustFinal)
                {
                    ++aNextOffset;
                    --aSizeOfBlock;
                    break;
                }

                auto vShortPtr = static_cast<USHORT*>(vFixupVA);
                LONG vValue = MAKELONG(0u, *vShortPtr);

                ++aNextOffset;
                --aSizeOfBlock;

                vValue += (LONG)(*(PSHORT)aNextOffset);
                vValue += static_cast<LONG>(aDiff & long(-1));
                vValue += 0x8000l;
                *(PUSHORT)vFixupVA = HIWORD(vValue);

                break;
            }

            case IMAGE_REL_BASED_DIR64:
            {
                auto vLonglongPtr = static_cast<UINT64 UNALIGNED *>(vFixupVA);
                *vLonglongPtr += aDiff;
                break;
            }

            case IMAGE_REL_BASED_IA64_IMM64:
            {
                //
                // Align it to bundle address before fixing up the
                // 64-bit immediate value of the movl instruction.
                //
                break;
            }

            case IMAGE_REL_BASED_MIPS_JMPADDR:
            {
                //
                // JumpAddress - (32-bits) relocate a MIPS jump address.
                //
                auto vLongPtr = static_cast<ULONG UNALIGNED *>(vFixupVA);

                auto vValue = (*vLongPtr & 0x03FFFFFF) << 2;
                vValue = vValue + (ULONG)(aDiff & long(-1));
                *vLongPtr = (*vLongPtr & ~0x03FFFFFF) | ((vValue >> 2) & 0x03FFFFFF);

                break;
            }

            case IMAGE_REL_BASED_ABSOLUTE:
            {
                //
                // Absolute - no fixup required.
                //
                break;
            }

            case IMAGE_REL_BASED_RESERVED:
            {
                //
                // Section Relative reloc.  Ignore for now.
                //
                break;
            }

            case IMAGE_REL_BASED_THUMB_MOV32:
            {
                //
                // Relative intrasection. Ignore for now.
                //
                break;
            }

            default:
            {
                //
                // Illegal - illegal relocation type.
                //

                return nullptr;
            }

            } // switch

            ++aNextOffset;
        }

        return reinterpret_cast<PIMAGE_BASE_RELOCATION>(aNextOffset);
    }

    DWORD ImageRelocate(PVOID aVABase, DWORD aSuccess, DWORD aConflict, DWORD aInvalid)
    {
        return ImageRelocateWithBias(aVABase, 0, aSuccess, aConflict, aInvalid);
    }

    DWORD ImageRelocateWithBias(PVOID aVABase, INT64 aAdditionalBias, DWORD aSuccess, DWORD aConflict, DWORD aInvalid)
        /*++

        Routine Description:

        This routine relocates an image file that was not loaded into memory
        at the preferred address.

        Arguments:

        NewBase - Supplies a pointer to the image base.

        AdditionalBias - An additional quantity to add to all fixups.  The
        32-bit X86 loader uses this when loading 64-bit images
        to specify a NewBase that is actually a 64-bit value.

        Success - Value to return if relocation successful.

        Conflict - Value to return if can't relocate.

        Invalid - Value to return if relocations are invalid.

        Return Value:

        Success if image is relocated.
        Conflict if image can't be relocated.
        Invalid if image contains invalid fixups.

        --*/
    {
        DWORD vStatus = aSuccess;

        for (;;)
        {
            auto vNtHeader = ImageNtHeader(aVABase);
            if (nullptr == vNtHeader)
            {
                vStatus = aInvalid;
                break;
            }

            auto vOldBase = ImageOptHeaderFieldsX$(vNtHeader, ImageBase);
            if (0 == vOldBase)
            {
                vStatus = aInvalid;
                break;
            }

            //
            // Locate the relocation section.
            //

            auto vTotalCountBytes = 0ul;
            auto vNextBlock = static_cast<PIMAGE_BASE_RELOCATION>(
                ImageDirectoryEntryToData(aVABase, TRUE, IMAGE_DIRECTORY_ENTRY_BASERELOC, &vTotalCountBytes));

            //
            // It is possible for a file to have no relocations, but the relocations
            // must not have been stripped.
            //

            if (!vNextBlock || !vTotalCountBytes)
            {
                vStatus = (vNtHeader->FileHeader.Characteristics & IMAGE_FILE_RELOCS_STRIPPED) ? aConflict : aSuccess;
                break;
            }

            //
            // If the image has a relocation table, then apply the specified fixup
            // information to the image.
            //

            auto vDiff = (reinterpret_cast<decltype(vOldBase)>(aVABase) - vOldBase) + aAdditionalBias;
            while (vTotalCountBytes)
            {
                auto vSizeOfBlock = vNextBlock->SizeOfBlock;
                vTotalCountBytes -= vSizeOfBlock;
                vSizeOfBlock -= sizeof(IMAGE_BASE_RELOCATION);
                vSizeOfBlock /= sizeof(USHORT);
                auto vNextOffset = reinterpret_cast<USHORT*>(reinterpret_cast<__int8*>(vNextBlock) + sizeof(IMAGE_BASE_RELOCATION));
                auto vVA = reinterpret_cast<decltype(vOldBase)>(aVABase) + vNextBlock->VirtualAddress;

                vNextBlock = ProcessRelocationBlockLonglong(static_cast<ULONG_PTR>(vVA), vSizeOfBlock, vNextOffset, vDiff);
                if (nullptr == vNextBlock)
                {
                    vStatus = aInvalid;
                    break;
                }
            }

            break;
        }

        return vStatus;
    }

}
