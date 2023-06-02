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

// 目录项
typedef struct {
    char name[MAX_FILENAME_LENGTH];
    unsigned int startBlock;
    unsigned int size;
    int isFile;
    unsigned int parentIndex;
    time_t createTime;
    time_t lastAccessTime;
} DirectoryEntry;

// 目录项表
typedef struct {
    DirectoryEntry* entries;
    unsigned int count;
} DirectoryEntryTable;

// 磁盘
typedef struct {
    unsigned int totalBlocks;
    unsigned int freeBlocks;
    unsigned int* FAT;
    unsigned char* diskData;
    DirectoryEntryTable directoryEntryTable;
} DiskInfo;

// 全局变量磁盘和当前目录
DiskInfo diskInfo;
unsigned int currentDirectoryIndex; // 当前目录对应的目录表索引

// 函数定义
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

// 分配一个空闲块
unsigned int allocateBlock()
{
    unsigned int block = 0;
    // 在 FAT 表中查找空闲块
    unsigned int i;
    for (i = 1; i < diskInfo.totalBlocks; i++) {
        if (diskInfo.FAT[i] == FAT_FREE) {
            diskInfo.FAT[i] = FAT_END; // 标记块为已分配
            diskInfo.freeBlocks--;
            block = i;
            break;
        }
    }
    return block;
}

// 分配一系列块，返回分配成功的块数
unsigned int* allocateBlocks(unsigned int numBlocks)
{
    if (numBlocks > diskInfo.freeBlocks) {
        // printf("Not enough free blocks to allocate.\n");
        return NULL;
    }

    unsigned int* blocks = malloc(numBlocks * sizeof(unsigned int));
    unsigned int allocatedBlocks = 0;
    unsigned int blockIndex = 1; // 跳过文件夹的假索引

    // 遍历 FAT 表，找到连续的空闲块
    while (allocatedBlocks < numBlocks) {
        if (diskInfo.FAT[blockIndex] == FAT_FREE) {
            blocks[allocatedBlocks] = blockIndex;
            allocatedBlocks++;

            // 更新 FAT 表中的链接关系
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

// 释放某个块的物理空间
void freeBlock(unsigned int block)
{
    if (block >= 1 && block < diskInfo.totalBlocks) {
        diskInfo.FAT[block] = FAT_FREE; // 标记块为空闲
        diskInfo.freeBlocks++;
    }
}

// 释放文件所占用的所有块，返回释放块个数
int freeBlocks(unsigned int startBlock)
{
    int freedBlocksCount = 0;
    if (startBlock >= 1 && startBlock < diskInfo.totalBlocks) {
        unsigned int currentBlock = startBlock;
        unsigned int nextBlock;

        while (currentBlock != FAT_END) {
            nextBlock = diskInfo.FAT[currentBlock];

            // 将当前块标记为空闲
            diskInfo.FAT[currentBlock] = FAT_FREE;
            diskInfo.freeBlocks++;

            currentBlock = nextBlock;
            freedBlocksCount++;
        }
        return freedBlocksCount;
    }
    return -1; // 磁盘已满或非法起始块
}

// 初始化 FAT 表
void initializeFAT()
{
    unsigned int i;
    for (i = 0; i < diskInfo.totalBlocks; i++) {
        diskInfo.FAT[i] = FAT_FREE;
    }
}

// 初始化目录项表
void initializeDirectoryEntryTable(DirectoryEntryTable* directoryEntryTable)
{
    directoryEntryTable->entries = NULL;
    directoryEntryTable->count = 0;
    // 初始化根目录
    createDirectory("/");
    currentDirectoryIndex = directoryEntryTable->count - 1;
}

// 释放目录项表的内存
void freeDirectoryEntryTable(DirectoryEntryTable* directoryEntryTable)
{
    if (directoryEntryTable->entries != NULL) {
        free(directoryEntryTable->entries);
    }
    directoryEntryTable->count = 0;
}

// 添加目录项到目录项表
void addDirectoryEntry(DirectoryEntryTable* directoryEntryTable, const char* name, unsigned int startBlock, unsigned int size, int isFile, unsigned int prev)
{
    directoryEntryTable->count++;
    DirectoryEntry* newEntries = (DirectoryEntry*)malloc(sizeof(DirectoryEntry) * directoryEntryTable->count);

    // 初始化新分配的内存
    memset(newEntries, 0, sizeof(DirectoryEntry) * directoryEntryTable->count);

    // 将原有数据拷贝到新分配的内存中
    if (directoryEntryTable->count > 1)
    {
        memcpy(newEntries, directoryEntryTable->entries, sizeof(DirectoryEntry) * (directoryEntryTable->count - 1));
        free(directoryEntryTable->entries);
    }

    directoryEntryTable->entries = newEntries;

    // 新增目录项
    DirectoryEntry* entry = &directoryEntryTable->entries[directoryEntryTable->count - 1];
    strncpy(entry->name, name, MAX_FILENAME_LENGTH);
    entry->startBlock = startBlock;
    entry->size = size;
    entry->isFile = isFile;
    entry->parentIndex = prev;
    time(&(entry->lastAccessTime));
    time(&(entry->createTime));
}

// 获取目录项在目录项表中的索引，不存在返回 -1
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

// 创建文件并分配一系列块和维护目录项表，成功返回 0 重名 -1 无可用空间 -2
int createFile(const char* name, unsigned int fileSize)
{
    // 重名文件处理
    int index = getDirectoryEntryIndex(&diskInfo.directoryEntryTable, name);
    if (index != -1) {
        DirectoryEntry* entry = &diskInfo.directoryEntryTable.entries[index];
        if (entry->parentIndex == currentDirectoryIndex) {
            // printf("File already exsists.\n");
            return -1;
        }
    }
    unsigned int curIndex = diskInfo.directoryEntryTable.count - 1; // 获取最新的目录项索引
    unsigned int numBlocks = (fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE; // 计算所需块数
    unsigned int* blocks = allocateBlocks(numBlocks); // 分配块
    if (blocks != NULL) {
        unsigned int startBlock = blocks[0]; // 获取起始块
        addDirectoryEntry(&diskInfo.directoryEntryTable, name, startBlock, fileSize, 1, currentDirectoryIndex);
        return 0;
    }
    return -2;
}

// 删除文件，成功返回 0 文件不存在 -1
int deleteFile(const char* name)
{
    // 查找有无该文件
    int index = getDirectoryEntryIndex(&diskInfo.directoryEntryTable, name);
    if (index != -1) {
        unsigned int startBlock = diskInfo.directoryEntryTable.entries[index].startBlock;
        unsigned int size = diskInfo.directoryEntryTable.entries[index].size;
        // 释放文件占用的物理空间
        freeBlocks(startBlock);
        // 从目录项表中删除目录项
        unsigned int i;
        for (i = index; i < diskInfo.directoryEntryTable.count - 1; i++) {
            diskInfo.directoryEntryTable.entries[i] = diskInfo.directoryEntryTable.entries[i + 1];
        }
        diskInfo.directoryEntryTable.count--;
        return 0;
    }
    return -1;
}

// 创建文件夹，成功返回 0 重名 -1
int createDirectory(const char* name)
{
    // 重名文件夹处理
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

// 删除文件夹，成功返回 0 不存在 -1
int deleteDirectory(const char* name)
{
    int index = getDirectoryEntryIndex(&diskInfo.directoryEntryTable, name);
    if (index != -1) {
        // 从目录项表中删除目录项
        unsigned int i;
        for (i = index; i < diskInfo.directoryEntryTable.count - 1; i++) {
            diskInfo.directoryEntryTable.entries[i] = diskInfo.directoryEntryTable.entries[i + 1];
        }
        diskInfo.directoryEntryTable.count--;
        return 0;
    }
    return -1;
}

// 设置当前文件夹，成功返回 0 文件 -1
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

// 列出当前文件夹下的所有文件和文件夹的属性
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

// 重命名文件，成功返回 0 不存在 -1
int renameFile(const char* filename, const char* newname)
{
    // 查找文件是否存在
    int index = getDirectoryEntryIndex(&diskInfo.directoryEntryTable, filename);
    if (index != -1) {
        DirectoryEntry* entry = &diskInfo.directoryEntryTable.entries[index];
        strcpy(entry->name, newname);
        time(&(entry->lastAccessTime));
        return 0;
    }
    return -1;
}

// 移动文件到文件夹，成功返回 0 文件不存在 -1 文件夹不存在 -2
int moveFile(const char* filename, const char* dirname)
{
    // 查找文件和文件夹
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

// 保存文件系统信息到文件，成功返回 0 文件打开失败 -1
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

// 从文件读取文件系统信息，成功返回 0 文件打开失败 -1
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

// 显示文件系统信息（全目录项遍历）
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

// 初始化磁盘和目录项表
void initializeDisk(int numBlocks)
{
    diskInfo.totalBlocks = numBlocks;
    diskInfo.freeBlocks = diskInfo.totalBlocks;
    diskInfo.FAT = malloc(sizeof(unsigned int) * diskInfo.totalBlocks);
    diskInfo.diskData = malloc(BLOCK_SIZE * diskInfo.totalBlocks);
    initializeFAT();
    initializeDirectoryEntryTable(&diskInfo.directoryEntryTable);
}

// 释放内存
void freeDisk()
{
    free(diskInfo.FAT);
    free(diskInfo.diskData);
    freeDirectoryEntryTable(&diskInfo.directoryEntryTable);
}