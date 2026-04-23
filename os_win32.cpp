#include "includes.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

size_t TOTAL_ALLOCATED_BYTES  = 0;
void * PlatformAlloc(size_t size)
{
    TOTAL_ALLOCATED_BYTES += size;
    return VirtualAlloc(0, size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
}

void PlatformFree(void * memory)
{
    if (memory)
    {
        VirtualFree(memory, 0, MEM_RELEASE);
    }
}

PLATFORM_FREE_FILE(DEBUG_PlatformFreeFileMemory)
{
    if (*memory)
    {
        VirtualFree(*memory, 0, MEM_RELEASE);
        *memory = NULL;
    }
}

PLATFORM_LOAD_FILE(DEBUG_PlatformReadEntireFile)
{
    DEBUG_FileResult result = {NULL, 0};
    HANDLE fileHandle = CreateFileA(filepath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);

    if (fileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER fileSize;
        if (GetFileSizeEx(fileHandle, &fileSize))
        {
            result.data = PlatformAlloc(fileSize.QuadPart);
            result.size = fileSize.QuadPart;
            if (result.data)
            {
                if (fileSize.QuadPart <= UINT_MAX) { /* TODO: assert here. */ }
                DWORD bytesRead = 0;
                if (ReadFile(fileHandle, result.data, fileSize.QuadPart, &bytesRead, 0) && (fileSize.QuadPart == bytesRead))
                {
                    // NOTE: File read successfully.
                }
                else
                {
                    DEBUG_PlatformFreeFileMemory(&result.data);
                }
            }
        }
    }

    CloseHandle(fileHandle);
    return result;
}

ImageData LoadImageFromFile(const char * filename)
{
    ImageData data = {};
    DEBUG_FileResult fileData = DEBUG_PlatformReadEntireFile(filename);
    if (fileData.data)
    {
        stbi_uc * pBitmap = stbi_load_from_memory((stbi_uc*)fileData.data, fileData.size, &data.width, &data.height, &data.numComponents, 4);
        if (pBitmap)
        {
            // NOTE(rordon): numComponts is forced to 4 in stb.
            data.numComponents = 4;
            data.size = data.width * data.height * data.numComponents;
            data.memory = pBitmap;
        }
        DEBUG_PlatformFreeFileMemory(&fileData.data);
    } 

    return data;
}


void FreeImage(ImageData * image)
{
    stbi_image_free(image->memory);
}
