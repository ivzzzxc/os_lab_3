#include <iostream>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <string>

char* getFilePath() {
    char symbol;
    char* filePath = (char*)malloc(sizeof(char));
    int counter = 0;
    int size = 1;

    std::cout << "Enter file path" << std::endl;

    while ((symbol = getchar()) != '\n') {
        filePath[counter++] = symbol;

        if (counter == size) {
            size *= 2;
            filePath = (char*)realloc(filePath, (counter + 1) * sizeof(char));
        }
    }

    filePath = (char*)realloc(filePath, size * sizeof(char));
    filePath[counter] = '\0';
    return filePath;
}

char* getInputString(int& size) {
    char symbol;
    char* in = (char*)malloc(sizeof(char));
    int counter = 0;
    size = 1;

    std::cout << "Enter something strings. If you want to stop, press Enter + Ctrl+D" << std::endl;

    while ((symbol = getchar()) != EOF) {
        in[counter++] = symbol;

        if (counter == size) {
            size *= 2;
            in = (char*)realloc(in, size * sizeof(char));
        }
    }

    size = counter + 1;
    in = (char*)realloc(in, size * sizeof(char));
    in[size - 1] = '\0';
    
    return in;
}

void cleanupResources(char* in, char* filePath, char* ptr, char* f, int* size, int fd) {
    free(in);
    free(filePath);
    munmap(ptr, *size * sizeof(char));
    munmap(f, *size * sizeof(char));
    munmap(size, sizeof(int));

    if (fd >= 0) {
        close(fd);
    }
}

void checkValidStr(const std::string & str, std::string & fileStr, std::string & outStr) {
    if (!str.empty()) {
        if (str.back() == '.' || str.back() == ';') {
            fileStr += str;
        } else {
            outStr += str;
        }
    }

    if (!fileStr.empty() && fileStr.back() != '\n') {
        fileStr += '\n';
    }
}

void checkValidStrings(const char* ptr, int size, std::string& fileStr, std::string& outStr) {
    std::string str;

    for (int i = 0; i < size; ++i) {
        if (i != size - 1) {
            str += ptr[i];
        }

        if (ptr[i] == '\n' || i == size - 1) {
            if ((i > 0) && (ptr[i - 1] == '.' || ptr[i - 1] == ';')) {
                fileStr += str;
            } else {
                outStr += str;
            }

            str.clear();
        }
    }
}

void lineBrakeCheck(std::string & str) {
    if (!str.empty() && str.back() != '\n') {
        str += '\n';
    }
}

int main() {
    char* filePath = getFilePath(); // Получаем путь к файлу
    int size;
    char* in = getInputString(size); // Получаем ввод строки

    char* ptr = (char*)mmap(NULL, size * sizeof(char), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
    char* f = nullptr;
    int* sizePtr = (int*)mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
    int fd = -1;

    if (ptr == MAP_FAILED) {
        std::perror("ERROR! mmap: array symbol");
        cleanupResources(in, filePath, ptr, f, sizePtr, fd);
        exit(EXIT_FAILURE);
    }

    strcpy(ptr, in);

    int flags = O_RDWR | O_CREAT;
    int mods = S_IRWXU | S_IRWXG | S_IRWXO;
    fd = open(filePath, flags, mods);

    if (fd < 0) {
        std::perror("ERROR! file: not opened");
        cleanupResources(in, filePath, ptr, f, sizePtr, fd);
        exit(EXIT_FAILURE);
    }

    f = (char*)mmap(NULL, size * sizeof(char), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (f == MAP_FAILED) {
        std::perror("ERROR! mmap: file create");
        cleanupResources(in, filePath, ptr, f, sizePtr, fd);
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();

    if (pid < 0) {
        std::perror("ERROR! fork: child process creation failed");
        cleanupResources(in, filePath, ptr, f, sizePtr, fd);
        exit(EXIT_FAILURE);
    }

    if (pid == 0) { // child
        std::string str, fileStr, outStr;

        checkValidStrings(ptr, size, fileStr, outStr);

        lineBrakeCheck(fileStr);

        if (fileStr.length() != 0) {
            if (ftruncate(fd, std::max(static_cast<int>(fileStr.length()), 1) * sizeof(char)) == -1) {
                std::perror("ERROR! ftruncate: file is not cut");
                cleanupResources(in, filePath, ptr, f, sizePtr, fd);
                exit(EXIT_FAILURE);
            }

            if ((f = (char*)mremap(f, size * sizeof(char), (fileStr.length() + 1) * sizeof(char), MREMAP_MAYMOVE)) == (void*)-1) {
                std::perror("ERROR! mremap: not resize memory");
                cleanupResources(in, filePath, ptr, f, sizePtr, fd);
                exit(EXIT_FAILURE);
            }

            sprintf(f, "%s", fileStr.c_str());
        }

        lineBrakeCheck(outStr);

        if ((ptr = (char*)mremap(ptr, size * sizeof(char), outStr.length() + 1, MREMAP_MAYMOVE)) == (void*)-1) {
            std::perror("ERROR! mremap: Failed to cut file by line");
            cleanupResources(in, filePath, ptr, f, sizePtr, fd);
            exit(EXIT_FAILURE);
        }

        *sizePtr = outStr.length() + 1;
        sprintf(ptr, "%s", outStr.c_str());
        
        exit(EXIT_SUCCESS);
    } else { // parent
        int wstatus;
        waitpid(pid, &wstatus, 0);

        if (wstatus) {
            std::perror("Child process exited with an error");
            cleanupResources(in, filePath, ptr, f, sizePtr, fd);
            exit(EXIT_FAILURE);
        }

        struct stat statbuf;
        if (fstat(fd, &statbuf) < 0) {
            std::perror("ERROR! fstat: cannot open file");
            cleanupResources(in, filePath, ptr, f, sizePtr, fd);
            exit(EXIT_FAILURE);
        }

        int fileLength = std::max(static_cast<int>(statbuf.st_size), 1);

        std::cout << "This strings end with '.' or ';' :" << std::endl;
        if (fileLength > 1) {
            std::cout << f << std::endl;
        }

        std::cout << "This strings not end with '.' or ';' :" << std::endl;
        std::cout << ptr;
    }

    cleanupResources(in, filePath, ptr, f, sizePtr, fd);

    return 0;
}