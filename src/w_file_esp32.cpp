#include "w_file_esp32.h"

#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>

extern "C"
{
#include "z_zone.h"
}

extern fs::FS* DG_FS;

typedef struct
{
    wad_file_t wad;
    File file;
} esp32_wad_file_t;

extern "C"
{

int DG_FileExists(const char *path)
{
    return DG_FS->exists(path);
}

void *DG_FileOpen(const char *path)
{
    File *f = new File(DG_FS->open(path));

    if (!(*f))
    {
        delete f;
        return NULL;
    }

    return f;
}

void DG_FileClose(void *handle)
{
    File *f = (File *)handle;

    f->close();

    delete f;
}

size_t DG_FileRead(void *handle, void *buffer, size_t len)
{
    return ((File *)handle)->read((uint8_t *)buffer, len);
}

int DG_FileSeek(void *handle, unsigned int offset)
{
    return ((File *)handle)->seek(offset);
}

unsigned int DG_FileSize(void *handle)
{
    return ((File *)handle)->size();
}

}

static wad_file_t* W_ESP32_OpenFile(char* path)
{
    if (DG_FS == nullptr)
    {
        Serial.println("DG_FS == NULL");
        return nullptr;
    }

    File file = DG_FS->open(path);
    if (!file)
    {
        Serial.println("OPEN FAILED");
        return nullptr;
    }
    
    esp32_wad_file_t *result = new esp32_wad_file_t();

    if (!result)
    {
        Serial.println("malloc failed");
        return nullptr;
    }

    result->wad.file_class = &esp32_wad_file;
    result->wad.mapped = NULL;
    result->wad.length = file.size();
    result->file = file;

    Serial.printf("OK (%u bytes)\n", result->wad.length);

    return &result->wad;
}

static void W_ESP32_CloseFile(wad_file_t* wad)
{
    esp32_wad_file_t *f = (esp32_wad_file_t *)wad;

    f->file.close();

    delete f;
}

static size_t W_ESP32_Read(
    wad_file_t* wad,
    unsigned int offset,
    void* buffer,
    size_t len)
{
    esp32_wad_file_t* f = (esp32_wad_file_t*)wad;

    f->file.seek(offset);

    return f->file.read((uint8_t*)buffer, len);
}

extern "C"
{

wad_file_class_t esp32_wad_file =
{
    W_ESP32_OpenFile,
    W_ESP32_CloseFile,
    W_ESP32_Read
};

}