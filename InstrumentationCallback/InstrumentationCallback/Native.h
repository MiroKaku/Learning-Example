#pragma once


namespace sdkext
{
    typedef enum _PROCESSINFOCLASS
    {
        ProcessBasicInformation, 
        ProcessQuotaLimits, 
        ProcessIoCounters, 
        ProcessVmCounters, 
        ProcessTimes, 
        ProcessBasePriority, 
        ProcessRaisePriority, 
        ProcessDebugPort, 
        ProcessExceptionPort, 
        ProcessAccessToken, 
        ProcessLdtInformation, 
        ProcessLdtSize, 
        ProcessDefaultHardErrorMode, 
        ProcessIoPortHandlers, 
        ProcessPooledUsageAndLimits, 
        ProcessWorkingSetWatch, 
        ProcessUserModeIOPL,
        ProcessEnableAlignmentFaultFixup, 
        ProcessPriorityClass, 
        ProcessWx86Information,
        ProcessHandleCount, 
        ProcessAffinityMask, 
        ProcessPriorityBoost, 
        ProcessDeviceMap, 
        ProcessSessionInformation, 
        ProcessForegroundInformation, 
        ProcessWow64Information, 
        ProcessImageFileName, 
        ProcessLUIDDeviceMapsEnabled, 
        ProcessBreakOnTermination, 
        ProcessDebugObjectHandle, 
        ProcessDebugFlags, 
        ProcessHandleTracing, 
        ProcessIoPriority, 
        ProcessExecuteFlags, 
        ProcessResourceManagement, 
        ProcessCookie, 
        ProcessImageInformation, 
        ProcessCycleTime, 
        ProcessPagePriority, 
        ProcessInstrumentationCallback,  // <<<< this
        ProcessThreadStackAllocation, 
        ProcessWorkingSetWatchEx, 
        ProcessImageFileNameWin32, 
        ProcessImageFileMapping, 
        ProcessAffinityUpdateMode, 
        ProcessMemoryAllocationMode, 
        ProcessGroupInformation, 
        ProcessTokenVirtualizationEnabled, 
        ProcessConsoleHostProcess, 
        ProcessWindowInformation, 
        ProcessHandleInformation, 
        ProcessMitigationPolicy, 
        ProcessDynamicFunctionTableInformation,
        ProcessHandleCheckingMode, 
        ProcessKeepAliveCount, 
        ProcessRevokeFileHandles, 
        ProcessWorkingSetControl, 
        ProcessHandleTable, 
        ProcessCheckStackExtentsMode,
        ProcessCommandLineInformation, 
        ProcessProtectionInformation, 
        ProcessMemoryExhaustion, 
        ProcessFaultInformation, 
        ProcessTelemetryIdInformation, 
        ProcessCommitReleaseInformation, 
        ProcessDefaultCpuSetsInformation,
        ProcessAllowedCpuSetsInformation,
        ProcessSubsystemProcess,
        ProcessJobMemoryInformation, 
        ProcessInPrivate, 
        ProcessRaiseUMExceptionOnInvalidHandleClose,
        ProcessIumChallengeResponse,
        ProcessChildProcessInformation, 
        ProcessHighGraphicsPriorityInformation,
        ProcessSubsystemInformation, 
        ProcessEnergyValues, 
        ProcessActivityThrottleState, 
        ProcessActivityThrottlePolicy, 
        ProcessWin32kSyscallFilterInformation,
        ProcessDisableSystemAllowedCpuSets, 
        ProcessWakeInformation, 
        ProcessEnergyTrackingState, 
        ProcessManageWritesToExecutableMemory, 
        ProcessCaptureTrustletLiveDump,
        ProcessTelemetryCoverage,
        ProcessEnclaveInformation,
        ProcessEnableReadWriteVmLogging, 
        ProcessUptimeInformation, 
        ProcessImageSection,
        ProcessDebugAuthInformation, 
        ProcessSystemResourceManagement, 
        ProcessSequenceNumber, 
        ProcessLoaderDetour, 
        ProcessSecurityDomainInformation, 
        ProcessCombineSecurityDomainsInformation, 
        ProcessEnableLogging, 
        ProcessLeapSecondInformation, 
        MaxProcessInfoClass
    } PROCESSINFOCLASS;

    using $NtSetInformationProcess = NTSTATUS(NTAPI *)(
        HANDLE  aProcessHandle,
        PROCESSINFOCLASS aProcessInformationClass,
        PVOID   aProcessInformation,
        UINT32  aProcessInformationLength
    );

    typedef struct _PROCESS_INSTRUMENTATION_CALLBACK_INFORMATION
    {
        PVOID Callback;
    } PROCESS_INSTRUMENTATION_CALLBACK_INFORMATION, * PPROCESS_INSTRUMENTATION_CALLBACK_INFORMATION;

    // Since Windows 10
    typedef struct _PROCESS_INSTRUMENTATION_CALLBACK_INFORMATION_EX
    {
        ULONG Version;
        ULONG Reserved;
        PVOID Callback;
    } PROCESS_INSTRUMENTATION_CALLBACK_INFORMATION_EX, * PPROCESS_INSTRUMENTATION_CALLBACK_INFORMATION_EX;
    

    using $RtlRandomEx = ULONG(NTAPI*)(
        PULONG aSeed
        );

    using $RtlGetVersion = NTSTATUS(NTAPI*)(
        PRTL_OSVERSIONINFOW aOSVI
    );
}
