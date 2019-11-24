#include <unistd.h>
#include <string>

struct Edge {
    std::string origin;
    std::string destination;
    std::string message;
    int pip[2];

    Edge(std::string origin, std::string destination, std::string message)
        : origin(origin), destination(destination), message(message) {
        
        // create the pipe
        if (pipe(pip) == -1) {
            std::cerr << "ERROR!: pipe could not be created." << std::endl;
            exit(EXIT_FAILURE);
        }
    }
};