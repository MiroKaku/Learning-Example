#pragma once


// 
// ObRegisterCallbacks() Cookie Memory layout
//
// +-------------------------------------------+
// | OB_CALLBACK_OBJECT_HEADER                 |
// +-------------------------------------------+
// | OB_CALLBACK_OBJECT_BODY[Header.BodyCount] |
// +-------------------------------------------+
// | WCHAR AltitudeBuffer[Altitude.Length]     |
// +-------------------------------------------+
//

struct OB_CALLBACK_OBJECT_HEADER;
typedef struct OB_CALLBACK_OBJECT_BODY
{
    // all OB_CALLBACK_BODY
    // Header -> OBJECT_TYPE.CallbackList
    LIST_ENTRY	                ListEntry;

    OB_OPERATION                Operations;
    ULONG		                Always_1;

    // Self
    OB_CALLBACK_OBJECT_HEADER* CallbackObject;

    POBJECT_TYPE                ObjectType;
    POB_PRE_OPERATION_CALLBACK  PreOperation;
    POB_POST_OPERATION_CALLBACK PostOperation;

    ULONG		                Reserved;
}*POB_CALLBACK_OBJECT_BODY;

typedef struct OB_CALLBACK_OBJECT_HEADER
{
    USHORT          Version;    // ObGetFilterVersion()
    USHORT          BodyCount;
    PVOID           RegistrationContext;
    UNICODE_STRING  Altitude;

    OB_CALLBACK_OBJECT_BODY Body[ANYSIZE_ARRAY];
}*POB_CALLBACK_OBJECT_HEADER;

typedef enum _SYSTEM_INFORMATION_CLASS
{
    SystemBasicInformation,
    SystemProcessorInformation,
    SystemPerformanceInformation,
    SystemTimeOfDayInformation,
    SystemNotImplemented1,
    SystemProcessesAndThreadsInformation,
    SystemCallCounts,
    SystemConfigurationInformation,
    SystemProcessorTimes,
    SystemGlobalFlag,
    SystemNotImplemented2,
    SystemModuleInformation,
    SystemLockInformation,
    SystemNotImplemented3,
    SystemNotImplemented4,
    SystemNotImplemented5,
    SystemHandleInformation,
    SystemObjectInformation,
    SystemPagefileInformation,
    SystemInstructionEmulationCounts,
    SystemInvalidInfoClass1,
    SystemCacheInformation,
    SystemPoolTagInformation,
    SystemProcessorStatistics,
    SystemDpcInformation,
    SystemNotImplemented6,
    SystemLoadImage,
    SystemUnloadImage,
    SystemTimeAdjustment,
    SystemNotImplemented7,
    SystemNotImplemented8,
    SystemNotImplemented9,
    SystemCrashDumpInformation,
    SystemExceptionInformation,
    SystemCrashDumpStateInformation,
    SystemKernelDebuggerInformation,
    SystemContextSwitchInformation,
    SystemRegistryQuotaInformation,
    SystemLoadAndCallImage,
    SystemPrioritySeparation,
    SystemNotImplemented10,
    SystemNotImplemented11,
    SystemInvalidInfoClass2,
    SystemInvalidInfoClass3,
    SystemTimeZoneInformation,
    SystemLookasideInformation,
    SystemSetTimeSlipEvent,
    SystemCreateSession,
    SystemDeleteSession,
    SystemInvalidInfoClass4,
    SystemRangeStartInformation,
    SystemVerifierInformation,
    SystemAddVerifier,
    SystemSessionProcessesInformation
}SYSTEM_INFORMATION_CLASS;

typedef struct _RTL_PROCESS_MODULE_INFORMATION
{
    HANDLE Section;                 // Not filled in
    PVOID MappedBase;
    PVOID ImageBase;
    ULONG ImageSize;
    ULONG Flags;
    USHORT LoadOrderIndex;
    USHORT InitOrderIndex;
    USHORT LoadCount;
    USHORT OffsetToFileName;
    UCHAR  FullPathName[256];
} RTL_PROCESS_MODULE_INFORMATION, * PRTL_PROCESS_MODULE_INFORMATION;

typedef struct _RTL_PROCESS_MODULES
{
    ULONG NumberOfModules;
    RTL_PROCESS_MODULE_INFORMATION Modules[1];
} RTL_PROCESS_MODULES, * PRTL_PROCESS_MODULES;

extern"C"
{
    NTSTATUS NTAPI ObQueryNameString(
        _In_    PVOID   Object,
        _Out_   POBJECT_NAME_INFORMATION ObjectNameInfo,
        _In_    ULONG   Length,
        _Out_   PULONG  ReturnLength
    );

    NTSTATUS NTAPI ZwQuerySystemInformation(
        _In_    SYSTEM_INFORMATION_CLASS SystemInformationClass,
        _Out_   PVOID SystemInformation,
        _In_    ULONG SystemInformationLength,
        _Out_   PULONG ReturnLength);
}
