#include "stdafx.h"
#include "Schtasks.h"


int main()
{
    auto vResult = S_OK;

    for (;;)
    {
        auto vSchtasks = Schtasks();
        vResult = vSchtasks.CreateTask(
            bstr_t(R"(\TestTask)"),
            bstr_t("notepad.exe"));
        if (FAILED(vResult))
        {
            break;
        }

        auto vTime = __time64_t();
        _time64(&vTime);

        auto vTm = tm();
        _localtime64_s(&vTm, &vTime);

        char vTmc[128]{};
        sprintf_s(vTmc, "%04d-%02d-%02dT%02d:%02d:%02d",
            vTm.tm_year + 1900, vTm.tm_mon + 1, vTm.tm_mday,
            vTm.tm_hour, vTm.tm_min, vTm.tm_sec);

        vResult = vSchtasks.CreateTaskTriggerTime(
            nullptr,
            bstr_t("PT1M"),
            nullptr,
            nullptr,
            bstr_t(vTmc));
        if (FAILED(vResult))
        {
            break;
        }

        vSchtasks.SetTaskRunlevel(TASK_RUNLEVEL_HIGHEST);
        vSchtasks.SetTaskRunOnlyIfIdle(VARIANT_FALSE);
        vSchtasks.SetTaskStartWhenAvailable(VARIANT_TRUE);
        vSchtasks.SetTaskAllowHardTerminate(VARIANT_FALSE);
        vSchtasks.SetTaskStopIfGoingOnBatteries(VARIANT_FALSE);
        vSchtasks.SetTaskDisallowStartIfOnBatteries(VARIANT_FALSE);
        vSchtasks.SetTaskMultipleInstancePolicy(TASK_INSTANCES_IGNORE_NEW);
        vSchtasks.SetTaskRegistrationInfo(bstr_t("Test"), bstr_t("Test Schtasks"));
        vResult = vSchtasks.ApplyChange();
        
        vSchtasks.RunTask();
        break;
    }

    return vResult;
}
