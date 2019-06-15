#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <Windows.h>
#include <process.h>

#include <iocpctl\iocpctl.h>


#define IO_MAXIMUN_NUMBER   20

typedef struct IOCP_CONTEXT
{
    OVERLAPPED  Overlapped;
    ULONG       MessageId;

}*PIOCP_CONTEXT;


auto __stdcall IOThread(void* aContext) -> unsigned
{
    auto vResult            = NOERROR;
    auto vCompletionPort    = static_cast<HANDLE>(aContext);
    auto vCount             = 0ul;
    auto vMaxCount          = ULONG_PTR();

    for (;;)
    {
        auto vOverlapped    = LPOVERLAPPED();
        auto vNumberBytes   = 0ul;

        if (!GetQueuedCompletionStatus(
            vCompletionPort,
            &vNumberBytes,
            &vMaxCount,
            &vOverlapped,
            INFINITE))
        {
            vResult = GetLastError();
            fprintf(stderr, "GetQueuedCompletionStatus on the IoPort failed, error %u\n", vResult);
            break;
        }
        auto vContext = CONTAINING_RECORD(vOverlapped, IOCP_CONTEXT, Overlapped);

        fprintf(stdout, "<<< RecvId: %u\n", vContext->MessageId);
        VirtualFree(vContext, 0, MEM_RELEASE);

        if (++vCount >= vMaxCount)
        {
            break;
        }
    }

    fprintf(stdout, "IOThread exit: %u\n", vResult);

    return _endthreadex(vResult), vResult;
}

int main()
{
    auto vResult            = NOERROR;
    auto vDevice            = HANDLE(nullptr);
    auto vCompletionPort    = HANDLE(nullptr);
    auto vThread            = HANDLE(nullptr);

    for (;;)
    {
        vDevice = CreateFileW(
            IOCPCTL_DOS_NAME,
            FILE_ANY_ACCESS,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_DEVICE | FILE_FLAG_OVERLAPPED,
            nullptr);
        if (vDevice == INVALID_HANDLE_VALUE)
        {
            vDevice = nullptr;
            vResult = GetLastError();
            fprintf(stderr, "CreateFile on the [%ls] failed, error %u\n", IOCPCTL_DOS_NAME, vResult);
            break;
        }

        vCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
        if (vCompletionPort == nullptr)
        {
            vResult = GetLastError();
            break;
        }

        if (CreateIoCompletionPort(vDevice, vCompletionPort, IO_MAXIMUN_NUMBER, 0) == nullptr)
        {
            vResult = GetLastError();
            break;
        }

        vThread = (HANDLE)_beginthreadex(nullptr, 0, IOThread, vCompletionPort, 0, nullptr);
        if (vThread == nullptr || vThread == INVALID_HANDLE_VALUE)
        {
            vResult = _doserrno;
            fprintf(stderr, "_beginthreadex failed, error %u\n", vResult);
            break;
        }

        for (auto i = 0ul; i < IO_MAXIMUN_NUMBER; i++)
        {
            auto vContext = (PIOCP_CONTEXT)VirtualAlloc(
                nullptr, sizeof(IOCP_CONTEXT), MEM_COMMIT, PAGE_READWRITE);
            if (vContext == nullptr)
            {
                vResult = GetLastError();
                fprintf(stderr, "VirtualAlloc failed, error %u\n", vResult);
                break;
            }

            vContext->MessageId     = i;
            vContext->Overlapped    = OVERLAPPED();

            fprintf(stdout, ">>> SendId: %u\n", vContext->MessageId);

            if (DeviceIoControl(
                vDevice,
                IOCTL_IO_BUFFERED,
                &vContext->MessageId, sizeof(vContext->MessageId),
                &vContext->MessageId, sizeof(vContext->MessageId),
                nullptr,
                &vContext->Overlapped))
            {
                fprintf(stdout, "<<< RecvId: %u\n", vContext->MessageId);
                VirtualFree(vContext, 0, MEM_RELEASE);
            }
        }

        break;
    }

    if (vThread)
    {
        WaitForSingleObject(vThread, INFINITE);
        CloseHandle(vThread);
    }
    if (vCompletionPort)
    {
        CloseHandle(vCompletionPort);
    }
    if (vDevice)
    {
        CloseHandle(vDevice);
    }

    return vResult;
}
