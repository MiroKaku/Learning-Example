#include "stdafx.h"
#include "Schtasks.h"

// lib
#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsupp.lib")
#pragma comment(lib, "comsuppw.lib")


static
auto SplitTaskName(
    _In_        bstr_t  aTaskFullName,
    _Out_       bstr_t* aTaskName,
    _Out_       bstr_t* aTaskFolder
)->VOID
{
    auto vToken = _tcsrchr(aTaskFullName.GetBSTR(), _T('\\'));
    if (vToken == nullptr)
    {
        *aTaskFolder = std::move(bstr_t("\\"));
        *aTaskName   = std::move(bstr_t(aTaskFullName, true));
    }
    else
    {
        if (vToken == aTaskFullName.GetBSTR())
        {
            *aTaskFolder = std::move(bstr_t("\\"));
        }
        vToken = _T('\0'), ++vToken;
        *aTaskName = std::move(bstr_t(vToken, true));
    }
}

Schtasks::Schtasks()
{
    auto vResult = S_OK;

    vResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(vResult))
    {
        throw std::runtime_error("Schtasks::ctor().CoInitializeEx() failed!");
    }
    vResult = CoInitializeSecurity(
        nullptr,
        -1,
        nullptr,
        nullptr,
        RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr,
        0,
        nullptr);
    if (FAILED(vResult) && vResult != RPC_E_TOO_LATE)
    {
        throw std::runtime_error("Schtasks::ctor().CoInitializeSecurity() failed!");
    }

    vResult = CoCreateInstance(
        CLSID_TaskScheduler,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_ITaskService,
        (void**)& _TaskService);
    if (FAILED(vResult))
    {
        throw std::runtime_error("Schtasks::ctor() CoCreateInstance(IID_ITaskService) failed!");
    }
    vResult = _TaskService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
    if (FAILED(vResult))
    {
        throw std::runtime_error("Schtasks::ctor().ITaskService::Connect() failed!");
    }
    vResult = _TaskService->GetFolder(bstr_t("\\"), &_TaskFolder);
    if (FAILED(vResult))
    {
        throw std::runtime_error("Schtasks::ctor().ITaskService::GetFolder('\') failed!");
    }
}

Schtasks::Schtasks(_In_ CComPtr<ITaskService>& aTaskService)
    : _TaskService(aTaskService)
{ }

Schtasks::Schtasks(_In_ CComPtr<ITaskService> & aTaskService, _In_ CComPtr<ITaskFolder>& aTaskFolder)
    : _TaskService(aTaskService)
    , _TaskFolder(aTaskFolder)
{ }

Schtasks::Schtasks(_In_ const Schtasks& aOther) noexcept
    : _TaskService(aOther._TaskService)
    , _TaskFolder(aOther._TaskFolder)
    , _Task(aOther._Task)
{ }

Schtasks::Schtasks(_Inout_ Schtasks && aOther) noexcept
    : _TaskService(std::move(aOther._TaskService))
    , _TaskFolder(std::move(aOther._TaskFolder))
    , _Task(std::move(aOther._Task))
{ }

auto Schtasks::operator=(_In_ const Schtasks & aOther) noexcept -> Schtasks &
{
    Schtasks(aOther).Swap(*this);
    return *this;
}

auto Schtasks::operator=(_Inout_ Schtasks&& aOther) noexcept -> Schtasks &
{
    Schtasks(std::move(aOther)).Swap(*this);
    return *this;
}

auto Schtasks::Swap(_Inout_ Schtasks& aOther) noexcept -> void
{
    std::swap(_TaskService  , aOther._TaskService);
    std::swap(_TaskFolder   , aOther._TaskFolder);
    std::swap(_Task         , aOther._Task);
}

auto Schtasks::OpenTask(
    _In_        BSTR aTaskName
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTask = CComPtr<IRegisteredTask>();
        vResult = _TaskFolder->GetTask(aTaskName, &vTask);
        if (FAILED(vResult))
        {
            break;
        }

        _Task = std::move(vTask);
        break;
    }

    return vResult;
}

auto Schtasks::CreateTask(
    _In_        BSTR aTaskName,
    _In_        BSTR aTaskRun,
    _In_opt_    BSTR aTaskRunArgs,
    _In_opt_    bool aUpdateIfExists,
    _In_opt_    BSTR aUserId,
    _In_opt_    BSTR aPassword,
    _In_opt_    TASK_LOGON_TYPE aLogonType,
    _In_opt_    BSTR aSddl
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskDef = CComPtr<ITaskDefinition>();
        vResult = _TaskService->NewTask(0, &vTaskDef);
        if (FAILED(vResult))
        {
            break;
        }

        auto vTaskActionCollection = CComPtr<IActionCollection>();
        vResult = vTaskDef->get_Actions(&vTaskActionCollection);
        if (FAILED(vResult))
        {
            break;
        }

        auto vTaskAction = CComPtr<IAction>();
        vResult = vTaskActionCollection->Create(TASK_ACTION_EXEC, &vTaskAction);
        if (FAILED(vResult))
        {
            break;
        }

        auto vTaskActionExec = CComPtr<IExecAction>();
        vResult = vTaskAction->QueryInterface(IID_IExecAction, (void**)& vTaskActionExec);
        if (FAILED(vResult))
        {
            break;
        }

        vResult = vTaskActionExec->put_Path(aTaskRun);
        if (FAILED(vResult))
        {
            break;
        }

        if (aTaskRunArgs)
        {
            vResult = vTaskActionExec->put_Arguments(aTaskRunArgs);
            if (FAILED(vResult))
            {
                break;
            }
        }
        
        auto vTask = CComPtr<IRegisteredTask>();
        vResult = _TaskFolder->RegisterTaskDefinition(
            aTaskName,
            vTaskDef,
            aUpdateIfExists ? TASK_CREATE_OR_UPDATE : TASK_CREATE,
            aUserId         ? variant_t(aUserId)    : variant_t(),
            aPassword       ? variant_t(aPassword)  : variant_t(),
            aLogonType,
            aSddl           ? variant_t(aSddl)      : variant_t(""),
            &vTask);
        if (FAILED(vResult))
        {
            break;
        }

        _Task       = std::move(vTask);
        _TaskDef    = std::move(vTaskDef);
        break;
    }

    return vResult;
}

auto Schtasks::DeleteTask(
    _In_        BSTR aTaskName
) -> HRESULT
{
    return _TaskFolder->DeleteTask(aTaskName, 0);;
}

auto Schtasks::OpenFolder(
    _In_        BSTR aFolder
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskFolder = CComPtr<ITaskFolder>();
        vResult = _TaskService->GetFolder(aFolder, &vTaskFolder);
        if (FAILED(vResult))
        {
            break;
        }

        _TaskFolder = std::move(vTaskFolder);
        break;
    }

    return vResult;
}

auto Schtasks::CreateFolder(
    _In_        BSTR aFolder,
    _In_opt_    BSTR aSddl,
    _Out_opt_   Schtasks* aNewFolder
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskFolder = CComPtr<ITaskFolder>();
        vResult = _TaskFolder->CreateFolder(
            bstr_t(aFolder),
            aSddl ? variant_t(aSddl) : variant_t(""),
            &vTaskFolder);
        if (FAILED(vResult))
        {
            break;
        }

        if (aNewFolder)
        {
            *aNewFolder = Schtasks(_TaskService, vTaskFolder);
        }
        break;
    }

    return vResult;
}

auto Schtasks::DeleteFolder(
    _In_        BSTR aFolder
) -> HRESULT
{
    return _TaskFolder->DeleteFolder(aFolder, 0);
}

auto Schtasks::RunTask(
    _In_opt_    BSTR aArgs
) -> HRESULT
{
    auto vTaskRunning = CComPtr<IRunningTask>();

    return _Task->Run(
        aArgs ? variant_t(aArgs) : variant_t(), 
        &vTaskRunning);
}

auto Schtasks::ApplyChange() -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskName = BSTR();
        vResult = _Task->get_Path(&vTaskName);
        if (FAILED(vResult))
        {
            break;
        }

        auto vTask = CComPtr<IRegisteredTask>();
        vResult = _TaskFolder->RegisterTaskDefinition(
            vTaskName,
            _TaskDef,
            TASK_UPDATE,
            variant_t(),
            variant_t(),
            TASK_LOGON_NONE,
            variant_t(""),
            &vTask);
        if (FAILED(vResult))
        {
            break;
        }

        _Task   = std::move(vTask);
        break;
    }

    return vResult;
}

auto Schtasks::SetTaskRegistrationInfo(
    _In_opt_    BSTR aAuthor,
    _In_opt_    BSTR aDescription,
    _In_opt_    BSTR aVersion,
    _In_opt_    BSTR aSddl
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskRegistrationInfo = CComPtr<IRegistrationInfo>();
        vResult = _TaskDef->get_RegistrationInfo(&vTaskRegistrationInfo);
        if (FAILED(vResult))
        {
            break;
        }
        if (aAuthor)
        {
            vResult = vTaskRegistrationInfo->put_Author(aAuthor);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aDescription)
        {
            vResult = vTaskRegistrationInfo->put_Description(aDescription);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aVersion)
        {
            vResult = vTaskRegistrationInfo->put_Version(aVersion);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aSddl)
        {
            vResult = vTaskRegistrationInfo->put_SecurityDescriptor(variant_t(aSddl));
            if (FAILED(vResult))
            {
                break;
            }
        }        

        break;
    }

    return vResult;
}

auto Schtasks::GetTaskRegistrationInfo(
    _Out_opt_   BSTR* aAuthor,
    _Out_opt_   BSTR* aDescription,
    _Out_opt_   BSTR* aVersion,
    _Out_opt_   BSTR* aSddl
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskRegistrationInfo = CComPtr<IRegistrationInfo>();
        vResult = _TaskDef->get_RegistrationInfo(&vTaskRegistrationInfo);
        if (FAILED(vResult))
        {
            break;
        }
        if (aAuthor)
        {
            vResult = vTaskRegistrationInfo->get_Author(aAuthor);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aDescription)
        {
            vResult = vTaskRegistrationInfo->get_Description(aDescription);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aVersion)
        {
            vResult = vTaskRegistrationInfo->get_Version(aVersion);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aSddl)
        {
            auto vSddl = variant_t();
            vResult = vTaskRegistrationInfo->get_SecurityDescriptor(&vSddl);
            if (FAILED(vResult))
            {
                break;
            }
            *aSddl = vSddl.bstrVal;
        }

        break;
    }

    return vResult;
}

auto Schtasks::SetTaskDisplayName(
    _In_        BSTR aTaskDisplayName
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskPrincipal = CComPtr<IPrincipal>();
        vResult = _TaskDef->get_Principal(&vTaskPrincipal);
        if (FAILED(vResult))
        {
            break;
        }

        vResult = vTaskPrincipal->put_DisplayName(aTaskDisplayName);
        if (FAILED(vResult))
        {
            break;
        }

        break;
    }

    return vResult;
}

auto Schtasks::SetTaskRunlevel(
    _In_        TASK_RUNLEVEL_TYPE aRunlevel
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskPrincipal = CComPtr<IPrincipal>();
        vResult = _TaskDef->get_Principal(&vTaskPrincipal);
        if (FAILED(vResult))
        {
            break;
        }

        vResult = vTaskPrincipal->put_RunLevel(aRunlevel);
        if (FAILED(vResult))
        {
            break;
        }

        break;
    }

    return vResult;
}

auto Schtasks::SetTaskLogon(
    _In_        TASK_LOGON_TYPE aLogonType,
    _In_opt_    BSTR aUserId,
    _In_opt_    BSTR aGroupId
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskPrincipal = CComPtr<IPrincipal>();
        vResult = _TaskDef->get_Principal(&vTaskPrincipal);
        if (FAILED(vResult))
        {
            break;
        }

        vResult = vTaskPrincipal->put_LogonType(aLogonType);
        if (FAILED(vResult))
        {
            break;
        }

        if (aUserId)
        {
            vResult = vTaskPrincipal->put_UserId(aUserId);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aGroupId)
        {
            vResult = vTaskPrincipal->put_GroupId(aGroupId);
            if (FAILED(vResult))
            {
                break;
            }
        }

        break;
    }

    return vResult;
}

auto Schtasks::GetTaskPrincipal(
    _Out_opt_   BSTR* aTaskName,
    _Out_opt_   TASK_RUNLEVEL_TYPE* aRunlevel,
    _Out_opt_   TASK_LOGON_TYPE* aLogonType,
    _Out_opt_   BSTR* aUserId,
    _Out_opt_   BSTR* aGroupId
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskPrincipal = CComPtr<IPrincipal>();
        vResult = _TaskDef->get_Principal(&vTaskPrincipal);
        if (FAILED(vResult))
        {
            break;
        }

        if (aTaskName)
        {
            vResult = vTaskPrincipal->get_DisplayName(aTaskName);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aRunlevel)
        {
            vResult = vTaskPrincipal->get_RunLevel(aRunlevel);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aLogonType)
        {
            vResult = vTaskPrincipal->get_LogonType(aLogonType);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aUserId)
        {
            vResult = vTaskPrincipal->get_UserId(aUserId);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aGroupId)
        {
            vResult = vTaskPrincipal->get_GroupId(aGroupId);
            if (FAILED(vResult))
            {
                break;
            }
        }

        break;
    }

    return vResult;
}

auto Schtasks::CreateTaskActionExec(
    _In_        BSTR aTaskRun,
    _In_opt_    BSTR aTaskRunArgs,
    _In_opt_    BSTR aTaskRunWorkingDirectory,
    _In_opt_    VARIANT_BOOL aHideWindow
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskActionCollection = CComPtr<IActionCollection>();
        vResult = _TaskDef->get_Actions(&vTaskActionCollection);
        if (FAILED(vResult))
        {
            break;
        }

        auto vTaskAction = CComPtr<IAction>();
        vResult = vTaskActionCollection->Create(TASK_ACTION_EXEC, &vTaskAction);
        if (FAILED(vResult))
        {
            break;
        }

        auto vTaskActionExec = CComPtr<IExecAction>();
        vResult = vTaskAction->QueryInterface(IID_IExecAction, (void**)& vTaskActionExec);
        if (FAILED(vResult))
        {
            break;
        }

        vResult = vTaskActionExec->put_Path(aTaskRun);
        if (FAILED(vResult))
        {
            break;
        }

        if (aTaskRunArgs)
        {
            vResult = vTaskActionExec->put_Arguments(aTaskRunArgs);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aTaskRunWorkingDirectory)
        {
            vResult = vTaskActionExec->put_WorkingDirectory(aTaskRunWorkingDirectory);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aHideWindow)
        {
            auto vTaskActionExec2 = CComPtr<IExecAction2>();
            vResult = vTaskAction->QueryInterface(IID_IExecAction, (void**)& vTaskActionExec2);
            if (FAILED(vResult))
            {
                break;
            }

            vResult = vTaskActionExec2->put_HideAppWindow(aHideWindow);
            if (FAILED(vResult))
            {
                break;
            }
        }
        break;
    }

    return vResult;
}

auto Schtasks::CreateTaskTriggerIdle(
    _In_opt_    BSTR aRepetitionInterval,
    _In_opt_    BSTR aRepetitionDuration,
    _In_opt_    BSTR aExecutionTimeLimit,
    _In_opt_    BSTR aStartBoundary,
    _In_opt_    BSTR aEndBoundary,
    _In_opt_    VARIANT_BOOL aStopAtDurationEnd
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskTriggerCollection = CComPtr<ITriggerCollection>();
        vResult = _TaskDef->get_Triggers(&vTaskTriggerCollection);
        if (FAILED(vResult))
        {
            break;
        }

        auto vTaskTrigger = CComPtr<ITrigger>();
        vResult = vTaskTriggerCollection->Create(TASK_TRIGGER_IDLE, &vTaskTrigger);
        if (FAILED(vResult))
        {
            break;
        }

        auto vTaskTriggerIdle = CComPtr<IIdleTrigger>();
        vResult = vTaskTrigger->QueryInterface(IID_IIdleTrigger, (void**)& vTaskTriggerIdle);
        if (FAILED(vResult))
        {
            break;
        }

        auto vTaskRepetition = CComPtr<IRepetitionPattern>();
        vResult = vTaskTriggerIdle->get_Repetition(&vTaskRepetition);
        if (FAILED(vResult))
        {
            break;
        }
        
        if (aRepetitionInterval)
        {
            vResult = vTaskRepetition->put_Interval(aRepetitionInterval);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aRepetitionDuration)
        {
            vResult = vTaskRepetition->put_Duration(aRepetitionDuration);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aStopAtDurationEnd)
        {
            vResult = vTaskRepetition->put_StopAtDurationEnd(aStopAtDurationEnd);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aExecutionTimeLimit)
        {
            vResult = vTaskTriggerIdle->put_ExecutionTimeLimit(aExecutionTimeLimit);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aStartBoundary)
        {
            vResult = vTaskTriggerIdle->put_StartBoundary(aStartBoundary);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aEndBoundary)
        {
            vResult = vTaskTriggerIdle->put_EndBoundary(aEndBoundary);
            if (FAILED(vResult))
            {
                break;
            }
        }
        break;
    }

    return vResult;
}

auto Schtasks::CreateTaskTriggerBoot(
    _In_opt_    BSTR aDelay,
    _In_opt_    BSTR aRepetitionInterval,
    _In_opt_    BSTR aRepetitionDuration,
    _In_opt_    BSTR aExecutionTimeLimit,
    _In_opt_    BSTR aStartBoundary,
    _In_opt_    BSTR aEndBoundary,
    _In_opt_    VARIANT_BOOL aStopAtDurationEnd
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskTriggerCollection = CComPtr<ITriggerCollection>();
        vResult = _TaskDef->get_Triggers(&vTaskTriggerCollection);
        if (FAILED(vResult))
        {
            break;
        }

        auto vTaskTrigger = CComPtr<ITrigger>();
        vResult = vTaskTriggerCollection->Create(TASK_TRIGGER_BOOT, &vTaskTrigger);
        if (FAILED(vResult))
        {
            break;
        }

        auto vTaskTriggerBoot = CComPtr<IBootTrigger>();
        vResult = vTaskTrigger->QueryInterface(IID_IBootTrigger, (void**)& vTaskTriggerBoot);
        if (FAILED(vResult))
        {
            break;
        }

        auto vTaskRepetition = CComPtr<IRepetitionPattern>();
        vResult = vTaskTriggerBoot->get_Repetition(&vTaskRepetition);
        if (FAILED(vResult))
        {
            break;
        }

        if (aRepetitionInterval)
        {
            vResult = vTaskRepetition->put_Interval(aRepetitionInterval);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aRepetitionDuration)
        {
            vResult = vTaskRepetition->put_Duration(aRepetitionDuration);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aStopAtDurationEnd)
        {
            vResult = vTaskRepetition->put_StopAtDurationEnd(aStopAtDurationEnd);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aExecutionTimeLimit)
        {
            vResult = vTaskTriggerBoot->put_ExecutionTimeLimit(aExecutionTimeLimit);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aStartBoundary)
        {
            vResult = vTaskTriggerBoot->put_StartBoundary(aStartBoundary);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aEndBoundary)
        {
            vResult = vTaskTriggerBoot->put_EndBoundary(aEndBoundary);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aDelay)
        {
            vResult = vTaskTriggerBoot->put_Delay(aDelay);
            if (FAILED(vResult))
            {
                break;
            }
        }
        break;
    }

    return vResult;
}

auto Schtasks::CreateTaskTriggerLogon(
    _In_opt_    BSTR aUserId,
    _In_opt_    BSTR aDelay,
    _In_opt_    BSTR aRepetitionInterval,
    _In_opt_    BSTR aRepetitionDuration,
    _In_opt_    BSTR aExecutionTimeLimit,
    _In_opt_    BSTR aStartBoundary,
    _In_opt_    BSTR aEndBoundary,
    _In_opt_    VARIANT_BOOL aStopAtDurationEnd
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskTriggerCollection = CComPtr<ITriggerCollection>();
        vResult = _TaskDef->get_Triggers(&vTaskTriggerCollection);
        if (FAILED(vResult))
        {
            break;
        }

        auto vTaskTrigger = CComPtr<ITrigger>();
        vResult = vTaskTriggerCollection->Create(TASK_TRIGGER_LOGON, &vTaskTrigger);
        if (FAILED(vResult))
        {
            break;
        }

        auto vTaskTriggerLogon = CComPtr<ILogonTrigger>();
        vResult = vTaskTrigger->QueryInterface(IID_ILogonTrigger, (void**)& vTaskTriggerLogon);
        if (FAILED(vResult))
        {
            break;
        }

        auto vTaskRepetition = CComPtr<IRepetitionPattern>();
        vResult = vTaskTriggerLogon->get_Repetition(&vTaskRepetition);
        if (FAILED(vResult))
        {
            break;
        }

        if (aRepetitionInterval)
        {
            vResult = vTaskRepetition->put_Interval(aRepetitionInterval);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aRepetitionDuration)
        {
            vResult = vTaskRepetition->put_Duration(aRepetitionDuration);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aStopAtDurationEnd)
        {
            vResult = vTaskRepetition->put_StopAtDurationEnd(aStopAtDurationEnd);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aExecutionTimeLimit)
        {
            vResult = vTaskTriggerLogon->put_ExecutionTimeLimit(aExecutionTimeLimit);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aStartBoundary)
        {
            vResult = vTaskTriggerLogon->put_StartBoundary(aStartBoundary);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aEndBoundary)
        {
            vResult = vTaskTriggerLogon->put_EndBoundary(aEndBoundary);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aUserId)
        {
            vResult = vTaskTriggerLogon->put_UserId(aUserId);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aDelay)
        {
            vResult = vTaskTriggerLogon->put_Delay(aDelay);
            if (FAILED(vResult))
            {
                break;
            }
        }
        break;
    }

    return vResult;
}

auto Schtasks::CreateTaskTriggerSessionStateChange(
    _In_opt_    BSTR aUserId,
    _In_opt_    BSTR aDelay,
    _In_opt_    TASK_SESSION_STATE_CHANGE_TYPE aType,
    _In_opt_    BSTR aRepetitionInterval,
    _In_opt_    BSTR aRepetitionDuration,
    _In_opt_    BSTR aExecutionTimeLimit,
    _In_opt_    BSTR aStartBoundary,
    _In_opt_    BSTR aEndBoundary,
    _In_opt_    VARIANT_BOOL aStopAtDurationEnd
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskTriggerCollection = CComPtr<ITriggerCollection>();
        vResult = _TaskDef->get_Triggers(&vTaskTriggerCollection);
        if (FAILED(vResult))
        {
            break;
        }

        auto vTaskTrigger = CComPtr<ITrigger>();
        vResult = vTaskTriggerCollection->Create(TASK_TRIGGER_SESSION_STATE_CHANGE, &vTaskTrigger);
        if (FAILED(vResult))
        {
            break;
        }

        auto vTaskTriggerSessionStateChange = CComPtr<ISessionStateChangeTrigger>();
        vResult = vTaskTrigger->QueryInterface(IID_ISessionStateChangeTrigger, (void**)& vTaskTriggerSessionStateChange);
        if (FAILED(vResult))
        {
            break;
        }

        auto vTaskRepetition = CComPtr<IRepetitionPattern>();
        vResult = vTaskTriggerSessionStateChange->get_Repetition(&vTaskRepetition);
        if (FAILED(vResult))
        {
            break;
        }

        if (aRepetitionInterval)
        {
            vResult = vTaskRepetition->put_Interval(aRepetitionInterval);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aRepetitionDuration)
        {
            vResult = vTaskRepetition->put_Duration(aRepetitionDuration);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aStopAtDurationEnd)
        {
            vResult = vTaskRepetition->put_StopAtDurationEnd(aStopAtDurationEnd);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aExecutionTimeLimit)
        {
            vResult = vTaskTriggerSessionStateChange->put_ExecutionTimeLimit(aExecutionTimeLimit);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aStartBoundary)
        {
            vResult = vTaskTriggerSessionStateChange->put_StartBoundary(aStartBoundary);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aEndBoundary)
        {
            vResult = vTaskTriggerSessionStateChange->put_EndBoundary(aEndBoundary);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aUserId)
        {
            vResult = vTaskTriggerSessionStateChange->put_UserId(aUserId);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aDelay)
        {
            vResult = vTaskTriggerSessionStateChange->put_Delay(aDelay);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aType)
        {
            vResult = vTaskTriggerSessionStateChange->put_StateChange(aType);
            if (FAILED(vResult))
            {
                break;
            }
        }
        break;
    }

    return vResult;
}

auto Schtasks::CreateTaskTriggerEvent(
    _In_opt_    BSTR aSubscription,
    _In_opt_    BSTR aDelay,
    _In_opt_    BSTR aRepetitionInterval,
    _In_opt_    BSTR aRepetitionDuration,
    _In_opt_    BSTR aExecutionTimeLimit,
    _In_opt_    BSTR aStartBoundary, 
    _In_opt_    BSTR aEndBoundary,
    _In_opt_    VARIANT_BOOL aStopAtDurationEnd
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskTriggerCollection = CComPtr<ITriggerCollection>();
        vResult = _TaskDef->get_Triggers(&vTaskTriggerCollection);
        if (FAILED(vResult))
        {
            break;
        }

        auto vTaskTrigger = CComPtr<ITrigger>();
        vResult = vTaskTriggerCollection->Create(TASK_TRIGGER_EVENT, &vTaskTrigger);
        if (FAILED(vResult))
        {
            break;
        }

        auto vTaskTriggerEvent = CComPtr<IEventTrigger>();
        vResult = vTaskTrigger->QueryInterface(IID_IEventTrigger, (void**)& vTaskTriggerEvent);
        if (FAILED(vResult))
        {
            break;
        }

        auto vTaskRepetition = CComPtr<IRepetitionPattern>();
        vResult = vTaskTriggerEvent->get_Repetition(&vTaskRepetition);
        if (FAILED(vResult))
        {
            break;
        }

        if (aRepetitionInterval)
        {
            vResult = vTaskRepetition->put_Interval(aRepetitionInterval);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aRepetitionDuration)
        {
            vResult = vTaskRepetition->put_Duration(aRepetitionDuration);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aStopAtDurationEnd)
        {
            vResult = vTaskRepetition->put_StopAtDurationEnd(aStopAtDurationEnd);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aExecutionTimeLimit)
        {
            vResult = vTaskTriggerEvent->put_ExecutionTimeLimit(aExecutionTimeLimit);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aStartBoundary)
        {
            vResult = vTaskTriggerEvent->put_StartBoundary(aStartBoundary);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aEndBoundary)
        {
            vResult = vTaskTriggerEvent->put_EndBoundary(aEndBoundary);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aSubscription)
        {
            vResult = vTaskTriggerEvent->put_Subscription(aSubscription);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aDelay)
        {
            vResult = vTaskTriggerEvent->put_Delay(aDelay);
            if (FAILED(vResult))
            {
                break;
            }
        }
        break;
    }

    return vResult;
}

auto Schtasks::CreateTaskTriggerTime(
    _In_opt_    BSTR aRandomDelay,
    _In_opt_    BSTR aRepetitionInterval,
    _In_opt_    BSTR aRepetitionDuration,
    _In_opt_    BSTR aExecutionTimeLimit,
    _In_opt_    BSTR aStartBoundary,
    _In_opt_    BSTR aEndBoundary,
    _In_opt_    VARIANT_BOOL aStopAtDurationEnd
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskTriggerCollection = CComPtr<ITriggerCollection>();
        vResult = _TaskDef->get_Triggers(&vTaskTriggerCollection);
        if (FAILED(vResult))
        {
            break;
        }

        auto vTaskTrigger = CComPtr<ITrigger>();
        vResult = vTaskTriggerCollection->Create(TASK_TRIGGER_TIME, &vTaskTrigger);
        if (FAILED(vResult))
        {
            break;
        }

        auto vTaskTriggerTime = CComPtr<ITimeTrigger>();
        vResult = vTaskTrigger->QueryInterface(IID_ITimeTrigger, (void**)& vTaskTriggerTime);
        if (FAILED(vResult))
        {
            break;
        }

        auto vTaskRepetition = CComPtr<IRepetitionPattern>();
        vResult = vTaskTriggerTime->get_Repetition(&vTaskRepetition);
        if (FAILED(vResult))
        {
            break;
        }

        if (aRepetitionInterval)
        {
            vResult = vTaskRepetition->put_Interval(aRepetitionInterval);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aRepetitionDuration)
        {
            vResult = vTaskRepetition->put_Duration(aRepetitionDuration);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aStopAtDurationEnd)
        {
            vResult = vTaskRepetition->put_StopAtDurationEnd(aStopAtDurationEnd);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aExecutionTimeLimit)
        {
            vResult = vTaskTriggerTime->put_ExecutionTimeLimit(aExecutionTimeLimit);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aStartBoundary)
        {
            vResult = vTaskTriggerTime->put_StartBoundary(aStartBoundary);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aEndBoundary)
        {
            vResult = vTaskTriggerTime->put_EndBoundary(aEndBoundary);
            if (FAILED(vResult))
            {
                break;
            }
        }
        if (aRandomDelay)
        {
            vResult = vTaskTriggerTime->put_RandomDelay(aRandomDelay);
            if (FAILED(vResult))
            {
                break;
            }
        }
        break;
    }

    return vResult;
}

auto Schtasks::SetTaskEnable(
    _In_    VARIANT_BOOL aEnabled
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskSet = CComPtr<ITaskSettings>();
        vResult = _TaskDef->get_Settings(&vTaskSet);
        if (FAILED(vResult))
        {
            break;
        }

        vResult = vTaskSet->put_Enabled(aEnabled);
        if (FAILED(vResult))
        {
            break;
        }
        break;
    }

    return vResult;
}

auto Schtasks::SetTaskHidden(
    _In_    VARIANT_BOOL aHidden
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskSet = CComPtr<ITaskSettings>();
        vResult = _TaskDef->get_Settings(&vTaskSet);
        if (FAILED(vResult))
        {
            break;
        }

        vResult = vTaskSet->put_Hidden(aHidden);
        if (FAILED(vResult))
        {
            break;
        }
        break;
    }

    return vResult;
}

auto Schtasks::SetTaskAllowDemandStart(
    _In_    VARIANT_BOOL aAllowDemandStart
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskSet = CComPtr<ITaskSettings>();
        vResult = _TaskDef->get_Settings(&vTaskSet);
        if (FAILED(vResult))
        {
            break;
        }

        vResult = vTaskSet->put_AllowDemandStart(aAllowDemandStart);
        if (FAILED(vResult))
        {
            break;
        }
        break;
    }

    return vResult;
}

auto Schtasks::SetTaskStopIfGoingOnBatteries(
    _In_    VARIANT_BOOL aStopIfGoingOnBatteries
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskSet = CComPtr<ITaskSettings>();
        vResult = _TaskDef->get_Settings(&vTaskSet);
        if (FAILED(vResult))
        {
            break;
        }

        vResult = vTaskSet->put_StopIfGoingOnBatteries(aStopIfGoingOnBatteries);
        if (FAILED(vResult))
        {
            break;
        }
        break;
    }

    return vResult;
}

auto Schtasks::SetTaskDisallowStartIfOnBatteries(
    _In_    VARIANT_BOOL aDisallowStartIfOnBatteries
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskSet = CComPtr<ITaskSettings>();
        vResult = _TaskDef->get_Settings(&vTaskSet);
        if (FAILED(vResult))
        {
            break;
        }

        vResult = vTaskSet->put_DisallowStartIfOnBatteries(aDisallowStartIfOnBatteries);
        if (FAILED(vResult))
        {
            break;
        }
        break;
    }

    return vResult;
}

auto Schtasks::SetTaskAllowHardTerminate(
    _In_    VARIANT_BOOL aAllowHardTerminate
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskSet = CComPtr<ITaskSettings>();
        vResult = _TaskDef->get_Settings(&vTaskSet);
        if (FAILED(vResult))
        {
            break;
        }

        vResult = vTaskSet->put_AllowHardTerminate(aAllowHardTerminate);
        if (FAILED(vResult))
        {
            break;
        }
        break;
    }

    return vResult;
}

auto Schtasks::SetTaskStartWhenAvailable(
    _In_    VARIANT_BOOL aStartWhenAvailable
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskSet = CComPtr<ITaskSettings>();
        vResult = _TaskDef->get_Settings(&vTaskSet);
        if (FAILED(vResult))
        {
            break;
        }

        vResult = vTaskSet->put_StartWhenAvailable(aStartWhenAvailable);
        if (FAILED(vResult))
        {
            break;
        }
        break;
    }

    return vResult;
}

auto Schtasks::SetTaskRunOnlyIfNetworkAvailable(
    _In_    VARIANT_BOOL aRunOnlyIfNetworkAvailable
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskSet = CComPtr<ITaskSettings>();
        vResult = _TaskDef->get_Settings(&vTaskSet);
        if (FAILED(vResult))
        {
            break;
        }

        vResult = vTaskSet->put_RunOnlyIfNetworkAvailable(aRunOnlyIfNetworkAvailable);
        if (FAILED(vResult))
        {
            break;
        }
        break;
    }

    return vResult;
}

auto Schtasks::SetTaskRunOnlyIfIdle(
    _In_    VARIANT_BOOL aRunOnlyIfIdle
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskSet = CComPtr<ITaskSettings>();
        vResult = _TaskDef->get_Settings(&vTaskSet);
        if (FAILED(vResult))
        {
            break;
        }

        vResult = vTaskSet->put_RunOnlyIfIdle(aRunOnlyIfIdle);
        if (FAILED(vResult))
        {
            break;
        }
        break;
    }

    return vResult;
}

auto Schtasks::SetTaskWakeToRun(
    _In_    VARIANT_BOOL aWakeToRun
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskSet = CComPtr<ITaskSettings>();
        vResult = _TaskDef->get_Settings(&vTaskSet);
        if (FAILED(vResult))
        {
            break;
        }

        vResult = vTaskSet->put_WakeToRun(aWakeToRun);
        if (FAILED(vResult))
        {
            break;
        }
        break;
    }

    return vResult;
}

auto Schtasks::SetTaskRestartCount(
    _In_    int aRestartCount
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskSet = CComPtr<ITaskSettings>();
        vResult = _TaskDef->get_Settings(&vTaskSet);
        if (FAILED(vResult))
        {
            break;
        }

        vResult = vTaskSet->put_RestartCount(aRestartCount);
        if (FAILED(vResult))
        {
            break;
        }
        break;
    }

    return vResult;
}

auto Schtasks::SetTaskMultipleInstancePolicy(
    _In_    TASK_INSTANCES_POLICY aInstancePolicy
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskSet = CComPtr<ITaskSettings>();
        vResult = _TaskDef->get_Settings(&vTaskSet);
        if (FAILED(vResult))
        {
            break;
        }

        vResult = vTaskSet->put_MultipleInstances(aInstancePolicy);
        if (FAILED(vResult))
        {
            break;
        }
        break;
    }

    return vResult;
}

auto Schtasks::SetTaskCompatibility(
    _In_    TASK_COMPATIBILITY aCompatibility
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskSet = CComPtr<ITaskSettings>();
        vResult = _TaskDef->get_Settings(&vTaskSet);
        if (FAILED(vResult))
        {
            break;
        }

        vResult = vTaskSet->put_Compatibility(aCompatibility);
        if (FAILED(vResult))
        {
            break;
        }
        break;
    }

    return vResult;
}

auto Schtasks::SetTaskRestartInterval(
    _In_    BSTR aRestartInterval
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskSet = CComPtr<ITaskSettings>();
        vResult = _TaskDef->get_Settings(&vTaskSet);
        if (FAILED(vResult))
        {
            break;
        }

        vResult = vTaskSet->put_RestartInterval(aRestartInterval);
        if (FAILED(vResult))
        {
            break;
        }
        break;
    }

    return vResult;
}

auto Schtasks::SetTaskExecutionTimeLimit(
    _In_    BSTR aExecutionTimeLimit
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskSet = CComPtr<ITaskSettings>();
        vResult = _TaskDef->get_Settings(&vTaskSet);
        if (FAILED(vResult))
        {
            break;
        }

        vResult = vTaskSet->put_ExecutionTimeLimit(aExecutionTimeLimit);
        if (FAILED(vResult))
        {
            break;
        }
        break;
    }

    return vResult;
}

auto Schtasks::SetTaskDeleteExpiredTaskAfter(
    _In_    BSTR aDeleteExpiredTaskAfter
) -> HRESULT
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vTaskSet = CComPtr<ITaskSettings>();
        vResult = _TaskDef->get_Settings(&vTaskSet);
        if (FAILED(vResult))
        {
            break;
        }

        vResult = vTaskSet->put_DeleteExpiredTaskAfter(aDeleteExpiredTaskAfter);
        if (FAILED(vResult))
        {
            break;
        }
        break;
    }

    return vResult;
}

