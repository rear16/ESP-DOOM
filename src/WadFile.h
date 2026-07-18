#pragma once

#include <Arduino.h>
#include <FS.h>

struct WadHeader
{
    char identification[4];
    int32_t numLumps;
    int32_t directoryOffset;
};

struct WadDirectory
{
    int32_t offset;
    int32_t size;
    char name[8];
};

class WadFile
{
public:

    bool begin(fs::FS* fs);

    bool open(const char* path);

    void printInfo();

    bool findLump(
        const char* name,
        WadDirectory& lump);

    bool read(
        const WadDirectory& lump,
        void* buffer);

private:

    fs::FS* filesystem=nullptr;

    File file;

    WadHeader header;
};