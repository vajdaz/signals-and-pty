#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pty.h>
#include <signal.h>
#include <cstring>

volatile sig_atomic_t child_died = 0;

void signal_handler(int signum) {
    if (signum != SIGCHLD) {
        child_died = signum;
    }

    const char* msg1 = "Parent receiving signal: ";
    const char* msg2 = strsignal(signum);
    const char* newline = "\n";

    write(STDOUT_FILENO, msg1, strlen(msg1));
    write(STDOUT_FILENO, msg2, strlen(msg2));
    write(STDOUT_FILENO, newline, 1);
}

int main() {
    // Create a pseudo-terminal pair
    int master_fd = TEMP_FAILURE_RETRY(open("/dev/ptmx", O_RDWR | O_CLOEXEC));

    if ( master_fd < 0 ) {
        perror("open /dev/ptmx failed");
        return 1;
    }

    if ( unlockpt(master_fd) < 0 ) {
        perror("unlockpt failed");
        return 1;
    }
    
    if ( grantpt(master_fd) < 0 ) {
        perror("grantpt failed");
        return 1;
    }
    
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGCHLD, &sa, nullptr) == -1) {
        perror("setup SIGCHLD handler failed in parent");
        return 1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed!");
        return 1;
    } else if (pid == 0) {
        // Child process

        // Create a new session. This detaches the child process from the terminal and
        // makes it the leader of a new session This is important for the child process
        // to not be affected by the signals of the parent. After this call the child
        // will not have a controlling terminal.
        setsid();

        char slave_name[64];
        if ( ptsname_r(master_fd, slave_name, sizeof(slave_name)) != 0 ) {
            perror("ptsname_r failed");
            return 1;
        }

        // The fist terminal opened by the child process becomes the controlling terminal.
        int slave_fd = TEMP_FAILURE_RETRY(open(slave_name, O_RDWR));
        if ( slave_fd < 0 ) {
            perror("open slave failed");
            return 1;
        }

        // Close both file descriptors.
        // Normally the child closes the master file descriptor and connects to the slave.
        // E.g. it calls dup2(slave_fd, STDIN_FILENO); to use the slave as stdin and does
        // simmilar for stdout and stderr. Even then the slave_fd would be closed because
        // the file it represents would be accessed via STDIN_FILENO and not slave_fd would
        // have no use any more.
        TEMP_FAILURE_RETRY(close(master_fd));
        TEMP_FAILURE_RETRY(dup2(slave_fd, STDIN_FILENO));
        //TEMP_FAILURE_RETRY(dup2(slave_fd, STDOUT_FILENO));
        //TEMP_FAILURE_RETRY(dup2(slave_fd, STDERR_FILENO));
        TEMP_FAILURE_RETRY(close(slave_fd));

        execl("./mychild", "./mychild", (char *)NULL);
        // A successful execl never returns. If it does, it means there was an error.
        perror("execl failed in child");
        return 1;
    } else {
        // Parent process

        std::cout << "Parent: waiting for child to die..." << std::endl;
        int status;
        TEMP_FAILURE_RETRY(waitpid(pid, &status, 0));
        if (WIFEXITED(status)) {
            std::cout << "Parent: Child process exited with status: " << WEXITSTATUS(status) << std::endl;
        } else {
            std::cout << "Parent: Child process did not exit normally." << std::endl;
        }
    }
}