#pragma once
#include <devioctl.h>


#define IOCPCTL_NAME            LR"(iocpctl)"
#define IOCPCTL_DEVICE_NAME     LR"(\Device\)"              IOCPCTL_NAME
#define IOCPCTL_DOSDEVICE_NAME  LR"(\DosDevices\Global\)"   IOCPCTL_NAME
#define IOCPCTL_DRIVER_NAME     LR"(\Driver\)"              IOCPCTL_NAME
#define IOCPCTL_DOS_NAME        LR"(\\.\)"                  IOCPCTL_NAME


enum : ULONG
{
    IOCTL_BEGIN         = 0x800,

    IOCTL_IO_BUFFERED   = CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_BEGIN + 0x00, METHOD_BUFFERED  , FILE_ANY_ACCESS),
};
