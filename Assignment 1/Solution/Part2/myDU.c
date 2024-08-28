#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

long long int calculateDirectorySize(const char *dirPath);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Unable to execute\n");
        return 1;
    }

    const char *dirPath = argv[1];
    long long int totalSize = calculateDirectorySize(dirPath);

    printf("%lld", totalSize);

    return 0;
}

long long int calculateDirectorySize(const char *dirPath) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        printf("Unable to execute\n");
        exit(1);
    }

    pid_t child_pid = fork();
    if (child_pid == -1) {
        printf("Unable to execute\n");
        exit(1);
    }

    if (child_pid == 0) {  // Child process
        close(pipefd[0]);  // Close read end of the pipe

        // Redirect stdout to the write end of the pipe
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            printf("Unable to execute\n");
            exit(1);
        }

        // Calculate the directory size including subdirectories
        long long int size = 0;
        struct dirent *entry;
        struct stat entryInfo;
        DIR *dir = opendir(dirPath);

        if (!dir) {
            printf("Unable to execute\n");
            exit(1);
        }


        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, "..") == 0) {
                continue;
            }


            if (strcmp(entry->d_name, ".") == 0) {
                struct stat dirInfo;
		        if (stat(dirPath, &dirInfo) == 0) {
		            size += dirInfo.st_size;
		        }
		        continue;
            }
            

            char filePath[1024];
            snprintf(filePath, sizeof(filePath), "%s/%s", dirPath, entry->d_name);

            if (stat(filePath, &entryInfo) == 0) {
                if (S_ISDIR(entryInfo.st_mode)) {
                    // Recursively calculate the size of subdirectories
                    size += calculateDirectorySize(filePath);
                } else if (S_ISREG(entryInfo.st_mode)) {
                    // Add the size of regular files
                    size += entryInfo.st_size;
                }
            }
        }

        closedir(dir);
        printf("%lld\n", size);

        exit(0);
    } else {  // Parent process
        close(pipefd[1]);  // Close write end of the pipe

        int status;
        waitpid(child_pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            char buffer[1024];
            long long int totalSize;

            // Read the result from the pipe
            ssize_t bytes_read = read(pipefd[0], buffer, sizeof(buffer));
            if (bytes_read == -1) {
                printf("Unable to execute\n");
                exit(1);
            }

            // Null-terminate the string
            buffer[bytes_read] = '\0';

            // Convert the result to a long long integer
            sscanf(buffer, "%lld", &totalSize);

            close(pipefd[0]);

            return totalSize;
        } else {
            printf("Unable to execute\n");
            exit(1);
        }
    }
}
