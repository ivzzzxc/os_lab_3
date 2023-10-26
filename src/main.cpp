#include <iostream>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <string>

int main()
{
    char symbol;
    char* in = (char* )malloc(sizeof(char));
    char* filePath = (char* )malloc(sizeof(char));
    
    int* size = (int* )mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
    int counter = 0;

    if (size == MAP_FAILED) {
        std::perror("ERROR! mmap: int size");
        exit(EXIT_FAILURE);
    }

    *size = 1;
    
    std::cout << "Enter file path" << std::endl;

    while ((symbol = getchar()) !=  '\n') {
        filePath[counter++] = symbol;
        
        if (counter == *size) {
            *size *= 2;
            filePath = (char* )realloc(filePath, (counter + 1) * sizeof(char));
        }
    }

    filePath = (char* )realloc(filePath, (*size) * sizeof(char));
    filePath[counter] = '\0';
    counter = 0;
    *size = 1;

    std::cout << "Enter something strings. If you want to stoped, press enter Ctrl+D" << std::endl;

    while ((symbol = getchar()) != EOF) {
        in[counter++] = symbol;

        if (counter == *size) {
            *size *= 2;
            in = (char* )realloc(in, (*size) * sizeof(char));
        }
    }

    *size = counter + 1;
    in = (char* )realloc(in, (*size) * sizeof(char));
    in[(*size) - 1] = '\0';

    char* ptr = (char* )mmap(NULL, (*size) * sizeof(char), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0 , 0);

    if (ptr == MAP_FAILED) {
        std::perror("ERROR! mmap: array symbol");
        free(in);
        free(filePath);

        int err = munmap(size, sizeof(int));

        if (err != 0) {
            std::perror("ERROR! munmap: delete");
        }

        exit(EXIT_FAILURE);
    }

    strcpy(ptr, in);

    int flags = O_RDWR | O_CREAT;
    int mods = S_IRWXU | S_IRWXG | S_IRWXO;
    int fd = open(filePath, flags, mods);

    if (fd < 0) {
        std::perror("ERROR! file: not opened");
        free(in);
        free(filePath);
        int err1 = munmap(ptr, (*size) * sizeof(char));
        int err2 = munmap(size, sizeof(int));

        if ((err1 != 0) || (err2 != 0)) {
            std::perror("ERROR! munmap: delete");
        }

        exit(EXIT_FAILURE);
    }

    char* f = (char* )mmap(NULL, sizeof(char), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (f == MAP_FAILED) {
        std::perror("ERROR! mmap: file create");
        free(in);
        free(filePath);

        int err1 = munmap(ptr, (*size) * sizeof(char));
        int err2 = munmap(size, sizeof(int));

        if ((err1 != 0) || (err2 != 0)) {
            std::perror("ERROR! munmap: delete");
        }

        exit(EXIT_FAILURE);
    }
    
    pid_t pid = fork();

    if (pid < 0) {
        std::perror("ERROR! fork: child didn't created");

        free(in);
        free(filePath);

        int err1 = munmap(ptr, (*size) * sizeof(char));
        int err2 = munmap(size, sizeof(int));

        if ((err1 != 0) || (err2 != 0)) {
            std::perror("ERROR! munmap: delete");
        }

        exit(EXIT_FAILURE);
    }

    if (pid == 0) { // child
        std::string str, fileStr, outStr;

        for (size_t i = 0; i < *size; ++i) {
            if (i != (*size) - 1) {
                str += ptr[i];
            }

            if ((ptr[i] == '\n') || (i == (*size) - 1)) {
                if ((i > 0) && ((ptr[i - 1] == '.') || (ptr[i - 1] == ';'))) {
                    fileStr += str;
                } else {
                    outStr += str;
                }

                str.clear();
            }
        }

        if ((fileStr.length()) && (fileStr[fileStr.length() - 1] != '\n')) {
            fileStr += '\n';
        }

        if (fileStr.length() != 0) {
            if (ftruncate(fd, std::max((int)fileStr.length(), 1) * sizeof(char)) == -1) {
                std::perror("ERROR! ftruncate: file is not cut");

                free(in);
                free(filePath);

                exit(EXIT_FAILURE);
            }

            if ((f = (char*)mremap(f, sizeof(char), (fileStr.length() + 1) * sizeof(char), MREMAP_MAYMOVE)) == (void* ) -1) {
                std::perror("ERROR! mremap: not resize memmory");

                free(in);
                free(filePath);
                exit(EXIT_FAILURE);
            }

            sprintf(f, "%s", fileStr.c_str());
        }

        if ((outStr.length()) && (outStr[outStr.length() - 1] != '\n')) {
            outStr += '\n';
        }

        if ((ptr = (char* )mremap(ptr, (*size) * sizeof(char), outStr.length() + 1, MREMAP_MAYMOVE)) == (void* )-1) {
            std::perror("ERROR! mremap: Failed to cut file by line");

            free(in);
            free(filePath);
            exit(EXIT_FAILURE);
        }

        *size = outStr.length() + 1;
        sprintf(ptr, "%s", outStr.c_str());
    } else { // parent
        int wstatus;
        waitpid(pid, &wstatus, 0);

        if (wstatus) {
            free(in);
            free(filePath);

            int err1 = munmap(ptr, (*size) * sizeof(char));
            int err2 = munmap(f, counter * sizeof(char));
            int err3 = munmap(size, sizeof(int));

            if ((err1 != 0) || (err2 != 0) || (err3 != 0)) {
                std::perror("ERROR! munmap: delete");
            }   

            exit(EXIT_FAILURE);
        }

        struct stat statbuf;
        if (fstat(fd, &statbuf) < 0) {
            std::perror("ERROR! fstat: cannot open file");
            exit(EXIT_FAILURE);
        }

        counter = std::max((int)statbuf.st_size, 1);

        std::cout << "This strings end to '.' or ';' :" << std::endl;

        if (statbuf.st_size > 1) {
            std::cout << f << std::endl;
        }

        std::cout << "This strings not end to '.' or ';' :" << std::endl;
        std::cout << ptr;
        close(fd);

        int err1 = munmap(ptr, (*size) * sizeof(char));
        int err2 = munmap(f, counter * sizeof(char));
        int err3 = munmap(size, sizeof(int));

        if ((err1 != 0) || (err2 != 0) || (err3 != 0)) {
            std::perror("ERROR! munmap: delete");
            free(in);
            free(filePath);

            exit(EXIT_FAILURE);
        }
    }

    free(in);
    free(filePath);

    return 0;
}