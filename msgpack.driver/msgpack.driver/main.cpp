
#define MSGPACK_NO_BOOST            1
#define MSGPACK_ENDIAN_LITTLE_BYTE  1

#include <ucxxrt.h>
#include <msgpack.hpp>

#define LOG(fmt, ...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[msgpack] " fmt "\n", __VA_ARGS__)

EXTERN_C DRIVER_INITIALIZE DriverMain;

EXTERN_C void _wassert(
    wchar_t const* message,
    wchar_t const* filename,
    unsigned line
)
{

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
        "================= assert ================\n"
        "\n"
        "%ls"
        "\n"
        "File  : %ls\n"
        "Line  : %d\n"
        "\n",
        message,
        filename,
        line);

    __debugbreak();
}

enum my_enum {
    elem1,
    elem2,
    elem3
};

MSGPACK_ADD_ENUM(my_enum);

class old_class
{
public:
    old_class()
        : value("default")
    {
        return;
    }

    std::string value;

    MSGPACK_DEFINE(value);
};

class new_class
{
public:
    new_class()
        : value("default")
        , flag(-1)
    {
        return;
    }

    std::string value;
    int flag;

    MSGPACK_DEFINE(value, flag);
};

struct legacy_struct
{
    char name[255];
    union MyUnion
    {
        int     a;
        double  b;

        MSGPACK_DEFINE(a, b);
    }x;

    MSGPACK_DEFINE(name, x);
};

class mixed_class
{
public:
    mixed_class()
        : value(__FUNCSIG__)
    {
    }

    mixed_class(legacy_struct* x)
        : mixed_class()
    {
        memcpy(&legacy, x, sizeof legacy_struct);
    }

    std::string     value;
    legacy_struct   legacy;

    MSGPACK_DEFINE(value, legacy);
};

namespace Main
{
    EXTERN_C NTSTATUS DriverMain(PDRIVER_OBJECT DriverObject, PUNICODE_STRING)
    {
        NTSTATUS Status = STATUS_UNSUCCESSFUL;

        do
        {
            DriverObject->DriverUnload = [](PDRIVER_OBJECT)
            {
                return;
            };

            // TEST_BIN
            {
                msgpack::sbuffer sbuf;
                msgpack::pack(sbuf, msgpack::type::raw_ref((char*)DriverObject, sizeof DRIVER_OBJECT));

                msgpack::object_handle oh;
                msgpack::unpack(oh, sbuf.data(), sbuf.size());

                msgpack::type::raw_ref ref_bin;
                oh.get().convert(ref_bin);

                auto x = (PDRIVER_OBJECT)ref_bin.ptr;
                assert(memcmp(x, DriverObject, sizeof DRIVER_OBJECT) == 0);
            }

            // TEST_ENUM
            {
                msgpack::sbuffer sbuf;
                msgpack::pack(sbuf, elem1);
                msgpack::pack(sbuf, elem2);
                my_enum e3 = elem3;
                msgpack::pack(sbuf, e3);

                msgpack::object_handle oh;
                std::size_t off = 0;

                msgpack::unpack(oh, sbuf.data(), sbuf.size(), off);
                assert(oh.get().as<my_enum>() == elem1);

                msgpack::unpack(oh, sbuf.data(), sbuf.size(), off);
                assert(oh.get().as<my_enum>() == elem2);

                msgpack::unpack(oh, sbuf.data(), sbuf.size(), off);
                assert(oh.get().as<my_enum>() == elem3);
            }

            // TEST_CLASS
            {
                old_class oc;
                new_class nc;

                msgpack::sbuffer sbuf;
                msgpack::pack(sbuf, oc);

                msgpack::object_handle oh =
                    msgpack::unpack(sbuf.data(), sbuf.size());
                msgpack::object obj = oh.get();

                obj.convert(nc);

                LOG("value=%hs, flag=%d", nc.value.c_str(), nc.flag);
            }
            {
                new_class nc;
                old_class oc;

                msgpack::sbuffer sbuf;
                msgpack::pack(sbuf, nc);

                msgpack::object_handle oh =
                    msgpack::unpack(sbuf.data(), sbuf.size());
                msgpack::object obj = oh.get();

                obj.convert(oc);

                LOG("value=%hs", oc.value.c_str());
            }

            // TEST_MIXED
            {
                legacy_struct legacy{ "Hello", 123};
                mixed_class mixed(&legacy);

                msgpack::sbuffer sbuf;
                msgpack::pack(sbuf, mixed);

                msgpack::object_handle oh =
                    msgpack::unpack(sbuf.data(), sbuf.size());
                msgpack::object obj = oh.get();

                mixed_class unpacked;
                obj.convert(unpacked);

                LOG("value=%hs, name=%hs, b=%d",
                    unpacked.value.c_str(), unpacked.legacy.name, unpacked.legacy.x.a);
            }

        } while (false);

        return Status;
    }
}
