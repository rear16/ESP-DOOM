#include "WadFile.h"

#include <cstring>

bool WadFile::begin(fs::FS* fs)
{
    filesystem=fs;
    return true;
}

bool WadFile::open(const char* path)
{
    file=filesystem->open(path);

    if(!file)
        return false;

    file.read(
        (uint8_t*)&header,
        sizeof(header));

    printInfo();

    return true;
}

void WadFile::printInfo()
{
    Serial.println();

    Serial.println("===== WAD =====");

    Serial.print("TYPE : ");
    Serial.write(header.identification,4);
    Serial.println();

    Serial.print("LUMPS : ");
    Serial.println(header.numLumps);

    Serial.print("DIRECTORY : ");
    Serial.println(header.directoryOffset);

    Serial.println("================");
}

bool WadFile::findLump(
    const char* name,
    WadDirectory& lump)
{
    file.seek(header.directoryOffset);

    WadDirectory entry;

    for(int i=0;i<header.numLumps;i++)
    {
        file.read(
            (uint8_t*)&entry,
            sizeof(entry));

        char lumpName[9];

        memcpy(
            lumpName,
            entry.name,
            8);

        lumpName[8]=0;

        if(strcmp(
            lumpName,
            name)==0)
        {
            lump=entry;
            return true;
        }
    }

    return false;
}

bool WadFile::read(
    const WadDirectory& lump,
    void* buffer)
{
    file.seek(lump.offset);

    return file.read(
        (uint8_t*)buffer,
        lump.size)==lump.size;
}