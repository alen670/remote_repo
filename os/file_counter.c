#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

int file_count = 0;

void count_files_recursive(const char *path, const char *extension) {
    char search_path[MAX_PATH];
    WIN32_FIND_DATA find_file_data;
    HANDLE hFind;
    
    // 构建搜索路径，例如 "C:\path\*.*"
    snprintf(search_path, MAX_PATH, "%s\\*.*", path);
    
    hFind = FindFirstFile(search_path, &find_file_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        printf("无法打开目录: %s\n", path);
        return;
    }
    
    do {
        // 跳过 "." 和 ".."
        if (strcmp(find_file_data.cFileName, ".") == 0 || 
            strcmp(find_file_data.cFileName, "..") == 0) {
            continue;
        }
        
        char full_path[MAX_PATH];
        snprintf(full_path, MAX_PATH, "%s\\%s", path, find_file_data.cFileName);
        
        // 如果是目录，递归处理
        if (find_file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            count_files_recursive(full_path, extension);
        } else {
            // 检查文件扩展名
            char *dot = strrchr(find_file_data.cFileName, '.');
            if (dot != NULL && strcmp(dot, extension) == 0) {
                file_count++;
                printf("找到文件: %s\n", full_path);
            }
        }
    } while (FindNextFile(hFind, &find_file_data) != 0);
    
    FindClose(hFind);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("用法: %s <目录> <扩展名>\n", argv[0]);
        printf("示例: %s . .txt\n", argv[0]);
        return 1;
    }
    
    const char *directory = argv[1];
    const char *extension = argv[2];
    
    // 确保扩展名以点号开头
    char full_extension[256];
    if (extension[0] != '.') {
        snprintf(full_extension, sizeof(full_extension), ".%s", extension);
        extension = full_extension;
    }
    
    printf("正在统计目录 '%s' 中扩展名为 '%s' 的文件...\n", directory, extension);
    count_files_recursive(directory, extension);
    printf("共找到 %d 个 '%s' 文件。\n", file_count, extension);
    
    return 0;
}