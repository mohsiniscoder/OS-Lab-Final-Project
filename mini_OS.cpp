#include <iostream>
#include <queue>
#include <functional>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>

// Shared memory key and size
#define SHM_KEY 1234
#define SHM_SIZE 1024

// Task identifiers
#define TASK_ADDITION 1
#define TASK_SUBTRACTION 2
#define TASK_MULTIPLICATION 3
#define TASK_DIVISION 4
#define TASK_MODULUS 5

class TaskManager {
public:
    TaskManager() : deadlockTimeout(1000), processing(false) {}

    void setDeadlockTimeout(int timeoutMs) {
        deadlockTimeout = timeoutMs;
    }

    void addTask(const std::function<void()>& task) {
        std::lock_guard<std::mutex> lock(mutex);
        tasks.push(task);
    }

    void processTasks() {
        processing = true;

        while (!tasks.empty()) {
            auto start = std::chrono::steady_clock::now();
            std::function<void()> task;

            {
                std::lock_guard<std::mutex> lock(mutex);
                task = tasks.front();
                tasks.pop();
            }

            task();
            auto end = std::chrono::steady_clock::now();

            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            if (duration.count() > deadlockTimeout) {
                std::cout << "Potential deadlock detected! Task took longer than the specified timeout." << std::endl;
                break;
            }
        }

        processing = false;
    }

    bool isProcessing() const {
        return processing;
    }

    void clearTasks() {
        std::lock_guard<std::mutex> lock(mutex);
        while (!tasks.empty()) {
            tasks.pop();
        }
    }

private:
    std::queue<std::function<void()>> tasks;
    std::mutex mutex;
    int deadlockTimeout;
    std::atomic<bool> processing;
};

// Sample task functions
void additionTask(int a, int b) {
    int result = a + b;
    std::cout << "Addition: " << a << " + " << b << " = " << result << std::endl;
}

void subtractionTask(int a, int b) {
    int result = a - b;
    std::cout << "Subtraction: " << a << " - " << b << " = " << result << std::endl;
}

void multiplicationTask(int a, int b) {
    int result = a * b;
    std::cout << "Multiplication: " << a << " * " << b << " = " << result << std::endl;
}

void divisionTask(int a, int b) {
    if (b != 0) {
        double result = static_cast<double>(a) / b;
        std::cout << "Division: " << a << " / " << b << " = " << result << std::endl;
    } else {
        std::cout << "Division by zero error!" << std::endl;
    }
}

void modulusTask(int a, int b) {
    if (b != 0) {
        int result = a % b;
        std::cout << "Modulus: " << a << " % " << b << " = " << result << std::endl;
    } else {
        std::cout << "Modulus by zero error!" << std::endl;
    }
}

// Function to perform arithmetic task
void performArithmeticTask(int a, int b, const std::function<void(int, int)>& taskFunction) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    taskFunction(a, b);
}

// Function to write task data to shared memory
void writeToSharedMemory(int taskIdentifier, int number1, int number2) {
    int shmId = shmget(SHM_KEY, SHM_SIZE, IPC_CREAT | 0666);
    if (shmId == -1) {
        std::cerr << "Failed to create shared memory segment." << std::endl;
        exit(EXIT_FAILURE);
    }

    int* sharedMemory = (int*)shmat(shmId, NULL, 0);
    if (sharedMemory == (int*)(-1)) {
        std::cerr << "Failed to attach shared memory segment." << std::endl;
        exit(EXIT_FAILURE);
    }

    sharedMemory[0] = taskIdentifier;
    sharedMemory[1] = number1;
    sharedMemory[2] = number2;

    shmdt(sharedMemory);
}

// Function to read task data from shared memory
void readFromSharedMemory(int& taskIdentifier, int& number1, int& number2) {
    int shmId = shmget(SHM_KEY, SHM_SIZE, 0666);
    if (shmId == -1) {
        std::cerr << "Failed to get shared memory segment." << std::endl;
        exit(EXIT_FAILURE);
    }

    int* sharedMemory = (int*)shmat(shmId, NULL, 0);
    if (sharedMemory == (int*)(-1)) {
        std::cerr << "Failed to attach shared memory segment." << std::endl;
        exit(EXIT_FAILURE);
    }

    taskIdentifier = sharedMemory[0];
    number1 = sharedMemory[1];
    number2 = sharedMemory[2];

    shmdt(sharedMemory);
}

// Function to display menu and get user choice
int displayMenu() {
    int choice;
    std::cout << "Menu:" << std::endl;
    std::cout << "1. Set deadlock timeout" << std::endl;
    std::cout << "2. Add task" << std::endl;
    std::cout << "3. Show Shared Memory Contents" << std::endl; // Added option to show shared memory contents
    std::cout << "4. Clear Shared Memory" << std::endl; // Added option to clear shared memory
    std::cout << "5. Display Task Manager Status" << std::endl; // Added option to display task manager status
    std::cout << "6. Exit" << std::endl;
    std::cout << "Enter your choice: ";
    std::cin >> choice;
    return choice;
}

// Function to display welcome page
void displayWelcomePage() {
    // ANSI escape code for blue color
    std::cout << "\033[1;34m"; // 1 for bold, 34 for blue

    std::cout << "==================================" << std::endl;
    std::cout << "       Welcome to Task Manager    " << std::endl;
    std::cout << "==================================" << std::endl;
    std::cout << std::endl;

    // Reset color
    std::cout << "\033[0m";
}

// Function to display shared memory contents
void displaySharedMemoryContents() {
    int taskIdentifier, number1, number2;
    readFromSharedMemory(taskIdentifier, number1, number2);

    std::cout << "Shared Memory Contents:" << std::endl;
    std::cout << "Task Identifier: " << taskIdentifier << std::endl;
    std::cout << "Number 1: " << number1 << std::endl;
    std::cout << "Number 2: " << number2 << std::endl;
}

int main() {
    displayWelcomePage();
    TaskManager taskManager;

    while (true) {
        int choice = displayMenu();
        switch (choice) {
            case 1: {
                int timeout;
                std::cout << "Enter deadlock timeout in milliseconds: ";
                std::cin >> timeout;
                taskManager.setDeadlockTimeout(timeout);
                break;
            }
            case 2: {
                int taskIdentifier, number1, number2;
                std::cout << "Enter task identifier (1: Addition, 2: Subtraction, 3: Multiplication, 4: Division, 5: Modulus): ";
                std::cin >> taskIdentifier;
                std::cout << "Enter the first number: ";
                std::cin >> number1;
                std::cout << "Enter the second number: ";
                std::cin >> number2;

                // Write the task data to shared memory
                writeToSharedMemory(taskIdentifier, number1, number2);

                // Fork a child process
                pid_t pid = fork();

                if (pid < 0) {
                    std::cerr << "Fork failed." << std::endl;
                    exit(EXIT_FAILURE);
                } else if (pid == 0) {
                    // Child process
                    int taskIdentifier, number1, number2;
                    readFromSharedMemory(taskIdentifier, number1, number2);
                    switch (taskIdentifier) {
                        case TASK_ADDITION:
                            taskManager.addTask([=]() { performArithmeticTask(number1, number2, additionTask); });
                            break;
                        case TASK_SUBTRACTION:
                            taskManager.addTask([=]() { performArithmeticTask(number1, number2, subtractionTask); });
                            break;
                        case TASK_MULTIPLICATION:
                            taskManager.addTask([=]() { performArithmeticTask(number1, number2, multiplicationTask); });
                            break;
                        case TASK_DIVISION:
                            taskManager.addTask([=]() { performArithmeticTask(number1, number2, divisionTask); });
                            break;
                        case TASK_MODULUS:
                            taskManager.addTask([=]() { performArithmeticTask(number1, number2, modulusTask); });
                            break;
                        default:
                            std::cerr << "Invalid task identifier." << std::endl;
                            exit(EXIT_FAILURE);
                    }

                    taskManager.processTasks();
                    exit(EXIT_SUCCESS);
                } else {
                    // Parent process
                    int status;
                    waitpid(pid, &status, 0);
                }
                break;
            }
            case 3:
                displaySharedMemoryContents(); // Display shared memory contents
                break;
            case 4:
                // Clear shared memory
                shmctl(shmget(SHM_KEY, SHM_SIZE, 0), IPC_RMID, NULL);
                std::cout << "Shared memory cleared." << std::endl;
                break;
            case 5:
                // Display task manager status
                if (taskManager.isProcessing()) {
                    std::cout << "Task manager is currently processing tasks." << std::endl;
                } else {
                    std::cout << "Task manager is idle." << std::endl;
                }
                break;
            case 6:
                std::cout << "Exiting..." << std::endl;
                return 0;
            default:
                std::cout << "Invalid choice. Please try again." << std::endl;
        }
    }

    return 0;
}