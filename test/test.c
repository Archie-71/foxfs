#include <stdio.h>

// 在 main 函数中测试
int main()
{
    initializeDisk(1024);
    char buffer[2048];

    // 示例操作
    createFile("file1.txt", 1000);
    createFile("file2.txt", 2048);
    createDirectory("folder1");

    listCurrentDirectoryEntry(buffer);
    printf("ls result:\n%s", buffer);

    setCurrentDirectoryIndex("folder1");
    createFile("file3.txt", 3000);
    createFile("file4.txt", 120);
    deleteFile("file3.txt");

    listCurrentDirectoryEntry(buffer);
    printf("ls result:\n%s", buffer);

    renameFile("file2.txt", "file4.c");
    listCurrentDirectoryEntry(buffer);
    printf("ls result:\n%s", buffer);

    setCurrentDirectoryIndex("/");
    moveFile("file1.txt", "folder1");
    listCurrentDirectoryEntry(buffer);
    printf("ls result:\n%s", buffer);

    displayFileSystemInfo(buffer);
    printf("display all result:\n%s", buffer);

    // 保存文件系统信息到文件
    saveFileSystemInfo("filesystem.dat");

    // 清理内存
    freeDisk();

    // 读取文件系统信息
    loadFileSystemInfo("filesystem.dat");
    displayFileSystemInfo(buffer);
    printf("display all result:\n%s", buffer);

    return 0;
}