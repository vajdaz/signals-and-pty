#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pty.h>
#include <signal.h>
#include <cstring>

volatile sig_atomic_t signal_received = 0;

void signal_handler(int signum) {
    signal_received = signum;

    const char* msg1 = "Parent receiving signal: ";
    const char* msg2 = strsignal(signum);
    const char* newline = "\n";

    write(STDOUT_FILENO, msg1, strlen(msg1));
    write(STDOUT_FILENO, msg2, strlen(msg2));
    write(STDOUT_FILENO, newline, 1);
}

int main() {
    int master_fd, slave_fd;

    // Create a pseudo-terminal pair
    if (openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == -1) {
        std::cerr << "Failed to create pseudo-terminal pair!" << std::endl;
        return 1;
    }

    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGCHLD, &sa, nullptr) == -1) {
        std::cout << "Parent failed to setup SIGCHLD handler.\n";
    }


    pid_t pid = fork();

    if (pid < 0) {
        // Fork failed
        std::cerr << "Fork failed!" << std::endl;
        return 1;
    } else if (pid == 0) {
        // Child process

        // Create a new session. This detaches the child process from the terminal and
        // makes it the leader of a new session This is important for the child process
        // to not be affected by the signals of the parent.
        setsid();

        // Set the controlling terminal to the slave side of the pseudo-terminal.
        if (ioctl(slave_fd, TIOCSCTTY, nullptr) == -1) {
            std::cerr << "Parent failed to set controlling terminal!" << std::endl;
            return 1;
        }

        // Close both file descriptors.
        // Normally the child closes the master file descriptor and connects to the slave.
        // E.g. it calls dup2(slave_fd, STDIN_FILENO); to use the slave as stdin and does
        // simmilar for stdout and stderr. Even then the slave_fd would be closed because
        // the file it represents would be accessed via STDIN_FILENO and not slave_fd would
        // have no use any more.
        close(master_fd);
        close(slave_fd);

        execl("./mychild", "./mychild", (char *)NULL);
        // A successful execl never returns. If it does, it means there was an error.
        std::cerr << "execl failed!" << std::endl;
        return 1;
    } else {
        // Parent process

        // Close the slave file descriptor in the parent
        close(slave_fd);

        std::cout << "Parent pausing...\n";

        while (!signal_received) {
            pause();
        }
    
        std::cout << "Parent get child status..." << std::endl;
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            std::cout << "Parent: Child process exited with status: " << WEXITSTATUS(status) << std::endl;
        } else {
            std::cout << "Parent: Child process did not exit normally." << std::endl;
        }

        close(master_fd);
    }

    return 0;
}