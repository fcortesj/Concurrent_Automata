#include <iostream>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <map>
#include <vector>

#include <yaml-cpp/yaml.h>
#include "edge.h"

void assign_intern_pipes(std::map<std::string, std::vector<Edge>> &transitions, std::string automaton_name, std::string state_name, YAML::Node config);
void assign_error_pipes(std::map<std::string, std::map<std::string, int[2]>> &error_pipes, std::string automaton_name, std::string state_name, YAML::Node config);
void assign_ending_pipes(std::map<std::string, std::map<std::string, int[2]>> &ending_pipes, std::string automaton_name, std::string state_name, YAML::Node config);
void assign_initial_pipes(std::map<std::string, std::map<std::string, int[2]>> &initial_pipes, std::string automaton_name, std::string state_name, YAML::Node config);
void create_error_pipes(std::map<std::string, std::map<std::string, int[2]>> &error_pipes, std::string automaton_name, std::string state_name);
void create_ending_pipes(std::map<std::string, std::map<std::string, int[2]>> &error_pipes, std::string automaton_name, std::string state_name);
void create_initial_pipes(std::map<std::string, std::map<std::string, int[2]>> &initial_pipes, std::string automaton_name, std::string state_name);

void close_control_intern_pipes(std::map<std::string, std::vector<Edge>> &transitions, YAML::Node config);



int main(int argc, char *argv[]) {

    if (argc != 2) {
        std::cerr << "Usage: ./parser <yaml file>";
        exit(EXIT_FAILURE);
    }

    YAML::Node config = YAML::LoadFile(argv[1]);
    const int NUM_AUTOMATA = config.size();

    // process related data
    // state ids matrix
    std::vector<pid_t> state_pids[NUM_AUTOMATA];
    // transitions
    std::map<std::string, std::vector<Edge>> transitions;
    // error pipes
    std::map<std::string, std::map<std::string, int[2]>> error_pipes;
    // ending pipes
    std::map<std::string, std::map<std::string, int[2]>> ending_pipes;
    // initial pipes
    std::map<std::string, std::map<std::string, int[2]>> initial_pipes;

    // transitions creation
    for (size_t i = 0; i < NUM_AUTOMATA; i++) {
        
        std::string automaton_name = config[i]["automata"].as<std::string>();
        for (size_t j = 0; j < config[i]["delta"].size(); j++) {

            for (size_t k = 0; k < config[i]["delta"][j]["trans"].size(); k++) {
                // create the edge (transition)
                std::string origin = config[i]["delta"][j]["node"].as<std::string>();
                std::string destination = config[i]["delta"][j]["trans"][k]["next"].as<std::string>();
                std::string message = config[i]["delta"][j]["trans"][k]["in"].as<std::string>();
                Edge transition = Edge(origin, destination, message);
                // add it to the transitions map
                transitions[automaton_name].push_back(transition);
            }
        }
    }
    // error pipes creation
    for (size_t i = 0; i < NUM_AUTOMATA; i++) {    
        const int NUM_NODES = config[i]["delta"].size();
        std::string automaton_name = config[i]["automata"].as<std::string>();
    
        // Loop through the states
        for (size_t j = 0; j < NUM_NODES; j++) {
            std::string state_name = config[i]["delta"][j]["node"].as<std::string>();
            // create the pipe
            create_error_pipes(error_pipes, automaton_name, state_name);
        }
    }
    // ending pipes creation
    for (size_t i = 0; i < NUM_AUTOMATA; i++) {
        const int NUM_FINALS = config[i]["final"].size();
        std::string automaton_name = config[i]["automata"].as<std::string>();
        
        // Loop through the final states
        for (size_t j = 0; j < NUM_FINALS; j++) {
            std::string state_name = config[i]["final"][j].as<std::string>();
            // create the pipe
            create_ending_pipes(ending_pipes, automaton_name, state_name);
        }
    }
    // initial pipes creation
    for (size_t i = 0; i < NUM_AUTOMATA; i++) {
        std::string automaton_name = config[i]["automata"].as<std::string>();
        std::string state_name = config[i]["start"].as<std::string>();
        create_initial_pipes(initial_pipes, automaton_name, state_name);
    }

    // Process creation loop
    // Loop through the automata
    for (size_t i = 0; i < NUM_AUTOMATA; i++) {
        
        const int NUM_NODES = config[i]["delta"].size();
        std::string automaton_name = config[i]["automata"].as<std::string>();
    
        // Loop through the states
        for (size_t j = 0; j < NUM_NODES; j++) {
            std::string state_name = config[i]["delta"][j]["node"].as<std::string>();

            // Create the process and...
            pid_t j_state = fork();
            // The father puts it inside the pid matrix
            if (j_state > 0) {
                state_pids[i].push_back(j_state);
                close_control_intern_pipes(transitions, config);
            }
            // State logic
            else if (j_state == 0) {

                assign_intern_pipes(transitions, automaton_name, state_name, config);
                assign_error_pipes(error_pipes, automaton_name, state_name, config);
                assign_ending_pipes(error_pipes, automaton_name, state_name, config);
                assign_initial_pipes(initial_pipes, automaton_name, state_name, config);

                // Do stuff...

                exit(EXIT_SUCCESS);
            }
            // Error creating the new process
            else {
                std::cerr << "ERROR!: " << strerror(errno) << std::endl;
                exit(EXIT_FAILURE);
            }
            
        }

    }

    // Wait for processes to finish
    for (size_t i = 0; i < NUM_AUTOMATA; i++) {
        const int NUM_NODES = config[i]["delta"].size();
        int status;

        for (size_t j = 0; j < NUM_NODES; j++) {
            waitpid(state_pids[i][j], &status, 0);
        }
    }

}

void close_control_intern_pipes(std::map<std::string, std::vector<Edge>> &transitions, YAML::Node config) {
    for (size_t i = 0; i < config.size(); i++) {
        std::string current_name = config[i]["automata"].as<std::string>();
        for (size_t j = 0; j < transitions[current_name].size(); j++) {
            for (Edge &trans : transitions[current_name]) {
                close(trans.pip[0]);
                close(trans.pip[1]);
            }
        }
    }
}

/**
 * Closes unnecessary intern pipes
 */
void assign_intern_pipes(std::map<std::string, std::vector<Edge>> &transitions, std::string automaton_name, std::string state_name, YAML::Node config) {
    for (size_t i = 0; i < config.size(); i++) {
        std::string current_name = config[i]["automata"].as<std::string>();
        for (size_t j = 0; j < transitions[current_name].size(); j++) {
            for (Edge &trans : transitions[current_name]) {
                if (current_name != automaton_name) {
                    close(trans.pip[1]);
                    close(trans.pip[0]);
                }
                else {
                    if (trans.origin != state_name) {
                        close(trans.pip[1]);
                    }
                    if (trans.destination != state_name) {
                        close(trans.pip[0]);
                    }
                }
            }
        }
    }
}

/**
 * Closes unnecessary error pipes
 */
void assign_error_pipes(std::map<std::string, std::map<std::string, int[2]>> &error_pipes, std::string automaton_name, std::string state_name, YAML::Node config) {
    for (size_t i = 0; i < config.size(); i++) {
        std::string current_name = config[i]["automata"].as<std::string>();
        for (size_t j = 0; j < error_pipes[current_name].size(); j++) {
            std::string current_state = config[i]["delta"][j]["node"].as<std::string>();
            if (current_name != automaton_name) {
                close(error_pipes[current_name][current_state][0]);
                close(error_pipes[current_name][current_state][1]);
            }
            else {
                if (current_state != state_name) {
                    close(error_pipes[current_name][current_state][0]);
                    close(error_pipes[current_name][current_state][1]);
                }
                else {
                    close(error_pipes[current_name][current_state][0]);
                }
            }
        }
    }
}

/**
 * Closes unnecessary ending pipes
 */
void assign_ending_pipes(std::map<std::string, std::map<std::string, int[2]>> &ending_pipes, std::string automaton_name, std::string state_name, YAML::Node config) {
    for (size_t i = 0; i < config.size(); i++) {
        std::string current_name = config[i]["automata"].as<std::string>();
        for (size_t j = 0; j < config[i]["final"].size(); j++) {
            std::string current_state = config[i]["final"][j].as<std::string>();
            if (current_name != automaton_name) {
                close(ending_pipes[current_name][current_state][0]);
                close(ending_pipes[current_name][current_state][1]);
            }
            else {
                if (current_state != state_name) {
                    close(ending_pipes[current_name][current_state][0]);
                    close(ending_pipes[current_name][current_state][1]);
                }
                else {
                    close(ending_pipes[current_name][current_state][0]);
                }
            }
        }
    }
}

/**
 * Closes unnecessary initial pipes
 */
void assign_initial_pipes(std::map<std::string, std::map<std::string, int[2]>> &initial_pipes, std::string automaton_name, std::string state_name, YAML::Node config) {
    for (size_t i = 0; i < config.size(); i++) {
        std::string current_name = config[i]["automata"].as<std::string>();
        std::string current_initial = config[i]["start"].as<std::string>();
        if (current_name != automaton_name) {
            close(initial_pipes[current_name][current_initial][0]);
            close(initial_pipes[current_name][current_initial][1]);
        }
        else {
            if (current_initial != state_name) {
                close(initial_pipes[current_name][current_initial][0]);
                close(initial_pipes[current_name][current_initial][1]);
            }
            else {
                close(initial_pipes[current_name][current_initial][1]);
            }
        }
    }
}

void create_error_pipes(std::map<std::string, std::map<std::string, int[2]>> &error_pipes, std::string automaton_name, std::string state_name) {
    if (pipe(error_pipes[automaton_name][state_name]) == -1) {
        std::cerr << "ERROR!: pipe could not be created." << std::endl;
        exit(EXIT_FAILURE);
    }
}

void create_ending_pipes(std::map<std::string, std::map<std::string, int[2]>> &ending_pipes, std::string automaton_name, std::string state_name) {
    if (pipe(ending_pipes[automaton_name][state_name]) == -1) {
        std::cerr << "ERROR!: pipe could not be created." << std::endl;
        exit(EXIT_FAILURE);
    }
}

void create_initial_pipes(std::map<std::string, std::map<std::string, int[2]>> &initial_pipes, std::string automaton_name, std::string state_name) {
    if (pipe(initial_pipes[automaton_name][state_name]) == -1) {
        std::cerr << "ERROR!: pipe could not be created." << std::endl;
        exit(EXIT_FAILURE);
    }
}
// crear tuberias entes de los pocesos
// procesos hijos deciden cuales necesitan y cierran las que no
// 