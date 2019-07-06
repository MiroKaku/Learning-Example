#pragma once

// C++ Runtime
#include <mutex>

// Windows
#include <atlbase.h>
#include <comutil.h>
#include <taskschd.h>


template<typename T>
class Singleton
{
    static std::unique_ptr<T> _Instance;

protected:
    Singleton() = default;

public:
    static auto GetInstance() -> T &
    {
        static std::once_flag s_flag;

        std::call_once(s_flag, []() { Singleton::_Instance = std::make_unique<T>(); });

        return *_Instance;
    }
};

template<typename T>
__declspec(selectany) std::unique_ptr<T> Singleton<T>::_Instance = nullptr;


class Schtasks final
{
    CComPtr<ITaskService>       _TaskService    = nullptr;
    CComPtr<ITaskFolder>        _TaskFolder     = nullptr;
    CComPtr<IRegisteredTask>    _Task           = nullptr;
    CComPtr<ITaskDefinition>    _TaskDef        = nullptr;

    explicit Schtasks(_In_ CComPtr<ITaskService>& aTaskService);
    explicit Schtasks(_In_ CComPtr<ITaskService>& aTaskService, _In_ CComPtr<ITaskFolder>& aTaskFolder);

public:
    Schtasks();
    Schtasks(_In_ const Schtasks&  aOther) noexcept;
    Schtasks(_Inout_    Schtasks&& aOther) noexcept;

    auto operator=(_In_ const Schtasks&  aOther) noexcept -> Schtasks &;
    auto operator=(_Inout_    Schtasks&& aOther) noexcept -> Schtasks &;

    auto Swap(_Inout_ Schtasks& aOther) noexcept -> void;

    auto OpenTask(
        _In_        BSTR aTaskName
    )->HRESULT;
    
    auto CreateTask(
        _In_        BSTR aTaskName,
        _In_        BSTR aTaskRun,
        _In_opt_    BSTR aTaskRunArgs       = nullptr,
        _In_opt_    bool aUpdateIfExists    = false,
        _In_opt_    BSTR aUserId            = nullptr,
        _In_opt_    BSTR aPassword          = nullptr,
        _In_opt_    TASK_LOGON_TYPE aLogonType = TASK_LOGON_INTERACTIVE_TOKEN,
        _In_opt_    BSTR aSddl              = nullptr
    )->HRESULT;

    auto DeleteTask(
        _In_        BSTR aTaskName
    )->HRESULT;
    
    auto OpenFolder(
        _In_        BSTR aFolder
    )->HRESULT;

    auto CreateFolder(
        _In_        BSTR aFolder,
        _In_opt_    BSTR aSddl           = nullptr,
        _Out_opt_   Schtasks* aNewFolder = nullptr
    )->HRESULT;

    auto DeleteFolder(
        _In_        BSTR aFolder
    )->HRESULT;

    auto RunTask(
        _In_opt_    BSTR aArgs          = nullptr
    )->HRESULT;

    auto ApplyChange(
    )->HRESULT;

    auto SetTaskRegistrationInfo(
        _In_opt_    BSTR aAuthor        = nullptr,
        _In_opt_    BSTR aDescription   = nullptr,
        _In_opt_    BSTR aVersion       = nullptr,
        _In_opt_    BSTR aSddl          = nullptr
    )->HRESULT;

    auto GetTaskRegistrationInfo(
        _Out_opt_   BSTR* aAuthor       = nullptr,
        _Out_opt_   BSTR* aDescription  = nullptr,
        _Out_opt_   BSTR* aVersion      = nullptr,
        _Out_opt_   BSTR* aSddl         = nullptr
    )->HRESULT;

    auto SetTaskDisplayName(
        _In_        BSTR aTaskDisplayName= nullptr
    )->HRESULT;

    auto SetTaskRunlevel(
        _In_        TASK_RUNLEVEL_TYPE aRunlevel
    )->HRESULT;

    auto SetTaskLogon(
        _In_        TASK_LOGON_TYPE aLogonType = TASK_LOGON_NONE,
        _In_opt_    BSTR aUserId        = nullptr,
        _In_opt_    BSTR aGroupId       = nullptr
        )->HRESULT;

    auto GetTaskPrincipal(
        _Out_opt_   BSTR* aTaskName    = nullptr,
        _Out_opt_   TASK_RUNLEVEL_TYPE* aRunlevel  = nullptr,
        _Out_opt_   TASK_LOGON_TYPE*    aLogonType = nullptr,
        _Out_opt_   BSTR* aUserId      = nullptr,
        _Out_opt_   BSTR* aGroupId     = nullptr
    )->HRESULT;

    auto CreateTaskActionExec(
        _In_        BSTR aTaskRun,
        _In_opt_    BSTR aTaskRunArgs               = nullptr,
        _In_opt_    BSTR aTaskRunWorkingDirectory   = nullptr,
        _In_opt_    VARIANT_BOOL aHideWindow        = VARIANT_FALSE
    )->HRESULT;

    auto CreateTaskTriggerIdle(
        _In_opt_    BSTR aRepetitionInterval        = nullptr,
        _In_opt_    BSTR aRepetitionDuration        = nullptr,
        _In_opt_    BSTR aExecutionTimeLimit        = nullptr,
        _In_opt_    BSTR aStartBoundary             = nullptr,
        _In_opt_    BSTR aEndBoundary               = nullptr,
        _In_opt_    VARIANT_BOOL aStopAtDurationEnd = VARIANT_FALSE
    )->HRESULT;
    
    auto CreateTaskTriggerBoot(
        _In_opt_    BSTR aDelay                     = nullptr,
        _In_opt_    BSTR aRepetitionInterval        = nullptr,
        _In_opt_    BSTR aRepetitionDuration        = nullptr,
        _In_opt_    BSTR aExecutionTimeLimit        = nullptr,
        _In_opt_    BSTR aStartBoundary             = nullptr,
        _In_opt_    BSTR aEndBoundary               = nullptr,
        _In_opt_    VARIANT_BOOL aStopAtDurationEnd = VARIANT_FALSE
    )->HRESULT;

    auto CreateTaskTriggerLogon(
        _In_opt_    BSTR aUserId,
        _In_opt_    BSTR aDelay                     = nullptr,
        _In_opt_    BSTR aRepetitionInterval        = nullptr,
        _In_opt_    BSTR aRepetitionDuration        = nullptr,
        _In_opt_    BSTR aExecutionTimeLimit        = nullptr,
        _In_opt_    BSTR aStartBoundary             = nullptr,
        _In_opt_    BSTR aEndBoundary               = nullptr,
        _In_opt_    VARIANT_BOOL aStopAtDurationEnd = VARIANT_FALSE
    )->HRESULT;

    auto CreateTaskTriggerSessionStateChange(
        _In_opt_    BSTR aUserId                    = nullptr,
        _In_opt_    BSTR aDelay                     = nullptr,
        _In_opt_    TASK_SESSION_STATE_CHANGE_TYPE aType = TASK_CONSOLE_CONNECT,
        _In_opt_    BSTR aRepetitionInterval        = nullptr,
        _In_opt_    BSTR aRepetitionDuration        = nullptr,
        _In_opt_    BSTR aExecutionTimeLimit        = nullptr,
        _In_opt_    BSTR aStartBoundary             = nullptr,
        _In_opt_    BSTR aEndBoundary               = nullptr,
        _In_opt_    VARIANT_BOOL aStopAtDurationEnd = VARIANT_FALSE
    )->HRESULT;

    auto CreateTaskTriggerEvent(
        _In_opt_    BSTR aSubscription              = nullptr,
        _In_opt_    BSTR aDelay                     = nullptr,
        _In_opt_    BSTR aRepetitionInterval        = nullptr,
        _In_opt_    BSTR aRepetitionDuration        = nullptr,
        _In_opt_    BSTR aExecutionTimeLimit        = nullptr,
        _In_opt_    BSTR aStartBoundary             = nullptr,
        _In_opt_    BSTR aEndBoundary               = nullptr,
        _In_opt_    VARIANT_BOOL aStopAtDurationEnd = VARIANT_FALSE
    )->HRESULT;

    auto CreateTaskTriggerTime(
        _In_opt_    BSTR aRandomDelay               = nullptr,
        _In_opt_    BSTR aRepetitionInterval        = nullptr,
        _In_opt_    BSTR aRepetitionDuration        = nullptr,
        _In_opt_    BSTR aExecutionTimeLimit        = nullptr,
        _In_opt_    BSTR aStartBoundary             = nullptr,
        _In_opt_    BSTR aEndBoundary               = nullptr,
        _In_opt_    VARIANT_BOOL aStopAtDurationEnd = VARIANT_FALSE
    )->HRESULT;

    auto SetTaskEnable(
        _In_        VARIANT_BOOL aEnabled
    )->HRESULT;

    auto SetTaskHidden(
        _In_        VARIANT_BOOL aHidden
        )->HRESULT;

    auto SetTaskAllowDemandStart(
        _In_        VARIANT_BOOL aAllowDemandStart
    )->HRESULT;

    auto SetTaskStopIfGoingOnBatteries(
        _In_        VARIANT_BOOL aStopIfGoingOnBatteries
    )->HRESULT;

    auto SetTaskDisallowStartIfOnBatteries(
        _In_        VARIANT_BOOL aDisallowStartIfOnBatteries
    )->HRESULT;

    auto SetTaskAllowHardTerminate(
        _In_        VARIANT_BOOL aAllowHardTerminate
    )->HRESULT;

    auto SetTaskStartWhenAvailable(
        _In_        VARIANT_BOOL aStartWhenAvailable
    )->HRESULT;

    auto SetTaskRunOnlyIfNetworkAvailable(
        _In_        VARIANT_BOOL aRunOnlyIfNetworkAvailable
    )->HRESULT;

    auto SetTaskRunOnlyIfIdle(
        _In_        VARIANT_BOOL aRunOnlyIfIdle
    )->HRESULT;

    auto SetTaskWakeToRun(
        _In_        VARIANT_BOOL aWakeToRun
    )->HRESULT;

    auto SetTaskRestartCount(
        _In_        int aRestartCount
    )->HRESULT;

    auto SetTaskMultipleInstancePolicy(
        _In_        TASK_INSTANCES_POLICY aInstancePolicy
    )->HRESULT;

    auto SetTaskCompatibility(
        _In_        TASK_COMPATIBILITY aCompatibility
    )->HRESULT;

    auto SetTaskRestartInterval(
        _In_        BSTR aRestartInterval
    )->HRESULT;

    auto SetTaskExecutionTimeLimit(
        _In_        BSTR aExecutionTimeLimit
    )->HRESULT;

    auto SetTaskDeleteExpiredTaskAfter(
        _In_        BSTR aDeleteExpiredTaskAfter
    )->HRESULT;

};
