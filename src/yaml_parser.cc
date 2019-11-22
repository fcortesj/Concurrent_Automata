#include <iostream>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>

#include <yaml-cpp/yaml.h>

int main(int argc, char *argv[]) {

    if (argc != 2) {
        std::cerr << "Usage: ./parser <yaml file>";
        exit(EXIT_FAILURE);
    }

    YAML::Node config = YAML::LoadFile(argv[1]);
    pid_t pid_state;
    int *status;

    for (size_t i = 0; i < config.size(); i++) {
        
        for (size_t j = 0; j < config[i]["delta"].size(); j++) {
            
            if ((pid_state = fork()) == 0) {
                std::cout << "I am the node " << config[i]["delta"][j]["node"] << '\n';
            }
            
            // std::cout << config[i]["delta"][j]["node"] << std::endl;
        }

    }

    wait(status); 
}