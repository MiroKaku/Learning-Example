#pragma once
#include <winternl.h>


#pragma warning(push)
#pragma warning(disable: 4201)
namespace sdkext
{
    using PVOID   = void*;
    using PVOID32 = void* __ptr32;
    using PVOID64 = void* __ptr64;

    using HANDLE   = PVOID;
    using HANDLE32 = PVOID32;
    using HANDLE64 = PVOID64;

    template<typename T = HANDLE>
    struct $CLIENT_ID
    {
        T   UniqueProcess;
        T   UniqueThread;
    };

    template<typename T = PVOID>
    struct $LIST_ENTRY
    {
        T   Flink;
        T   Blink;
    };

    template<typename T = PVOID>
    struct $X_STRING
    {
        UINT16  Length;
        UINT16  MaximumLength;
        T       Buffer;
    };

    template<typename T = PVOID>
    using $UNICODE_STRING = $X_STRING<T>;

    template<typename T = PVOID>
    using $ANSI_STRING = $X_STRING<T>;

    template<typename T = PVOID>
    struct $NT_TIB
    {
        T   ExceptionList;
        T   StackBase;
        T   StackLimit;
        T   SubSystemTib;
        union
        {
            T       FiberData;
            UINT32  Version;
        };
        T   ArbitraryUserPointer;
        T   Self;
    };
    using NT_TIB   = $NT_TIB<>;
    using NT_TIB32 = $NT_TIB<PVOID32>;
    using NT_TIB64 = $NT_TIB<PVOID64>;

    template<typename T = PVOID>
    struct $TEB
    {
        $NT_TIB<T>      NtTib;
        T               EnvironmentPointer;
        $CLIENT_ID<T>   ClientId;
        T               ActiveRpcHandle;
        T               ThreadLocalStoragePointer;
        T               ProcessEnvironmentBlock;
        UINT32          LastErrorValue;
        UINT32          CountOfOwnedCriticalSections;
        T               CsrClientThread;
        T               Win32ThreadInfo;
        //rest of the structure is not defined for now, as it is not needed
    };
    using TEB   = $TEB<>;
    using TEB32 = $TEB<PVOID32>;
    using TEB64 = $TEB<PVOID64>;

    template<typename T = PVOID>
    struct $LDR_DATA_TABLE_ENTRY
    {
        $LIST_ENTRY<T>      InLoadOrderLinks;
        $LIST_ENTRY<T>      InMemoryOrderLinks;
        $LIST_ENTRY<T>      InInitializationOrderLinks;

        T                   DllBase;
        T                   EntryPoint;
        UINT32              SizeOfImage;

        $UNICODE_STRING<T>  FullDllName;
        $UNICODE_STRING<T>  BaseDllName;

        union
        {
            UINT32      Flags;
            struct
            {
                UINT32  PackagedBinary : 1;
                UINT32  MarkedForRemoval : 1;
                UINT32  ImageDll : 1;
                UINT32  LoadNotificationsSent : 1;
                UINT32  TelemetryEntryProcessed : 1;
                UINT32  ProcessStaticImport : 1;
                UINT32  InLegacyLists : 1;
                UINT32  InIndexes : 1;
                UINT32  ShimDll : 1;
                UINT32  InExceptionTable : 1;
                UINT32  ReservedFlags1 : 2;
                UINT32  LoadInProgress : 1;
                UINT32  LoadConfigProcessed : 1;
                UINT32  EntryProcessed : 1;
                UINT32  ProtectDelayLoad : 1;
                UINT32  ReservedFlags3 : 2;
                UINT32  DontCallForThreads : 1;
                UINT32  ProcessAttachCalled : 1;
                UINT32  ProcessAttachFailed : 1;
                UINT32  CorDeferredValidate : 1;
                UINT32  CorImage : 1;
                UINT32  DontRelocate : 1;
                UINT32  CorILOnly : 1;
                UINT32  ReservedFlags5 : 3;
                UINT32  Redirected : 1;
                UINT32  ReservedFlags6 : 2;
                UINT32  CompatDatabaseProcessed : 1;
            };
        };

        UINT16          LoadCount;
        UINT16          TlsIndex;

        $LIST_ENTRY<T>  HashLinks;

        union
        {
            UINT32  TimeDateStamp;
            T       LoadedImports;
        };

        T   EntryPointActivationContext;
        T   Lock;
        T   DdagNode;
        $LIST_ENTRY<T> NodeModuleLink;
        T   LoadContext;
        T   ParentDllBase;
        T   SwitchBackContext;
    };
    using LDR_DATA_TABLE_ENTRY   = $LDR_DATA_TABLE_ENTRY<>;
    using LDR_DATA_TABLE_ENTRY32 = $LDR_DATA_TABLE_ENTRY<PVOID32>;
    using LDR_DATA_TABLE_ENTRY64 = $LDR_DATA_TABLE_ENTRY<PVOID64>;

    template<typename T = PVOID>
    struct $PEB_LDR_DATA
    {
        UINT32          Length;
        UINT8           Initialized;
        T               SsHandle;
        $LIST_ENTRY<T>  InLoadOrderModuleList;  // LDR_DATA_TABLE_ENTRY
        $LIST_ENTRY<T>  InMemoryOrderModuleList;
        $LIST_ENTRY<T>  InInitializationOrderModuleList;
        T               EntryInProgress;
        UINT8           ShutdownInProgress;
        T               ShutdownThreadId;
    };
    using PEB_LDR_DATA   = $PEB_LDR_DATA<>;
    using PEB_LDR_DATA32 = $PEB_LDR_DATA<PVOID32>;
    using PEB_LDR_DATA64 = $PEB_LDR_DATA<PVOID64>;

    template<typename T = PVOID, typename U = SIZE_T>
    struct $PEB
    {
        enum : UINT32
        {
            GdiHandleBufferSize32   = 34,
            GdiHandleBufferSize64   = 60,
            GdiHandleBufferSize     = sizeof(T) == sizeof(UINT32) ? GdiHandleBufferSize32 : GdiHandleBufferSize64,

            FlsMaximumAvailable     = 128,
            TlsMinimumAvailable     = 64,
            TlsExpansionSlots       = 1024,
        };

        UINT8   InheritedAddressSpace;
        UINT8   ReadImageFileExecOptions;
        UINT8   BeingDebugged;
        union
        {
            UINT8        BitField;
            struct
            {
                UINT8 ImageUsesLargePages : 1;
                UINT8 IsProtectedProcess : 1;
                UINT8 IsImageDynamicallyRelocated : 1;
                UINT8 SkipPatchingUser32Forwarders : 1;
                UINT8 IsPackagedProcess : 1;
                UINT8 IsAppContainer : 1;
                UINT8 IsProtectedProcessLight : 1;
                UINT8 IsLongPathAwareProcess : 1;
            };
        };
        T    Mutant;
        T    ImageBaseAddress;
        T    Ldr; // PEB_LDR_DATA
        T    ProcessParameters;
        T    SubSystemData;
        T    ProcessHeap;
        T    FastPebLock;
        T    AtlThunkSListPtr;
        T    IFEOKey;
        union
        {
            UINT32      CrossProcessFlags;
            struct
            {
                UINT32  ProcessInJob : 1;
                UINT32  ProcessInitializing : 1;
                UINT32  ProcessUsingVEH : 1;
                UINT32  ProcessUsingVCH : 1;
                UINT32  ProcessUsingFTH : 1;
                UINT32  ProcessPreviouslyThrottled : 1;
                UINT32  ProcessCurrentlyThrottled : 1;
                UINT32  ReservedBits0 : 25;
            };
        };
        union
        {
            T    KernelCallbackTable;
            T    UserSharedInfoPtr;
        };
        UINT32  SystemReserved[1];
        UINT32  AtlThunkSListPtr32;
        T    ApiSetMap;
        UINT32  TlsExpansionCounter;
        T    TlsBitmap;
        UINT32  TlsBitmapBits[2];
        T    ReadOnlySharedMemoryBase;
        T    HotpatchInformation;
        T    ReadOnlyStaticServerData;
        T    AnsiCodePageData;
        T    OemCodePageData;
        T    UnicodeCaseTableData;
        UINT32  NumberOfProcessors;
        UINT32  NtGlobalFlag;
        LARGE_INTEGER CriticalSectionTimeout;
        U    HeapSegmentReserve;
        U    HeapSegmentCommit;
        U    HeapDeCommitTotalFreeThreshold;
        U    HeapDeCommitFreeBlockThreshold;
        UINT32  NumberOfHeaps;
        UINT32  MaximumNumberOfHeaps;
        T    ProcessHeaps;
        T    GdiSharedHandleTable;
        T    ProcessStarterHelper;
        UINT32  GdiDCAttributeList;
        T    LoaderLock;
        UINT32  OSMajorVersion;
        UINT32  OSMinorVersion;
        UINT16  OSBuildNumber;
        UINT16  OSCSDVersion;
        UINT32  OSPlatformId;
        UINT32  ImageSubsystem;
        UINT32  ImageSubsystemMajorVersion;
        UINT32  ImageSubsystemMinorVersion;
        U    ActiveProcessAffinityMask;
        UINT32  GdiHandleBuffer[GdiHandleBufferSize];
        T    PostProcessInitRoutine;
        T    TlsExpansionBitmap;
        UINT32  TlsExpansionBitmapBits[32];
        UINT32  SessionId;
        ULARGE_INTEGER AppCompatFlags;
        ULARGE_INTEGER AppCompatFlagsUser;
        T    pShimData;
        T    AppCompatInfo;
        $UNICODE_STRING<T> CSDVersion;
        T    ActivationContextData;
        T    ProcessAssemblyStorageMap;
        T    SystemDefaultActivationContextData;
        T    SystemAssemblyStorageMap;
        U    MinimumStackCommit;
        T    FlsCallback;
        $LIST_ENTRY<T>  FlsListHead;
        T    FlsBitmap;
        UINT32  FlsBitmapBits[FlsMaximumAvailable / (sizeof(UINT32) * 8)];
        UINT32  FlsHighIndex;
        T    WerRegistrationData;
        T    WerShipAssertPtr;
        T    ContextData;
        T    ImageHeaderHash;
        union
        {
            UINT32      TracingFlags;
            struct
            {
                UINT32  HeapTracingEnabled : 1;
                UINT32  CritSecTracingEnabled : 1;
                UINT32  LibLoaderTracingEnabled : 1;
                UINT32  SpareTracingBits : 29;
            };
        };
        UINT64  CsrServerReadOnlySharedMemoryBase;
        T    TppWorkerpListLock;
        $LIST_ENTRY<T> TppWorkerpList;
        T    WaitOnAddressHashTable[128];
        T    TelemetryCoverageHeader;    // REDSTONE3
        UINT32  CloudFileFlags;
        UINT32  CloudFileDiagFlags;         // REDSTONE4
        UINT8   PlaceholderCompatibilityMode;
        UINT8   PlaceholderCompatibilityModeReserved[7];
    };
    using PEB   = $PEB<>;
    using PEB32 = $PEB<PVOID32, UINT32>;
    using PEB64 = $PEB<PVOID64, UINT64>;

    static_assert(offsetof(PEB32, CriticalSectionTimeout) == 0x70);
    static_assert(offsetof(PEB64, CriticalSectionTimeout) == 0xC0);
    static_assert(offsetof(PEB32, SessionId) == 0x1D4);
    static_assert(offsetof(PEB64, SessionId) == 0x2C0);
    static_assert(sizeof(PEB32) == 0x470); // REDSTONE4
    static_assert(sizeof(PEB64) == 0x7B8);

    enum Context64Flags : long
    {
        CONTEXT_ARCH = 0x00100000L,

        CONTEXT64_CONTROL           = (CONTEXT_ARCH | 0x00000001L),
        CONTEXT64_INTEGER           = (CONTEXT_ARCH | 0x00000002L),
        CONTEXT64_SEGMENTS          = (CONTEXT_ARCH | 0x00000004L),
        CONTEXT64_FLOATING_POINT    = (CONTEXT_ARCH | 0x00000008L),
        CONTEXT64_DEBUG_REGISTERS   = (CONTEXT_ARCH | 0x00000010L),

        CONTEXT64_FULL              = (CONTEXT64_CONTROL | CONTEXT64_INTEGER | CONTEXT64_FLOATING_POINT),
        CONTEXT64_ALL               = (CONTEXT64_CONTROL | CONTEXT64_INTEGER | CONTEXT64_SEGMENTS | CONTEXT64_FLOATING_POINT | CONTEXT64_DEBUG_REGISTERS),

        CONTEXT64_XSTATE            = (CONTEXT_ARCH | 0x00000040L),
    };


    struct DECLSPEC_ALIGN(16) XSAVE_FORMAT64 {
        UINT16  ControlWord;
        UINT16  StatusWord;
        UINT8   TagWord;
        UINT8   Reserved1;
        UINT16  ErrorOpcode;
        UINT32  ErrorOffset;
        UINT16  ErrorSelector;
        UINT16  Reserved2;
        UINT32  DataOffset;
        UINT16  DataSelector;
        UINT16  Reserved3;
        UINT32  MxCsr;
        UINT32  MxCsr_Mask;
        M128A   FloatRegisters[8];
        M128A   XmmRegisters[16];
        UINT8   Reserved4[96];

    };
    using PXSAVE_FORMAT64   = XSAVE_FORMAT64 *;
    using XMM_SAVE_AREA64   = XSAVE_FORMAT64;
    using PXMM_SAVE_AREA64  = XMM_SAVE_AREA64 *;

    typedef struct DECLSPEC_ALIGN(16) CONTEXT64 {

        //
        // Register parameter home addresses.
        //
        // N.B. These fields are for convience - they could be used to extend the
        //      context record in the future.
        //

        UINT64 P1Home;
        UINT64 P2Home;
        UINT64 P3Home;
        UINT64 P4Home;
        UINT64 P5Home;
        UINT64 P6Home;

        //
        // Control flags.
        //

        UINT32 ContextFlags;
        UINT32 MxCsr;

        //
        // Segment Registers and processor flags.
        //

        UINT16 SegCs;
        UINT16 SegDs;
        UINT16 SegEs;
        UINT16 SegFs;
        UINT16 SegGs;
        UINT16 SegSs;
        UINT32 EFlags;

        //
        // Debug registers
        //

        UINT64 Dr0;
        UINT64 Dr1;
        UINT64 Dr2;
        UINT64 Dr3;
        UINT64 Dr6;
        UINT64 Dr7;

        //
        // Integer registers.
        //

        UINT64 Rax;
        UINT64 Rcx;
        UINT64 Rdx;
        UINT64 Rbx;
        UINT64 Rsp;
        UINT64 Rbp;
        UINT64 Rsi;
        UINT64 Rdi;
        UINT64 R8;
        UINT64 R9;
        UINT64 R10;
        UINT64 R11;
        UINT64 R12;
        UINT64 R13;
        UINT64 R14;
        UINT64 R15;

        //
        // Program counter.
        //

        UINT64 Rip;

        //
        // Floating point state.
        //

        union {
            XMM_SAVE_AREA64 FltSave;
            struct {
                M128A Header[2];
                M128A Legacy[8];
                M128A Xmm0;
                M128A Xmm1;
                M128A Xmm2;
                M128A Xmm3;
                M128A Xmm4;
                M128A Xmm5;
                M128A Xmm6;
                M128A Xmm7;
                M128A Xmm8;
                M128A Xmm9;
                M128A Xmm10;
                M128A Xmm11;
                M128A Xmm12;
                M128A Xmm13;
                M128A Xmm14;
                M128A Xmm15;
            } DUMMYSTRUCTNAME;
        } DUMMYUNIONNAME;

        //
        // Vector registers.
        //

        M128A VectorRegister[26];
        UINT64 VectorControl;

        //
        // Special debug control registers.
        //

        UINT64 DebugControl;
        UINT64 LastBranchToRip;
        UINT64 LastBranchFromRip;
        UINT64 LastExceptionToRip;
        UINT64 LastExceptionFromRip;
    }*PCONTEXT64;

    enum MEMORY_INFORMATION_CLASS
    {
        MemoryBasicInformation,             // MEMORY_BASIC_INFORMATION
        MemoryWorkingSetInformation,        // MEMORY_WORKING_SET_INFORMATION
        MemoryMappedFilenameInformation,    // UNICODE_STRING
        MemoryRegionInformation,            // MEMORY_REGION_INFORMATION
        MemoryWorkingSetExInformation,      // MEMORY_WORKING_SET_EX_INFORMATION
        MemorySharedCommitInformation,      // MEMORY_SHARED_COMMIT_INFORMATION
        MemoryImageInformation,             // MEMORY_IMAGE_INFORMATION
        MemoryRegionInformationEx,
        MemoryPrivilegedBasicInformation,
        MemoryEnclaveImageInformation,      // MEMORY_ENCLAVE_IMAGE_INFORMATION // since REDSTONE3
        MemoryBasicInformationCapped
    };

    template<typename T = PVOID>
    struct $PROCESS_BASIC_INFORMATION {
        T   Reserved1;
        T   PebBaseAddress;
        T   Reserved2[2];
        T   UniqueProcessId;
        T   Reserved3;
    };
    using PROCESS_BASIC_INFORMATION   = $PROCESS_BASIC_INFORMATION<PVOID>;
    using PROCESS_BASIC_INFORMATION32 = $PROCESS_BASIC_INFORMATION<PVOID32>;
    using PROCESS_BASIC_INFORMATION64 = $PROCESS_BASIC_INFORMATION<PVOID64>;

    typedef enum _SECTION_INHERIT
    {
        ViewShare = 1,
        ViewUnmap = 2
    } SECTION_INHERIT, * PSECTION_INHERIT;
}
#pragma warning(pop)


namespace sdkext
{
    extern"C"
    {
        VOID WINAPI RtlSetLastWin32ErrorAndNtStatusFromNtStatus(
            NTSTATUS Status);

        NTSTATUS NTAPI NtAllocateVirtualMemory(
            _In_ HANDLE ProcessHandle,
            _Inout_ _At_(*BaseAddress, _Readable_bytes_(*RegionSize) _Writable_bytes_(*RegionSize) _Post_readable_byte_size_(*RegionSize)) PVOID* BaseAddress,
            _In_ ULONG_PTR ZeroBits,
            _Inout_ PSIZE_T RegionSize,
            _In_ ULONG AllocationType,
            _In_ ULONG Protect);

        NTSTATUS NTAPI NtFreeVirtualMemory(
            _In_ HANDLE ProcessHandle,
            _Inout_ PVOID* BaseAddress,
            _Inout_ PSIZE_T RegionSize,
            _In_ ULONG FreeType);

        NTSTATUS NTAPI NtReadVirtualMemory(
            _In_ HANDLE ProcessHandle,
            _In_opt_ PVOID BaseAddress,
            _Out_writes_bytes_(BufferSize) PVOID Buffer,
            _In_ SIZE_T BufferSize,
            _Out_opt_ PSIZE_T NumberOfBytesRead);

        NTSTATUS NTAPI NtWriteVirtualMemory(
            _In_ HANDLE ProcessHandle,
            _In_opt_ PVOID BaseAddress,
            _In_reads_bytes_(BufferSize) PVOID Buffer,
            _In_ SIZE_T BufferSize,
            _Out_opt_ PSIZE_T NumberOfBytesWritten);

        NTSTATUS NTAPI NtProtectVirtualMemory(
            _In_ HANDLE ProcessHandle,
            _Inout_ PVOID* BaseAddress,
            _Inout_ PSIZE_T RegionSize,
            _In_ ULONG NewProtect,
            _Out_ PULONG OldProtect);

        NTSTATUS NTAPI NtFlushInstructionCache(
            _In_ HANDLE ProcessHandle,
            _In_opt_ PVOID BaseAddress,
            _In_ SIZE_T Length);

        NTSTATUS NTAPI NtQueryVirtualMemory(
            _In_ HANDLE ProcessHandle,
            _In_ PVOID BaseAddress,
            _In_ MEMORY_INFORMATION_CLASS MemoryInformationClass,
            _Out_writes_bytes_(MemoryInformationLength) PVOID MemoryInformation,
            _In_ SIZE_T MemoryInformationLength,
            _Out_opt_ PSIZE_T ReturnLength);

        NTSTATUS NTAPI NtGetContextThread(
            _In_ HANDLE ThreadHandle,
            _Inout_ PCONTEXT ThreadContext);

        NTSTATUS NTAPI NtSetContextThread(
            _In_ HANDLE ThreadHandle,
            _In_ PCONTEXT ThreadContext);

        NTSTATUS NTAPI NtMapViewOfSection(
            _In_ HANDLE SectionHandle,
            _In_ HANDLE ProcessHandle,
            _Inout_ _At_(*BaseAddress, _Readable_bytes_(*ViewSize) _Writable_bytes_(*ViewSize) _Post_readable_byte_size_(*ViewSize)) PVOID* BaseAddress,
            _In_ ULONG_PTR ZeroBits,
            _In_ SIZE_T CommitSize,
            _Inout_opt_ PLARGE_INTEGER SectionOffset,
            _Inout_ PSIZE_T ViewSize,
            _In_ SECTION_INHERIT InheritDisposition,
            _In_ ULONG AllocationType,
            _In_ ULONG Win32Protect);

        NTSTATUS NTAPI NtUnmapViewOfSection(
            _In_ HANDLE ProcessHandle,
            _In_opt_ PVOID BaseAddress);

        NTSTATUS NTAPI NtQueryInformationProcess(
            _In_ HANDLE ProcessHandle,
            _In_ PROCESSINFOCLASS ProcessInformationClass,
            _Out_writes_bytes_(ProcessInformationLength) PVOID ProcessInformation,
            _In_ ULONG ProcessInformationLength,
            _Out_opt_ PULONG ReturnLength);

        BOOL WINAPI CreateProcessInternalW(
            HANDLE                  aUserToken,
            LPCWSTR                 aApplicationName,
            LPWSTR                  aCommandLine,
            LPSECURITY_ATTRIBUTES   aProcessAttributes,
            LPSECURITY_ATTRIBUTES   aThreadAttributes,
            BOOL                    aInheritHandles,
            DWORD                   aCreationFlags,
            LPVOID                  aEnvironment,
            LPCWSTR                 aCurrentDirectory,
            LPSTARTUPINFOW          aStartupInfo,
            LPPROCESS_INFORMATION   aProcessInformation,
            PHANDLE                 aNewToken);
    }
}
