#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BLOCK_SIZE 1024 // Byte
#define MAX_FILENAME_LENGTH 128
#define MAX_DIRECTORY_ENTRIES 100
#define FAT_FREE 0x00000000
#define FAT_END 0xFFFFFFFF

// Ŀ¼��
typedef struct {
    char name[MAX_FILENAME_LENGTH];
    unsigned int startBlock;
    unsigned int size;
    int isFile;
    unsigned int parentIndex;
    time_t createTime;
    time_t lastAccessTime;
} DirectoryEntry;

// Ŀ¼���
typedef struct {
    DirectoryEntry* entries;
    unsigned int count;
} DirectoryEntryTable;

// ����
typedef struct {
    unsigned int totalBlocks;
    unsigned int freeBlocks;
    unsigned int* FAT;
    unsigned char* diskData;
    DirectoryEntryTable directoryEntryTable;
} DiskInfo;

// ȫ�ֱ������̺͵�ǰĿ¼
DiskInfo diskInfo;
unsigned int currentDirectoryIndex; // ��ǰĿ¼��Ӧ��Ŀ¼������

// ��������
unsigned int allocateBlock();
unsigned int* allocateBlocks(unsigned int numBlocks);
void freeBlock(unsigned int block);
int freeBlocks(unsigned int startBlock);
void initializeFAT();
void initializeDirectoryEntryTable(DirectoryEntryTable* directoryEntryTable);
void freeDirectoryEntryTable(DirectoryEntryTable* directoryEntryTable);
void addDirectoryEntry(DirectoryEntryTable* directoryEntryTable, const char* name, unsigned int startBlock, unsigned int size, int isFile, unsigned int prev);
int getDirectoryEntryIndex(DirectoryEntryTable* directoryEntryTable, const char* name);
int createFile(const char* name, unsigned int fileSize);
int deleteFile(const char* name);
int createDirectory(const char* name);
int deleteDirectory(const char* name);
int setCurrentDirectoryIndex(const char* dirname);
int setCurrentDirectoryIndex(const char* dirname);
void listCurrentDirectoryEntry(char* infoBuf);
int renameFile(const char* filename, const char* newname);
int moveFile(const char* filename, const char* dirname);
int saveFileSystemInfo(const char* filename);
int loadFileSystemInfo(const char* filename);
void displayFileSystemInfo(char* infoBuf);
void initializeDisk(int numBlocks);
void freeDisk();

// ����һ�����п�
unsigned int allocateBlock()
{
    unsigned int block = 0;
    // �� FAT ���в��ҿ��п�
    unsigned int i;
    for (i = 1; i < diskInfo.totalBlocks; i++) {
        if (diskInfo.FAT[i] == FAT_FREE) {
            diskInfo.FAT[i] = FAT_END; // ��ǿ�Ϊ�ѷ���
            diskInfo.freeBlocks--;
            block = i;
            break;
        }
    }
    return block;
}

// ����һϵ�п飬���ط���ɹ��Ŀ���
unsigned int* allocateBlocks(unsigned int numBlocks)
{
    if (numBlocks > diskInfo.freeBlocks) {
        // printf("Not enough free blocks to allocate.\n");
        return NULL;
    }

    unsigned int* blocks = malloc(numBlocks * sizeof(unsigned int));
    unsigned int allocatedBlocks = 0;
    unsigned int blockIndex = 1; // �����ļ��еļ�����

    // ���� FAT ���ҵ������Ŀ��п�
    while (allocatedBlocks < numBlocks) {
        if (diskInfo.FAT[blockIndex] == FAT_FREE) {
            blocks[allocatedBlocks] = blockIndex;
            allocatedBlocks++;

            // ���� FAT ���е����ӹ�ϵ
            if (allocatedBlocks < numBlocks) {
                diskInfo.FAT[blockIndex] = blockIndex + 1;
            }
            else {
                diskInfo.FAT[blockIndex] = FAT_END;
            }
        }
        blockIndex++;
    }

    diskInfo.freeBlocks -= numBlocks;
    return blocks;
}

// �ͷ�ĳ���������ռ�
void freeBlock(unsigned int block)
{
    if (block >= 1 && block < diskInfo.totalBlocks) {
        diskInfo.FAT[block] = FAT_FREE; // ��ǿ�Ϊ����
        diskInfo.freeBlocks++;
    }
}

// �ͷ��ļ���ռ�õ����п飬�����ͷſ����
int freeBlocks(unsigned int startBlock)
{
    int freedBlocksCount = 0;
    if (startBlock >= 1 && startBlock < diskInfo.totalBlocks) {
        unsigned int currentBlock = startBlock;
        unsigned int nextBlock;

        while (currentBlock != FAT_END) {
            nextBlock = diskInfo.FAT[currentBlock];

            // ����ǰ����Ϊ����
            diskInfo.FAT[currentBlock] = FAT_FREE;
            diskInfo.freeBlocks++;

            currentBlock = nextBlock;
            freedBlocksCount++;
        }
        return freedBlocksCount;
    }
    return -1; // ����������Ƿ���ʼ��
}

// ��ʼ�� FAT ��
void initializeFAT()
{
    unsigned int i;
    for (i = 0; i < diskInfo.totalBlocks; i++) {
        diskInfo.FAT[i] = FAT_FREE;
    }
}

// ��ʼ��Ŀ¼���
void initializeDirectoryEntryTable(DirectoryEntryTable* directoryEntryTable)
{
    directoryEntryTable->entries = NULL;
    directoryEntryTable->count = 0;
    // ��ʼ����Ŀ¼
    createDirectory("/");
    currentDirectoryIndex = directoryEntryTable->count - 1;
}

// �ͷ�Ŀ¼�����ڴ�
void freeDirectoryEntryTable(DirectoryEntryTable* directoryEntryTable)
{
    if (directoryEntryTable->entries != NULL) {
        free(directoryEntryTable->entries);
    }
    directoryEntryTable->count = 0;
}

// ���Ŀ¼�Ŀ¼���
void addDirectoryEntry(DirectoryEntryTable* directoryEntryTable, const char* name, unsigned int startBlock, unsigned int size, int isFile, unsigned int prev)
{
    directoryEntryTable->count++;
    DirectoryEntry* newEntries = (DirectoryEntry*)malloc(sizeof(DirectoryEntry) * directoryEntryTable->count);

    // ��ʼ���·�����ڴ�
    memset(newEntries, 0, sizeof(DirectoryEntry) * directoryEntryTable->count);

    // ��ԭ�����ݿ������·�����ڴ���
    if (directoryEntryTable->count > 1)
    {
        memcpy(newEntries, directoryEntryTable->entries, sizeof(DirectoryEntry) * (directoryEntryTable->count - 1));
        free(directoryEntryTable->entries);
    }

    directoryEntryTable->entries = newEntries;

    // ����Ŀ¼��
    DirectoryEntry* entry = &directoryEntryTable->entries[directoryEntryTable->count - 1];
    strncpy(entry->name, name, MAX_FILENAME_LENGTH);
    entry->startBlock = startBlock;
    entry->size = size;
    entry->isFile = isFile;
    entry->parentIndex = prev;
    time(&(entry->lastAccessTime));
    time(&(entry->createTime));
}

// ��ȡĿ¼����Ŀ¼����е������������ڷ��� -1
int getDirectoryEntryIndex(DirectoryEntryTable* directoryEntryTable, const char* name)
{
    unsigned int i;
    for (i = 0; i < directoryEntryTable->count; i++) {
        if (strcmp(directoryEntryTable->entries[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

// �����ļ�������һϵ�п��ά��Ŀ¼����ɹ����� 0 ���� -1 �޿��ÿռ� -2
int createFile(const char* name, unsigned int fileSize)
{
    // �����ļ�����
    int index = getDirectoryEntryIndex(&diskInfo.directoryEntryTable, name);
    if (index != -1) {
        DirectoryEntry* entry = &diskInfo.directoryEntryTable.entries[index];
        if (entry->parentIndex == currentDirectoryIndex) {
            // printf("File already exsists.\n");
            return -1;
        }
    }
    unsigned int curIndex = diskInfo.directoryEntryTable.count - 1; // ��ȡ���µ�Ŀ¼������
    unsigned int numBlocks = (fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE; // �����������
    unsigned int* blocks = allocateBlocks(numBlocks); // �����
    if (blocks != NULL) {
        unsigned int startBlock = blocks[0]; // ��ȡ��ʼ��
        addDirectoryEntry(&diskInfo.directoryEntryTable, name, startBlock, fileSize, 1, currentDirectoryIndex);
        return 0;
    }
    return -2;
}

// ɾ���ļ����ɹ����� 0 �ļ������� -1
int deleteFile(const char* name)
{
    // �������޸��ļ�
    int index = getDirectoryEntryIndex(&diskInfo.directoryEntryTable, name);
    if (index != -1) {
        unsigned int startBlock = diskInfo.directoryEntryTable.entries[index].startBlock;
        unsigned int size = diskInfo.directoryEntryTable.entries[index].size;
        // �ͷ��ļ�ռ�õ�����ռ�
        freeBlocks(startBlock);
        // ��Ŀ¼�����ɾ��Ŀ¼��
        unsigned int i;
        for (i = index; i < diskInfo.directoryEntryTable.count - 1; i++) {
            diskInfo.directoryEntryTable.entries[i] = diskInfo.directoryEntryTable.entries[i + 1];
        }
        diskInfo.directoryEntryTable.count--;
        return 0;
    }
    return -1;
}

// �����ļ��У��ɹ����� 0 ���� -1
int createDirectory(const char* name)
{
    // �����ļ��д���
    int index = getDirectoryEntryIndex(&diskInfo.directoryEntryTable, name);
    if (index != -1) {
        DirectoryEntry* entry = &diskInfo.directoryEntryTable.entries[index];
        if (entry->parentIndex == currentDirectoryIndex) {
            // printf("File already exsists.\n");
            return -1;
        }
    }
    addDirectoryEntry(&diskInfo.directoryEntryTable, name, 0, 0, 0, currentDirectoryIndex);
    return 0;
}

// ɾ���ļ��У��ɹ����� 0 ������ -1
int deleteDirectory(const char* name)
{
    int index = getDirectoryEntryIndex(&diskInfo.directoryEntryTable, name);
    if (index != -1) {
        // ��Ŀ¼�����ɾ��Ŀ¼��
        unsigned int i;
        for (i = index; i < diskInfo.directoryEntryTable.count - 1; i++) {
            diskInfo.directoryEntryTable.entries[i] = diskInfo.directoryEntryTable.entries[i + 1];
        }
        diskInfo.directoryEntryTable.count--;
        return 0;
    }
    return -1;
}

// ���õ�ǰ�ļ��У��ɹ����� 0 �ļ� -1
int setCurrentDirectoryIndex(const char* dirname)
{
    unsigned int index = getDirectoryEntryIndex(&diskInfo.directoryEntryTable, dirname);
    if (diskInfo.directoryEntryTable.entries[index].isFile) {
        // printf("Unable to set current directory to a file!\n");
        return -1;
    }
    else {
        currentDirectoryIndex = index;
        return 0;
    }
}

// �г���ǰ�ļ����µ������ļ����ļ��е�����
void listCurrentDirectoryEntry(char* infoBuf)
{
    unsigned int i;
    int j = 0;
    for (i = 0; i < diskInfo.directoryEntryTable.count; i++) {
        DirectoryEntry* entry = &diskInfo.directoryEntryTable.entries[i];
        if (entry->parentIndex == currentDirectoryIndex) {
            char cTimeBuff[80];
            char aTimeBuff[80];
            strftime(cTimeBuff, 80, "%Y-%m-%d %H:%M:%S", localtime(&(entry->createTime)));
            strftime(aTimeBuff, 80, "%Y-%m-%d %H:%M:%S", localtime(&(entry->lastAccessTime)));
            // printf("[%s]\tStart Block: %u\tSize: %u Bytes\tName: %s\tC: %ld\tA: %ld\n", entry->isFile ? "File" : "Folder", entry->startBlock, entry->size, entry->name, entry->createTime, entry->lastAccessTime);
            j += sprintf(infoBuf + j, "[%-6s] Size: %-8u Bytes Name: %-10s CreateTime: %s AccessTime: %s\n", entry->isFile ? "File" : "Folder", entry->size, entry->name, cTimeBuff, aTimeBuff);
        }
    }
    // printf("ls result:\n%s", infoBuf);
}

// �������ļ����ɹ����� 0 ������ -1
int renameFile(const char* filename, const char* newname)
{
    // �����ļ��Ƿ����
    int index = getDirectoryEntryIndex(&diskInfo.directoryEntryTable, filename);
    if (index != -1) {
        DirectoryEntry* entry = &diskInfo.directoryEntryTable.entries[index];
        strcpy(entry->name, newname);
        time(&(entry->lastAccessTime));
        return 0;
    }
    return -1;
}

// �ƶ��ļ����ļ��У��ɹ����� 0 �ļ������� -1 �ļ��в����� -2
int moveFile(const char* filename, const char* dirname)
{
    // �����ļ����ļ���
    int fileIndex = getDirectoryEntryIndex(&diskInfo.directoryEntryTable, filename);
    int dirIndex = getDirectoryEntryIndex(&diskInfo.directoryEntryTable, filename);
    if (fileIndex == -1) {
        return -1;
    }
    else if (dirIndex == -1) {
        return -2;
    }
    else {
        DirectoryEntry* entry = &diskInfo.directoryEntryTable.entries[fileIndex];
        entry->parentIndex = dirIndex;
        time(&(entry->lastAccessTime));
        return 0;
    }
}

// �����ļ�ϵͳ��Ϣ���ļ����ɹ����� 0 �ļ���ʧ�� -1
int saveFileSystemInfo(const char* filename)
{
    FILE* file = fopen(filename, "wb");
    if (file != NULL) {
        fwrite(&diskInfo, sizeof(DiskInfo), 1, file);
        fwrite(&diskInfo.directoryEntryTable.count, sizeof(unsigned int), 1, file);
        fwrite(diskInfo.directoryEntryTable.entries, sizeof(DirectoryEntry), diskInfo.directoryEntryTable.count, file);
        fclose(file);
        return 0;
    }
    return -1;
}

// ���ļ���ȡ�ļ�ϵͳ��Ϣ���ɹ����� 0 �ļ���ʧ�� -1
int loadFileSystemInfo(const char* filename)
{
    FILE* file = fopen(filename, "rb");
    if (file != NULL) {
        fread(&diskInfo, sizeof(DiskInfo), 1, file);
        fread(&diskInfo.directoryEntryTable.count, sizeof(unsigned int), 1, file);
        diskInfo.directoryEntryTable.entries = malloc(sizeof(DirectoryEntry) * diskInfo.directoryEntryTable.count);
        fread(diskInfo.directoryEntryTable.entries, sizeof(DirectoryEntry), diskInfo.directoryEntryTable.count, file);
        fclose(file);
        return 0;
    }
    return -1;
}

// ��ʾ�ļ�ϵͳ��Ϣ��ȫĿ¼�������
void displayFileSystemInfo(char* infoBuf)
{
    int j = 0;
    j += sprintf(infoBuf + j, "Total Blocks: %u\n", diskInfo.totalBlocks);
    j += sprintf(infoBuf + j, "Free Blocks: %u\n", diskInfo.freeBlocks);
    j += sprintf(infoBuf + j, "Used Blocks: %u\n", diskInfo.totalBlocks - diskInfo.freeBlocks);
    j += sprintf(infoBuf + j, "Block Size: %u\n", BLOCK_SIZE);
    j += sprintf(infoBuf + j, "=== ALL Entry Listing ===\n");
    unsigned int i;
    for (i = 0; i < diskInfo.directoryEntryTable.count; i++) {
        DirectoryEntry* entry = &diskInfo.directoryEntryTable.entries[i];
        char cTimeBuff[80];
        char aTimeBuff[80];
        strftime(cTimeBuff, 80, "%Y-%m-%d %H:%M:%S", localtime(&(entry->createTime)));
        strftime(aTimeBuff, 80, "%Y-%m-%d %H:%M:%S", localtime(&(entry->lastAccessTime)));
        j += sprintf(infoBuf + j, "Index: %-5u[%-6s] Start Block: %-5uSize: %-8u Bytes Name: %-10s Parent: %-5u C: %s A: %s\n", i, entry->isFile ? "File" : "Folder", entry->startBlock, entry->size, entry->name, entry->parentIndex, cTimeBuff, aTimeBuff);
    }
    // printf("%s", infoBuf);
}

// ��ʼ�����̺�Ŀ¼���
void initializeDisk(int numBlocks)
{
    diskInfo.totalBlocks = numBlocks;
    diskInfo.freeBlocks = diskInfo.totalBlocks;
    diskInfo.FAT = malloc(sizeof(unsigned int) * diskInfo.totalBlocks);
    diskInfo.diskData = malloc(BLOCK_SIZE * diskInfo.totalBlocks);
    initializeFAT();
    initializeDirectoryEntryTable(&diskInfo.directoryEntryTable);
}

// �ͷ��ڴ�
void freeDisk()
{
    free(diskInfo.FAT);
    free(diskInfo.diskData);
    freeDirectoryEntryTable(&diskInfo.directoryEntryTable);
}