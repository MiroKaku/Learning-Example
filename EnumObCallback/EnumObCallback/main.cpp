#include "stdafx.h"


extern"C"   DRIVER_INITIALIZE   DriverEntry;
static      PVOID               s_Cookie = nullptr;


auto ReleaseSystemModule(PRTL_PROCESS_MODULES& aModules) -> void
{
    if (aModules)
    {
        ExFreePoolWithTag(aModules, 'Miro');
        aModules = nullptr;
    }
}

auto ReferenceSystemModules(PRTL_PROCESS_MODULES* aModules) -> NTSTATUS
{
    auto vResult    = STATUS_SUCCESS;
    auto vModules   = PRTL_PROCESS_MODULES(nullptr);

    for (;;)
    {
        auto vNeedBytes = 0ul;
        vResult = ZwQuerySystemInformation(SystemModuleInformation, vModules, vNeedBytes, &vNeedBytes);
        if (0 == vNeedBytes)
        {
            if (NT_SUCCESS(vResult)) vResult = STATUS_INTERNAL_ERROR;
            break;
        }

        vNeedBytes  = vNeedBytes * 2;
        vModules    = (PRTL_PROCESS_MODULES)ExAllocatePoolWithTag(NonPagedPool, vNeedBytes, 'Miro');
        if (nullptr == vModules)
        {
            vResult = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        vResult = ZwQuerySystemInformation(SystemModuleInformation, vModules, vNeedBytes, &vNeedBytes);
        if (!NT_SUCCESS(vResult))
        {
            break;
        }

        *aModules = vModules;
        break;
    }

    if (!NT_SUCCESS(vResult))
    {
        ReleaseSystemModule(vModules);
    }

    return vResult;
}

auto GetModuleNameByAddress(PVOID aAddress, LPSTR aModuleName, ULONG aNameBytes) -> NTSTATUS
{
    auto vResult    = STATUS_SUCCESS;
    auto vModInfo   = PRTL_PROCESS_MODULES(nullptr);

    for (;;)
    {
        if (!MmIsAddressValid(aAddress))
        {
            vResult = STATUS_INVALID_ADDRESS;
        }

        vResult = ReferenceSystemModules(&vModInfo);
        if (!NT_SUCCESS(vResult))
        {
            break;
        }

        auto vModules = vModInfo->Modules;
        for (size_t i = 0; i < vModInfo->NumberOfModules; i++)
        {
            if (aAddress > vModules[i].ImageBase &&
                aAddress < (static_cast<UINT8*>(vModules[i].ImageBase) + vModules[i].ImageSize))
            {
                RtlStringCbCopyA(aModuleName, aNameBytes, (LPSTR)(vModules[i].FullPathName));
                break;
            }
        }

        break;
    }

    ReleaseSystemModule(vModInfo);
    return vResult;
}

template<typename Fn>
auto EnumObCallback(PVOID aObCookie, Fn aCallback) -> NTSTATUS
{
    auto vResult    = STATUS_SUCCESS;
    auto vCbHeader  = static_cast<POB_CALLBACK_OBJECT_HEADER>(aObCookie);
    auto vCbBody    = vCbHeader->Body;

    do
    {
        auto vCurrentHeader = vCbBody->CallbackObject;

        if (!MmIsAddressValid(vCurrentHeader) ||
            vCurrentHeader->Version != ObGetFilterVersion() ||
            static_cast<int>(vCurrentHeader->BodyCount) <= 0)
        {
            // Skip head-node
            vCbBody = CONTAINING_RECORD(vCbBody->ListEntry.Flink, OB_CALLBACK_OBJECT_BODY, ListEntry);
            continue;
        }

        if (aCallback(vCurrentHeader))
        {
            break;
        }

        vCbBody = CONTAINING_RECORD(vCbBody->ListEntry.Flink, OB_CALLBACK_OBJECT_BODY, ListEntry);

    } while (vCbBody != vCbHeader->Body);

    return vResult;
}

static auto PreCallback(PVOID /*aContext*/, POB_PRE_OPERATION_INFORMATION /*aInfo*/)
->OB_PREOP_CALLBACK_STATUS
{
    return OB_PREOP_CALLBACK_STATUS::OB_PREOP_SUCCESS;
}

auto DriverUnload(PDRIVER_OBJECT /*aDriverObject*/) -> void
{

}

auto DriverEntry(PDRIVER_OBJECT aDriverObject, PUNICODE_STRING /*aRegistryPath*/) -> NTSTATUS
{
    auto vResult = STATUS_SUCCESS;

    for (;;)
    {
        DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL, 
            "[EnumObCallback] start!\n\n");

        OB_OPERATION_REGISTRATION vOpArgs[]
        {
            {
                PsProcessType,
                OB_OPERATION_HANDLE_CREATE,
                PreCallback,
            },
        { nullptr }
        };

        auto vArgs = OB_CALLBACK_REGISTRATION();
        vArgs.Version                       = ObGetFilterVersion();
        vArgs.Altitude                      = RTL_CONSTANT_STRING(L"372102");
        vArgs.RegistrationContext           = PVOID(372102);
        vArgs.OperationRegistrationCount    = 1;
        vArgs.OperationRegistration         = vOpArgs;

        vResult = ObRegisterCallbacks(&vArgs, &s_Cookie);
        if (!NT_SUCCESS(vResult))
        {
            break;
        }

        vResult = EnumObCallback(
            s_Cookie, 
            [](POB_CALLBACK_OBJECT_HEADER aCbObject) -> bool
            {
                enum { MaxObName = 1024, MaxModulePath = 260 };

                auto vResult        = STATUS_SUCCESS;
                auto vObjectName    = POBJECT_NAME_INFORMATION(nullptr);
                char vPreModulePath [MaxModulePath] = { 0 };
                char vPostModulePath[MaxModulePath] = { 0 };


                vObjectName = (POBJECT_NAME_INFORMATION)ExAllocatePoolWithTag(NonPagedPool, MaxObName, 'Miro');
                if (nullptr == vObjectName)
                {
                    return false;
                }

                DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
                    "[EnumObCallback] Found a Callback Header at 0x%p, Context: 0x%p(%Iu), Altitude: %wZ, %d Callback Bodies:\n",
                    aCbObject,
                    aCbObject->RegistrationContext, (size_t)aCbObject->RegistrationContext,
                    aCbObject->Altitude,
                    aCbObject->BodyCount
                );

                for (size_t i = 0; i < aCbObject->BodyCount; i++)
                {
                    auto& vCbBody   = aCbObject->Body[i];
                    auto  vRetBytes = 0ul;

                    // Get the ObjectType name
                    RtlInitEmptyUnicodeString(&vObjectName->Name, (PWCHAR)((PUINT8)vObjectName + sizeof(*vObjectName)), MaxObName - sizeof(*vObjectName));
                    vResult = ObQueryNameString(vCbBody.ObjectType, vObjectName, MaxObName, &vRetBytes);
                    if (!NT_SUCCESS(vResult))
                    {
                        continue;
                    }

                    // Get the module where PreCallbackRoutine  is located
                    vResult = GetModuleNameByAddress(vCbBody.PreOperation, vPreModulePath, sizeof(vPreModulePath));
                    if (!NT_SUCCESS(vResult))
                    {
                        vPreModulePath[0] = 0;
                    }

                    // Get the module where PostCallbackRoutine is located
                    vResult = GetModuleNameByAddress(vCbBody.PostOperation, vPostModulePath, sizeof(vPostModulePath));
                    if (!NT_SUCCESS(vResult))
                    {
                        vPostModulePath[0] = 0;
                    }

                    // Print
                    DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
                        "[EnumObCallback]     No.%Iu Callback Body at: 0x%p\n"
                        "[EnumObCallback]         ObjectType: %wZ, Operations: %x\n"
                        "[EnumObCallback]         PreOperation: 0x%p (%s)\n"
                        "[EnumObCallback]         PostOperation: 0x%p (%s)\n\n",
                        i,
                        &vCbBody,
                        &vObjectName->Name,
                        vCbBody.Operations,
                        vCbBody.PreOperation , vPreModulePath,
                        vCbBody.PostOperation, vPostModulePath);
                }

                ExFreePoolWithTag(vObjectName, 'Miro');
                return false;
            });
        ObUnRegisterCallbacks(s_Cookie), s_Cookie = nullptr;

        DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
            "[EnumObCallback] over!\n");

        aDriverObject->DriverUnload = DriverUnload;
        break;
    }

    return vResult;
}
