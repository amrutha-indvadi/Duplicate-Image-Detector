#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_IMPLEMENTATION
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>
#include "stb_image.h"

#define RGBA_CHANNELS 4

typedef struct
{
    char FilePath[MAX_PATH];
    int Width;
    int Height;
    int OriginalChannels;
    unsigned char* Pixels;
    size_t DataSize;
} Image;

typedef enum
{
    IMAGE_DIFFERENT = 0,
    IMAGE_DUPLICATE = 1,
    IMAGE_ERROR = -1
} CompareResult;

static void FreeImageMemory(Image* image)
{
    if (image->Pixels != NULL)
    {
        stbi_image_free(image->Pixels);
        image->Pixels = NULL;
    }
    image->DataSize = 0;
}

static int LoadImages(Image* image)
{
    image->Pixels = stbi_load(image->FilePath, &image->Width, &image->Height, &image->OriginalChannels, RGBA_CHANNELS);

    if (image->Pixels == NULL)
    {
        printf("Error loading image: %s\n", image->FilePath);
        printf("Reason: %s\n", stbi_failure_reason());
        return 0;
    }

    if (image->Width <= 0 || image->Height <= 0 || (size_t)image->Width > SIZE_MAX / RGBA_CHANNELS / (size_t)image->Height)
    {
        printf("Error: Invalid image dimensions.\n");
        stbi_image_free(image->Pixels);
        image->Pixels = NULL;
        return 0;
    }

    image->DataSize = (size_t)image->Width * (size_t)image->Height * RGBA_CHANNELS;
    return 1;
}

static int CompareDimensions(const Image* img1, const Image* img2)
{
    if (img1->Width != img2->Width || img1->Height != img2->Height)
    {
        return 0;
    }
    return 1;
}

static int ComparePixels(const Image* img1, const Image* img2)
{
    if (img1->DataSize != img2->DataSize)
    {
        return 0;
    }

    if (memcmp(img1->Pixels, img2->Pixels, img1->DataSize) != 0)
    {
        return 0;
    }

    return 1;
}

static CompareResult CompareImages(const Image* img1, const Image* img2)
{
    if (!CompareDimensions(img1, img2))
        return IMAGE_DIFFERENT;

    if (!ComparePixels(img1, img2))
        return IMAGE_DIFFERENT;

    return IMAGE_DUPLICATE;
}

static int IsSupportedImage(const char* filename)
{
    const char* ext = strrchr(filename, '.');
    if (!ext) return 0;
    if (_stricmp(ext, ".jpg") == 0 ||
        _stricmp(ext, ".jpeg") == 0 ||
        _stricmp(ext, ".png") == 0 ||
        _stricmp(ext, ".bmp") == 0 ||
        _stricmp(ext, ".tga") == 0)
    {
        return 1;
    }
    return 0;
}

static void DisplayImage(const char* filePath)
{
    char choice[10] = { 0 };
    printf("Do you want to view this image? \n(y/n): ");
    if (fgets(choice, sizeof(choice), stdin) == NULL) return;
    choice[strcspn(choice, "\r\n")] = 0;

    if (_stricmp(choice, "y") == 0)
    {
        HINSTANCE result = ShellExecuteA(NULL, "open", filePath, NULL, NULL, SW_SHOWNORMAL);
        if ((intptr_t)result <= 32)
        {
            printf("Error: Could not open the image viewer for %s\n", filePath);
        }
        else
        {
            printf("Opening image in system viewer...\n");
        }
    }
}

static int DeleteDuplicate(Image* image2)
{
    char choice[10] = { 0 };
    printf("Do you want to delete the duplicate image? \n(y/n): ");
    if (fgets(choice, sizeof(choice), stdin) == NULL) return 0;
    choice[strcspn(choice, "\r\n")] = 0;

    if (_stricmp(choice, "y") == 0)
    {
        FreeImageMemory(image2);

        if (remove(image2->FilePath) == 0)
        {
            printf("Successfully deleted: %s\n", image2->FilePath);
            return 1;
        }
        else
        {
            printf("Error: Could not delete the file (it may be in use or lack permissions).\n");
        }
    }
    return 0;
}

// Separate function to scan folder and collect image paths
static int LoadFolderImages(const char* folderPath, char (**outPaths)[MAX_PATH], int** outIsDeleted)
{
    char searchPath[MAX_PATH] = { 0 };
    snprintf(searchPath, sizeof(searchPath), "%s\\*.*", folderPath);

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath, &findData);

    if (hFind == INVALID_HANDLE_VALUE)
    {
        printf("Error: Could not open directory or directory is empty. Check your path.\n");
        return -1;
    }

    int capacity = 10;
    int count = 0;

    char (*filePaths)[MAX_PATH] = (char (*)[MAX_PATH])malloc(capacity * sizeof(*filePaths));
    int* isDeleted = (int*)calloc(capacity, sizeof(int));

    if (!filePaths || !isDeleted)
    {
        printf("Memory allocation failed.\n");
        FindClose(hFind);
        free(filePaths);
        free(isDeleted);
        return -1;
    }

    do
    {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            if (IsSupportedImage(findData.cFileName))
            {
                if (count >= capacity)
                {
                    capacity *= 2;
                    char (*tempPaths)[MAX_PATH] = (char (*)[MAX_PATH])realloc(filePaths, capacity * sizeof(*tempPaths));
                    int* tempDeleted = (int*)realloc(isDeleted, capacity * sizeof(int));

                    if (!tempPaths || !tempDeleted)
                    {
                        printf("Memory reallocation failed.\n");
                        break;
                    }
                    filePaths = tempPaths;
                    isDeleted = tempDeleted;
                    memset(isDeleted + count, 0, (capacity - count) * sizeof(int));
                }
                snprintf(filePaths[count], MAX_PATH, "%s\\%s", folderPath, findData.cFileName);
                count++;
            }
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);

    *outPaths = filePaths;
    *outIsDeleted = isDeleted;
    return count;
}

int main()
{
    char folderPath[MAX_PATH] = { 0 };

    printf("--------------------------------------------------------------\n");
    printf("FOLDER IMAGE DEDUPLICATION ENGINE\n");
    printf("--------------------------------------------------------------\n");

    printf("Enter folder path to scan: ");

    if (fgets(folderPath, sizeof(folderPath), stdin) != NULL)
    {
        folderPath[strcspn(folderPath, "\r\n")] = 0;
    }
    else
    {
        printf("Error reading folder path.\n");
        return 1;
    }

    size_t len = strlen(folderPath);
    if (len > 0 && folderPath[0] == '"' && folderPath[len - 1] == '"')
    {
        folderPath[len - 1] = '\0';
        memmove(folderPath, folderPath + 1, len - 1);
    }

    char (*filePaths)[MAX_PATH] = NULL;
    int* isDeleted = NULL;

    int count = LoadFolderImages(folderPath, &filePaths, &isDeleted);
    if (count < 0)
    {
        return 1;
    }

    printf("--------------------------------------------------------------\n");
    printf("Total number of supported images found: %d\n", count);
    printf("--------------------------------------------------------------\n");

    if (count < 2)
    {
        printf("Need at least 2 images in the folder to compare.\n");
        free(filePaths);
        free(isDeleted);
        return 0;
    }

    // Compare all pairs
    for (int i = 0; i < count; i++)
    {
        if (isDeleted[i]) continue;

        Image img1 = { 0 };
        strcpy(img1.FilePath, filePaths[i]);

        if (!LoadImages(&img1))
            continue;

        for (int j = i + 1; j < count; j++)
        {
            if (isDeleted[j]) continue;

            Image img2 = { 0 };
            strcpy(img2.FilePath, filePaths[j]);

            if (!LoadImages(&img2))
                continue;

            CompareResult result = CompareImages(&img1, &img2);

            if (result == IMAGE_DUPLICATE)
            {
                printf("\n[DUPLICATE MATCH FOUND]\n");
                printf("Original:  %s\n", img1.FilePath);
                printf("Duplicate: %s\n", img2.FilePath);
                printf("--------------------------------------------------------------\n");

                DisplayImage(img2.FilePath);

                if (DeleteDuplicate(&img2))
                {
                    isDeleted[j] = 1;
                }
                printf("--------------------------------------------------------------\n");
            }

            FreeImageMemory(&img2);
        }

        FreeImageMemory(&img1);
    }

    printf("\nScan and deduplication process completed.\n");

    free(filePaths);
    free(isDeleted);
    return 0;
}