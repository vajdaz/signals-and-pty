#include <csignal>
#include <unistd.h>
#include <cstring>
#include <iostream>

volatile sig_atomic_t signal_received = 0;

void signal_handler(int signum) {
    signal_received = signum;

    const char* msg1 = "Child received signal: ";
    const char* msg2 = strsignal(signum);
    const char* newline = "\n";

    write(STDOUT_FILENO, msg1, strlen(msg1));
    write(STDOUT_FILENO, msg2, strlen(msg2));
    write(STDOUT_FILENO, newline, 1);
}

int main() {
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGHUP, &sa, nullptr) == -1) {
        std::cout << "Child failed to setup SIGHUP handler.\n";
    }

    std::cout << "Child pausing...\n";

    while (!signal_received) {
        pause();
    }
    
    std::cout << "Child exiting.\n";
}
