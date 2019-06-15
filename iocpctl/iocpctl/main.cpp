#include "stdafx.h"


extern"C"   DRIVER_INITIALIZE   DriverEntry;


enum : ULONG
{
    IOCPCTL_TAG = 'CPTL' 
};

typedef struct FILE_CONTEXT
{
    IO_REMOVE_LOCK  FileRundownLock;

    KSPIN_LOCK      QueueLock;
    LIST_ENTRY      QueueHead;

}*PFILE_CONTEXT;

typedef struct NOTIFY_RECORD
{
    LIST_ENTRY      ListEntry;
    PIRP            PendingIrp;
    KDPC            Dpc;
    KTIMER          Timer;
    PFILE_OBJECT    FileObject;
    PFILE_CONTEXT   FileContext;
    BOOLEAN         CancelRoutineFreeMemory;
}*PNOTIFY_RECORD;


static
auto CancelRoutine(PDEVICE_OBJECT /*aDeviceObject*/, PIRP aIrp) -> void
{
    auto vRecord = static_cast<PNOTIFY_RECORD>(aIrp->Tail.Overlay.DriverContext[3]);
    
    NT_ASSERT(vRecord != nullptr);

    //
    // Release the cancel spinlock
    //
    IoReleaseCancelSpinLock(aIrp->CancelIrql);

    //
    // Acquire the queue spinlock
    //
    auto vIrql      = KIRQL();
    auto vContext   = vRecord->FileContext;
    KeAcquireSpinLock(&vContext->QueueLock, &vIrql);

    RemoveEntryList(&vRecord->ListEntry);

    //
    // Clear the pending Irp field because we complete the IRP no matter whether
    // we succeed or fail to cancel the timer. TimerDpc will check this field
    // before dereferencing the IRP.
    //
    vRecord->PendingIrp = nullptr;

    if (KeCancelTimer(&vRecord->Timer))
    {
        ExFreePoolWithTag(vRecord, IOCPCTL_TAG);
    }
    else 
    {
        //
        // Here the possibilities are:
        // 1) DPC is fired and waiting to acquire the lock.
        // 2) DPC has run to completion.
        // 3) DPC has been cancelled by the cleanup routine.
        // By checking the CancelRoutineFreeMemory, we can figure out whether
        // dpc is waiting to acquire the lock and access the notifyRecord memory.
        //
        if (vRecord->CancelRoutineFreeMemory == FALSE) 
        {
            //
            // This is case 1 where the DPC is waiting to run.
            //
            InitializeListHead(&vRecord->ListEntry);
        }
        else 
        {
            //
            // This is either 2 or 3.
            //
            ExFreePoolWithTag(vRecord, IOCPCTL_TAG);
        }
    }
    KeReleaseSpinLock(&vContext->QueueLock, vIrql);
    
    aIrp->Tail.Overlay.DriverContext[3] = nullptr;

    aIrp->IoStatus.Status       = STATUS_CANCELLED;
    aIrp->IoStatus.Information  = 0;
    IoCompleteRequest(aIrp, IO_NO_INCREMENT);
}

static
auto CustomTimerDPC(
    PKDPC /*Dpc*/,
    PVOID DeferredContext,
    PVOID /*SystemArgument1*/,
    PVOID /*SystemArgument2*/
) -> void
{
    NT_ASSERT(DeferredContext != nullptr);

    auto vRecord    = static_cast<PNOTIFY_RECORD>(DeferredContext);
    auto vContext   = vRecord->FileContext;
    auto vIrp       = vRecord->PendingIrp;

    KeAcquireSpinLockAtDpcLevel(&vContext->QueueLock);
    RemoveEntryList(&vRecord->ListEntry);
    if (vIrp != nullptr)
    {
        if (IoSetCancelRoutine(vIrp, nullptr) != nullptr)
        {
            vIrp->Tail.Overlay.DriverContext[3] = nullptr;

            //
            // Drop the lock before completing the request.
            //
            KeReleaseSpinLockFromDpcLevel(&vContext->QueueLock);
            for (;;)
            {
                auto vResult    = STATUS_SUCCESS;
                auto vRetBytes  = sizeof(ULONG);
                auto vIrpStack  = IoGetCurrentIrpStackLocation(vIrp);
                auto vInBytes   = vIrpStack->Parameters.DeviceIoControl.InputBufferLength;
                auto vOutBytes  = vIrpStack->Parameters.DeviceIoControl.OutputBufferLength;

                if (vInBytes  != sizeof(ULONG) || 
                    vOutBytes != sizeof(ULONG))
                {
                    vResult   = STATUS_INVALID_PARAMETER;
                    vRetBytes = 0ul;
                }

                vIrp->IoStatus.Status       = vResult;
                vIrp->IoStatus.Information  = vRetBytes;
                IoCompleteRequest(vIrp, IO_NO_INCREMENT);
                break;
            }
            KeAcquireSpinLockAtDpcLevel(&vContext->QueueLock);
        }
        else
        {
            //
            // Cancel routine will run as soon as we release the lock.
            // So let it complete the request and free the record.
            //
            InitializeListHead(&vRecord->ListEntry);
            vRecord->CancelRoutineFreeMemory = TRUE;
            vRecord = nullptr;
        }
    }
    else
    {
        //
        // Cancel routine has run and completed the IRP. So just free
        // the record.
        //
        NT_ASSERT(vRecord->CancelRoutineFreeMemory == FALSE);
    }
    KeReleaseSpinLockFromDpcLevel(&vContext->QueueLock);

    //
    // Free the memory outside the lock for better performance.
    //
    if (vRecord != nullptr)
    {
        ExFreePoolWithTag(vRecord, IOCPCTL_TAG);
        vRecord = nullptr;
    }
}

static 
auto IOCtlIoPending(PDEVICE_OBJECT /*aDeviceObject*/, PIRP aIrp) -> NTSTATUS
{
    auto vIrp       = IoGetCurrentIrpStackLocation(aIrp);
    auto vRecord    = PNOTIFY_RECORD(nullptr);

    NT_ASSERT(vIrp->FileObject != nullptr);
    auto vContext = static_cast<PFILE_CONTEXT>(vIrp->FileObject->FsContext);

    vRecord = static_cast<PNOTIFY_RECORD>(ExAllocatePoolWithQuotaTag(
        NonPagedPool, sizeof(*vRecord), IOCPCTL_TAG));
    if (vRecord == nullptr)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    InitializeListHead(&vRecord->ListEntry);

    vRecord->PendingIrp     = aIrp;
    vRecord->FileObject     = vIrp->FileObject;
    vRecord->FileContext    = vContext;
    vRecord->CancelRoutineFreeMemory = FALSE;

    //
    // Start the timer to run the CustomTimerDPC in DueTime seconds to
    // simulate an interrupt (which would queue a DPC).
    // The IRP is completed in the DPC to notify the hardware event.
    //

    KeInitializeDpc(
        &vRecord->Dpc,  // Dpc
        CustomTimerDPC, // DeferredRoutine
        vRecord         // DeferredContext
    );
    KeInitializeTimer(&vRecord->Timer);

    //
    // We will set the cancel routine and TimerDpc within the
    // lock so that they don't modify the list before we are
    // completely done.
    //
    auto vIrql = KIRQL();
    KeAcquireSpinLock(&vContext->QueueLock, &vIrql);

    //
    // Set the cancel routine. This is required if the app decides to
    // exit or cancel the event prematurely.
    //
    IoSetCancelRoutine(aIrp, CancelRoutine);

    //
    // Before we queue the IRP, we must check to see if it's cancelled.
    //
    if (aIrp->Cancel) {

        //
        // Clear the cancel-routine automically and check the return value.
        // We will complete the IRP here if we succeed in clearing it. If
        // we fail then we will let the cancel-routine complete it.
        //
        if (IoSetCancelRoutine(aIrp, nullptr) != nullptr) {

            //
            // We are able to successfully clear the routine. Either the
            // the IRP is cancelled before we set the cancel-routine or
            // we won the race with I/O manager in clearing the routine.
            // Return STATUS_CANCELLED so that the caller can complete
            // the request.

            KeReleaseSpinLock(&vContext->QueueLock, vIrql);

            ExFreePoolWithTag(vRecord, IOCPCTL_TAG);

            return STATUS_CANCELLED;
        }
        else {
            //
            // The IRP got cancelled after we set the cancel-routine and the
            // I/O manager won the race in clearing it and called the cancel
            // routine. So queue the request so that cancel-routine can dequeue
            // and complete it. Note the cancel-routine cannot run until we
            // drop the queue lock.
            //
        }
    }

    IoMarkIrpPending(aIrp);

    InsertTailList(&vContext->QueueHead, &vRecord->ListEntry);

    vRecord->CancelRoutineFreeMemory = FALSE;

    //
    // We will save the record pointer in the IRP so that we can get to
    // it directly in the CancelRoutine.
    //
    aIrp->Tail.Overlay.DriverContext[3] = vRecord;
    
    //
    // Pretending to be very busy
    //

    auto vDueTime       = LARGE_INTEGER();
    auto vMilliseconds  = (__rdtsc() & reinterpret_cast<size_t>(aIrp)) % 10000;
    vDueTime.QuadPart   = Int32x32To64(vMilliseconds, -10000);
    
    KeSetTimer(
        &vRecord->Timer,    // Timer
        vDueTime,           // DueTime
        &vRecord->Dpc       // Dpc
    );

    KeReleaseSpinLock(&vContext->QueueLock, vIrql);
    return STATUS_PENDING;
}

static
auto DeviceCreate(PDEVICE_OBJECT /*aDeviceObject*/, PIRP aIrp) -> NTSTATUS
{
    auto vResult        = STATUS_SUCCESS;
    auto vIrp           = IoGetCurrentIrpStackLocation(aIrp);
    auto vFileObject    = vIrp->FileObject;

    for (;;)
    {
        NT_ASSERT(vIrp->FileObject != nullptr);

        auto vContext = static_cast<PFILE_CONTEXT>(ExAllocatePoolWithQuotaTag(
            NonPagedPool, sizeof(FILE_CONTEXT), IOCPCTL_TAG));
        if (vContext == nullptr)
        {
            vResult = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        IoInitializeRemoveLock  (&vContext->FileRundownLock, IOCPCTL_TAG, 0, 0);
        KeInitializeSpinLock    (&vContext->QueueLock);
        InitializeListHead      (&vContext->QueueHead);

        //
        // Make sure nobody is using the FsContext scratch area.
        // And
        // Store the context in the FileObject's scratch area.
        //
        NT_ASSERT(vFileObject->FsContext == nullptr);

        vFileObject->FsContext = vContext;
        break;
    }

    aIrp->IoStatus.Status       = vResult;
    aIrp->IoStatus.Information  = 0;
    return IoCompleteRequest(aIrp, IO_NO_INCREMENT), vResult;
}

static
auto DeviceClose(PDEVICE_OBJECT /*aDeviceObject*/, PIRP aIrp) -> NTSTATUS
{
    auto vResult        = STATUS_SUCCESS;
    auto vIrp           = IoGetCurrentIrpStackLocation(aIrp);
    auto vFileObject    = vIrp->FileObject;

    for (;;)
    {
        NT_ASSERT(vIrp->FileObject != nullptr);
        
        auto& vContext = vFileObject->FsContext;

        ExFreePoolWithTag(vContext, IOCPCTL_TAG);
        vContext = nullptr;
        break;
    }

    aIrp->IoStatus.Status       = vResult;
    aIrp->IoStatus.Information  = 0;
    return IoCompleteRequest(aIrp, IO_NO_INCREMENT), vResult;
}

static
auto DeviceCleanup(PDEVICE_OBJECT /*aDeviceObject*/, PIRP aIrp) -> NTSTATUS
{
    auto vResult        = STATUS_SUCCESS;
    auto vIrp           = IoGetCurrentIrpStackLocation(aIrp);
    auto vFileObject    = vIrp->FileObject;
    auto vCleanupQueue  = LIST_ENTRY();
    auto vIrql          = KIRQL();

    NT_ASSERT(vIrp->FileObject != nullptr);
    auto vContext = static_cast<PFILE_CONTEXT>(vFileObject->FsContext);

    //
    // This acquire cannot fail because you cannot get more than one
    // cleanup for the same handle.
    //
    vResult = IoAcquireRemoveLock(&vContext->FileRundownLock, aIrp);
    NT_ASSERT(NT_SUCCESS(vResult));
    if (!NT_SUCCESS(vResult))
    {
        aIrp->IoStatus.Status       = vResult;
        aIrp->IoStatus.Information  = 0;
        return IoCompleteRequest(aIrp, IO_NO_INCREMENT), vResult;
    }

    //
    // Wait for all the threads that are currently dispatching to exit and 
    // prevent any threads dispatching I/O on the same handle beyond this point.
    //
    IoReleaseRemoveLockAndWait(&vContext->FileRundownLock, aIrp);

    InitializeListHead(&vCleanupQueue);

    //
    // Walk the list and remove all the pending notification records
    // that belong to this filehandle.
    //

    KeAcquireSpinLock(&vContext->QueueLock, &vIrql);

    auto vHead  = &vContext->QueueHead;
    auto vEntry = PLIST_ENTRY();
    auto vNext  = PLIST_ENTRY();

    for (vEntry = vHead->Flink; vEntry != vHead; vEntry = vNext)
    {
        vNext = vEntry->Flink;

        auto vRecord = CONTAINING_RECORD(vEntry, NOTIFY_RECORD, ListEntry);

        //
        // KeCancelTimer returns if the timer is successfully cancelled.
        // If it returns FALSE, there are two possibilities. Either the
        // TimerDpc has just run and waiting to acquire the lock or it
        // has run to completion. We wouldn't be here if it had run to
        // completion because we wouldn't found the record in the list.
        // So the only possibility is that it's waiting to acquire the lock.
        // In that case, we will just let the DPC to complete the request
        // and free the record.
        //
        if (!KeCancelTimer(&vRecord->Timer))
        {
            continue;
        }
        RemoveEntryList(vEntry);

        //
        // Clear the cancel-routine and check the return value to
        // see whether it was cleared by us or by the I/O manager.
        //
        if (IoSetCancelRoutine(vRecord->PendingIrp, nullptr) != nullptr)
        {
            //
            // We cleared it and as a result we own the IRP and
            // nobody can cancel it anymore. We will queue the IRP
            // in the local cleanup list so that we can complete
            // all the IRPs outside the lock to avoid deadlocks in
            // the completion routine of the driver above us re-enters
            // our driver.
            //
            InsertTailList(&vCleanupQueue, &vRecord->PendingIrp->Tail.Overlay.ListEntry);

            ExFreePoolWithTag(vRecord, IOCPCTL_TAG);
        }
        else {
            //
            // The I/O manager cleared it and called the cancel-routine.
            // Cancel routine is probably waiting to acquire the lock.
            // So reinitialze the ListEntry so that it doesn't crash
            // when it tries to remove the entry from the list and
            // set the CancelRoutineFreeMemory to indicate that it should
            // free the notification record.
            //
            InitializeListHead(&vRecord->ListEntry);
            vRecord->CancelRoutineFreeMemory = TRUE;
        }
    }
    KeReleaseSpinLock(&vContext->QueueLock, vIrql);

    //
    // Walk through the cleanup list and cancel all
    // the IRPs.
    //
    while (!IsListEmpty(&vCleanupQueue))
    {
        auto vPendingIrp = PIRP(nullptr);

        //
        // Complete the IRP
        //
        vEntry = RemoveHeadList(&vCleanupQueue);
        vPendingIrp = CONTAINING_RECORD(vEntry, IRP, Tail.Overlay.ListEntry);

        vPendingIrp->Tail.Overlay.DriverContext[3] = NULL;
        vPendingIrp->IoStatus.Information   = 0;
        vPendingIrp->IoStatus.Status        = STATUS_CANCELLED;

        IoCompleteRequest(vPendingIrp, IO_NO_INCREMENT);
    }

    aIrp->IoStatus.Status       = vResult;
    aIrp->IoStatus.Information  = 0;
    return IoCompleteRequest(aIrp, IO_NO_INCREMENT), vResult;
}

static
auto DeviceControl(PDEVICE_OBJECT aDeviceObject, PIRP aIrp) -> NTSTATUS
{
    auto vResult        = STATUS_SUCCESS;
    auto vIrp           = IoGetCurrentIrpStackLocation(aIrp);

    NT_ASSERT(vIrp->FileObject != nullptr);

    auto vContext = static_cast<PFILE_CONTEXT>(vIrp->FileObject->FsContext);

    vResult = IoAcquireRemoveLock(&vContext->FileRundownLock, aIrp);
    if (!NT_SUCCESS(vResult))
    {   
        //
        // Lock is in a removed state. That means we have already received 
        // cleaned up request for this handle. 
        //

        aIrp->IoStatus.Status = vResult;
        return IoCompleteRequest(aIrp, IO_NO_INCREMENT), vResult;
    }

    switch (vIrp->Parameters.DeviceIoControl.IoControlCode)
    {
    default:
        NT_ASSERTMSG("IoControlCode Does Not Support!", FALSE);
        vResult = STATUS_NOT_IMPLEMENTED;
        break;

    case IOCTL_IO_BUFFERED:
        vResult = IOCtlIoPending(aDeviceObject, aIrp);
        break;
    }

    if (vResult != STATUS_PENDING) {
        //
        // complete the Irp
        //
        aIrp->IoStatus.Status       = vResult;
        aIrp->IoStatus.Information  = 0;
        IoCompleteRequest(aIrp, IO_NO_INCREMENT);
    }

    //
    // We don't hold the lock for IRP that's pending in the list because this
    // lock is meant to rundown currently dispatching threads when the cleanup
    // is handled.
    //
    IoReleaseRemoveLock(&vContext->FileRundownLock, aIrp);

    return vResult;
}

static
auto DriverUnload(PDRIVER_OBJECT aDriverObject) -> void
{
    auto vDeviceObject  = aDriverObject->DeviceObject;
    auto vSymlinkName   = UNICODE_STRING(RTL_CONSTANT_STRING(IOCPCTL_DOS_NAME));

    if (vDeviceObject)
    {
        IoDeleteSymbolicLink(&vSymlinkName);
        IoDeleteDevice(vDeviceObject);
    }
}

extern"C"
auto DriverEntry(PDRIVER_OBJECT aDriverObject, PUNICODE_STRING /*aRegistryPath*/) -> NTSTATUS
{
    auto vResult = STATUS_SUCCESS;

    for (;;)
    {
        ExInitializeDriverRuntime(DrvRtPoolNxOptIn);

        aDriverObject->DriverUnload                         = DriverUnload;
        aDriverObject->MajorFunction[IRP_MJ_CREATE]         = DeviceCreate;
        aDriverObject->MajorFunction[IRP_MJ_CLOSE]          = DeviceClose;
        aDriverObject->MajorFunction[IRP_MJ_CLEANUP]        = DeviceCleanup;
        aDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceControl;

        auto vDeviceName    = UNICODE_STRING(RTL_CONSTANT_STRING(IOCPCTL_DEVICE_NAME));
        auto vSymlinkName   = UNICODE_STRING(RTL_CONSTANT_STRING(IOCPCTL_DOS_NAME));
        auto vDeviceObject  = PDEVICE_OBJECT(nullptr);

        vResult = IoCreateDevice(
            aDriverObject,
            0,
            &vDeviceName,
            FILE_DEVICE_UNKNOWN,
            FILE_DEVICE_SECURE_OPEN,
            FALSE,
            &vDeviceObject);
        if (!NT_SUCCESS(vResult))
        {
            break;
        }

        vResult = IoCreateSymbolicLink(&vSymlinkName, &vDeviceName);
        if (!NT_SUCCESS(vResult))
        {
            break;
        }        
        
        vDeviceObject->Flags |= DO_BUFFERED_IO;
        vDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
        break;
    }
    if (!NT_SUCCESS(vResult))
    {
        DriverUnload(aDriverObject);
    }

    return vResult;
}
