#include <iostream>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <map>
#include <vector>
#include <thread>
#include <semaphore.h>

#include <cstdio>

#include <yaml-cpp/yaml.h>
#include "edge.h"

// STATES PIPE MANAGEMENT
void assign_intern_pipes(std::map<std::string, std::vector<Edge>> &transitions, std::string automaton_name, std::string state_name, YAML::Node config);
void assign_error_pipes(std::map<std::string, std::map<std::string, int[2]>> &error_pipes, std::string automaton_name, std::string state_name, YAML::Node config);
void assign_ending_pipes(std::map<std::string, std::map<std::string, int[2]>> &ending_pipes, std::string automaton_name, std::string state_name, YAML::Node config);
void assign_initial_pipes(std::map<std::string, std::map<std::string, int[2]>> &initial_pipes, std::string automaton_name, std::string state_name, YAML::Node config);
void create_error_pipes(std::map<std::string, std::map<std::string, int[2]>> &error_pipes, std::string automaton_name, std::string state_name);
void create_ending_pipes(std::map<std::string, std::map<std::string, int[2]>> &error_pipes, std::string automaton_name, std::string state_name);
void create_initial_pipes(std::map<std::string, std::map<std::string, int[2]>> &initial_pipes, std::string automaton_name, std::string state_name);
// STATES LOGIC
void state_logic(std::map<std::string, std::map<std::string, int[2]>> &initial_pipes, std::map<std::string, std::map<std::string, int[2]>> &error_pipes, std::map<std::string, std::map<std::string, int[2]>> &ending_pipes, std::map<std::string, std::vector<Edge>> &transitions, std::string automaton_name, std::string state_name, std::vector<std::string> current_final_states, bool is_initial);
void initial_state_logic(std::map<std::string, std::map<std::string, int[2]>> &initial_pipes, std::map<std::string, std::map<std::string, int[2]>> &error_pipes, std::map<std::string, std::map<std::string, int[2]>> &ending_pipes, std::map<std::string, std::vector<Edge>> &transitions, std::string automaton_name, std::string state_name, std::vector<std::string> current_final_states, sem_t *mutex);
void intermediate_state_logic(std::map<std::string, std::vector<Edge>> &transitions, std::map<std::string, std::map<std::string, int[2]>> &error_pipes, std::map<std::string, std::map<std::string, int[2]>> &ending_pipes, std::string automaton_name, std::string state_name, std::vector<std::string> current_final_states, Edge &trans, sem_t *mutex);
bool analyze_prefix(std::string trans_msg, std::string buff_msg);
void propagate_message(std::string &buffer, std::map<std::string, std::map<std::string, int[2]>> &error_pipes, std::map<std::string, std::map<std::string, int[2]>> &ending_pipes, std::map<std::string, std::vector<Edge>> &transitions, std::string automaton_name, std::string state_name, std::vector<std::string> current_final_states, sem_t *mutex);

// SISCTRL PIPE MANAGEMENT
void close_sisctrl_intern_pipes(std::map<std::string, std::vector<Edge>> &transitions, YAML::Node config);
void close_sisctrl_error_pipes(std::map<std::string, std::map<std::string, int[2]>> &error_pipes, YAML::Node config);
void close_sisctrl_ending_pipes(std::map<std::string, std::map<std::string, int[2]>> &ending_pipes, YAML::Node config);
void close_sisctrl_initial_pipes(std::map<std::string, std::map<std::string, int[2]>> &initial_pipes, YAML::Node config);
// SISCTRL LOGIC
void get_user_input(std::string &input_message);
void sisctrl_listen_end_pipes(const int NUM_AUTOMATA, YAML::Node config, std::map<std::string, std::map<std::string, int[2]>> &ending_pipes, sem_t *mutex);
void sisctrl_listen_error_pipes(const int NUM_AUTOMATA, YAML::Node config, std::map<std::string, std::map<std::string, int[2]>> &error_pipes, sem_t *mutex);


int main(int argc, char *argv[]) {

    if (argc != 2) {
        std::cerr << "Usage: ./parser <yaml file>";
        exit(EXIT_FAILURE);
    }

    // Get input from user
    std::string input_message;
    get_user_input(input_message);
    YAML::Node input_cmd = YAML::Load(input_message);

    std::string command = input_cmd["cmd"].as<std::string>();
    std::string msg = input_cmd["msg"].as<std::string>();
    printf("Command %s, msg %s\n", command.c_str(), msg.c_str());
    //--------------------

    YAML::Node config = YAML::LoadFile(argv[1]);
    const int NUM_AUTOMATA = config.size();

    // process related data
    // state ids matrix
    std::vector<pid_t> state_pids;
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
        std::string current_start_state = config[i]["start"].as<std::string>();
        std::vector<std::string> current_final_states;
        for (size_t p = 0; p < config[i]["final"].size(); p++) {
            std::string final_state = config[i]["final"][p].as<std::string>();
            current_final_states.push_back(final_state);
        }
    
        // Loop through the states
        for (size_t j = 0; j < NUM_NODES; j++) {
            std::string state_name = config[i]["delta"][j]["node"].as<std::string>();

            // CREATE THE PROCESS and...
            pid_t j_state = fork();
            // The father puts it inside the pid vector
            // and closes his unnecessary pipes
            if (j_state > 0) {
                state_pids.push_back(j_state);
                std::cout << "creating process " << j_state << std::endl;
            }
            // State logic
            else if (j_state == 0) {

                assign_intern_pipes(transitions, automaton_name, state_name, config);
                assign_error_pipes(error_pipes, automaton_name, state_name, config);
                assign_ending_pipes(error_pipes, automaton_name, state_name, config);
                assign_initial_pipes(initial_pipes, automaton_name, state_name, config);

                // start checking the qualities of the state
                if (current_start_state == state_name) {
                    // This is an initial state
                    while (true) {
                        state_logic(initial_pipes, error_pipes, ending_pipes, transitions, automaton_name, state_name, current_final_states, true);
                    }
                }
                else {
                    while (true) {
                        state_logic(initial_pipes, error_pipes, ending_pipes, transitions, automaton_name, state_name, current_final_states, false);
                    }
                }

                // END
                exit(EXIT_SUCCESS);
            }
            // Error creating the new process
            else {
                std::cerr << "ERROR!: " << strerror(errno) << std::endl;
                exit(EXIT_FAILURE);
            }
            
        }
    }

    // FATHER LOGIC
    
    // Close unnecessary pipes
    close_sisctrl_intern_pipes(transitions, config);
    close_sisctrl_error_pipes(error_pipes, config);
    close_sisctrl_ending_pipes(ending_pipes, config);
    close_sisctrl_initial_pipes(initial_pipes, config);

    // Write to the initial states
    for (size_t i = 0; i < NUM_AUTOMATA; i++) {
        
        const int NUM_NODES = config[i]["delta"].size();
        std::string automaton_name = config[i]["automata"].as<std::string>();
        std::string current_start_state = config[i]["start"].as<std::string>();

        const char *message = input_message.c_str();
        size_t n = strlen(message);

        // Create the Yaml command
        YAML::Emitter out;
        out << YAML::Flow;
        out << YAML::BeginMap;
        out << YAML::Key << "recog";
        out << YAML::Value << "";
        out << YAML::Key << "rest";
        out << YAML::Value << msg;
        out << YAML::EndMap;
        
        const char *msg_to_send = out.c_str();
        if (write(initial_pipes[automaton_name][current_start_state][1], msg_to_send, strlen(msg_to_send)) < 0) {
            std::cerr << "ERROR!: could not wrtie initial message" << std::endl;
        }
    }

    // Yaml out
    YAML::Emitter out;
    sem_t mutex;
    sem_init(&mutex, 0, 1);

    // Listen to the ending pipes
    std::thread end_pipes_thread (sisctrl_listen_end_pipes, NUM_AUTOMATA, config, std::ref(ending_pipes), &mutex);

    // Listen to the error pipes
    std::thread error_pipes_thread (sisctrl_listen_error_pipes, NUM_AUTOMATA, config, std::ref(error_pipes), &mutex);

    end_pipes_thread.join();
    error_pipes_thread.join();

    for (int pid : state_pids) {
        int status;
        waitpid(pid, &status, 0);
    }
}

void sisctrl_listen_end_pipes(const int NUM_AUTOMATA, YAML::Node config, std::map<std::string, std::map<std::string, int[2]>> &ending_pipes, sem_t *mutex) {
    while (true) {
        for (size_t i = 0; i < NUM_AUTOMATA; i++) {
        
            const int NUM_NODES = config[i]["delta"].size();
            std::string automaton_name = config[i]["automata"].as<std::string>();
            
            for (size_t j = 0; j < config[i]["final"].size(); j++) {

                std::string current_final = config[i]["final"][j].as<std::string>();

                char c;
                std::string buffer = "";
                while (read(ending_pipes[automaton_name][current_final][0], &c, 1) > 0) {
                    buffer += c;
                }
                if (c == '}') {
                    // Parse buffer
                    YAML::Node buff_msg = YAML::Load(buffer);
                    YAML::Emitter out;

                    // Create acceptance entry
                    out << YAML::BeginSeq;
                    out << YAML::BeginMap;
                    out << YAML::Key << "msgtype";
                    out << YAML::Value << "accept";
                    out << YAML::Key << "accept";
                    out << YAML::Value << YAML::BeginSeq;
                    out << YAML::BeginMap;
                    out << YAML::Key << "automata";
                    out << YAML::Value << automaton_name;
                    out << YAML::Key << "msg";
                    out << YAML::Value << buff_msg["recog"];
                    out << YAML::EndMap << YAML::EndSeq << YAML::EndMap << YAML::EndSeq;

                    sem_wait(mutex);
                    printf("%s\n", out.c_str());
                    sem_post(mutex);
                }
                c = 0;
            }
        }
    }
}

void sisctrl_listen_error_pipes(const int NUM_AUTOMATA, YAML::Node config, std::map<std::string, std::map<std::string, int[2]>> &error_pipes, sem_t *mutex) {
    while (true) {
        for (size_t i = 0; i < NUM_AUTOMATA; i++) {
        
            const int NUM_NODES = config[i]["delta"].size();
            std::string automaton_name = config[i]["automata"].as<std::string>();
            
            for (size_t j = 0; j < config[i]["delta"].size(); j++) {

                std::string current_state = config[i]["delta"][j]["node"].as<std::string>();

                char c;
                std::string buffer = "";
                while (read(error_pipes[automaton_name][current_state][0], &c, 1) > 0) {
                    buffer += c;
                }
                if (c == '}') {
                    // Parse buffer
                    YAML::Node buff_msg = YAML::Load(buffer);
                    YAML::Emitter out;

                    std::string original_msg = buff_msg["recog"].as<std::string>() + buff_msg["rest"].as<std::string>();

                    // Create acceptance entry
                    out << YAML::BeginSeq;
                    out << YAML::BeginMap;
                    out << YAML::Key << "msgtype";
                    out << YAML::Value << "reject";
                    out << YAML::Key << "reject";
                    out << YAML::Value << YAML::BeginSeq;
                    out << YAML::BeginMap;
                    out << YAML::Key << "automata";
                    out << YAML::Value << automaton_name;
                    out << YAML::Key << "msg";
                    out << YAML::Value << original_msg;
                    out << YAML::Key << "pos";
                    out << YAML::Value << buff_msg["recog"].as<std::string>().length() - 1;
                    out << YAML::EndMap << YAML::EndSeq << YAML::EndMap << YAML::EndSeq;

                    sem_wait(mutex);
                    printf("%s\n", out.c_str());
                    sem_post(mutex);
                }
                c = 0;
            }
        }
    }
}

bool analyze_prefix(std::string trans_msg, std::string buff_msg) {
    if (trans_msg.length() > buff_msg.length()) {
        return false;
    }
    else {
        std::string prefix = buff_msg.substr(0, trans_msg.length());
        if (prefix == trans_msg) {
            return true;
        }
        else {
            return false;
        }
    }
}

void propagate_message(std::string &buffer,  
                       std::map<std::string, std::map<std::string, int[2]>> &error_pipes,
                       std::map<std::string, std::map<std::string, int[2]>> &ending_pipes,
                       std::map<std::string, std::vector<Edge>> &transitions,
                       std::string automaton_name,
                       std::string state_name,
                       std::vector<std::string> current_final_states, 
                       sem_t *mutex) {
    
    YAML::Node buffer_msg = YAML::Load(buffer);
    std::string recog = buffer_msg["recog"].as<std::string>();
    std::string rest = buffer_msg["rest"].as<std::string>();

    printf("Automaton %s, state %s (initial), got: %s \n", automaton_name.c_str(), state_name.c_str(), buffer.c_str());
    // std::cout << "Automaton " << automaton_name << " state " << state_name << "(initial) got: " << buffer << std::endl;
    // Send the message to the other states
    bool msg_sent = false;
    for (Edge &trans : transitions[automaton_name]) {
        if (trans.origin == state_name) {
            if (analyze_prefix(trans.message, rest) == true) {
                // read the rest prefix into recog
                std::string new_recog = recog + rest.substr(0, trans.message.length());
                std::string new_rest = rest.substr(trans.message.length(), rest.length());
                // generate Yaml
                YAML::Emitter out;
                out << YAML::Flow;
                out << YAML::BeginMap;
                out << YAML::Key << "recog";
                out << YAML::Value << new_recog;
                out << YAML::Key << "rest";
                out << YAML::Value << new_rest;
                out << YAML::EndMap;
                const char *msg_to_send = out.c_str();
                // Lock the mutex
                sem_wait(mutex);
                write(trans.pip[1], msg_to_send, strlen(msg_to_send));
                sem_post(mutex);
                // Unlock
                msg_sent = true;
                break;
            }
        }
    }

    bool am_i_final = false;
    for (std::string final_state : current_final_states) {
        if (final_state == state_name) {
            am_i_final = true;
        }
    }

    if (am_i_final && rest == "") {

        // generate Yaml
        YAML::Emitter out;
        out << YAML::Flow;
        out << YAML::BeginMap;
        out << YAML::Key << "codterm";
        out << YAML::Value << 0;
        out << YAML::Key << "recog";
        out << YAML::Value << recog;
        out << YAML::Key << "rest";
        out << YAML::Value << rest;
        out << YAML::EndMap;
        const char *msg_to_send = out.c_str();
        // Lock the mutex
        sem_wait(mutex);
        write(ending_pipes[automaton_name][state_name][1], msg_to_send, strlen(msg_to_send));
        sem_post(mutex);
        // unlock
        msg_sent = true;
    }

    if (!msg_sent) {
        YAML::Emitter out;
        out << YAML::Flow;
        out << YAML::BeginMap;
        out << YAML::Key << "codterm";
        out << YAML::Value << 1;
        out << YAML::Key << "recog";
        out << YAML::Value << recog;
        out << YAML::Key << "rest";
        out << YAML::Value << rest;
        out << YAML::EndMap;
        const char *msg_to_send = out.c_str();
        // Lock mutex
        sem_wait(mutex);
        if (write(error_pipes[automaton_name][state_name][1], msg_to_send, strlen(msg_to_send)) < 0) {
            std::cerr << "ERROR!: could not write to error pipe" << std::endl;
        }
        sem_post(mutex);
        // unlock
    }
}

void state_logic(std::map<std::string, std::map<std::string, int[2]>> &initial_pipes, 
                 std::map<std::string, std::map<std::string, int[2]>> &error_pipes,
                 std::map<std::string, std::map<std::string, int[2]>> &ending_pipes,
                 std::map<std::string, std::vector<Edge>> &transitions,
                 std::string automaton_name,
                 std::string state_name,
                 std::vector<std::string> current_final_states, 
                 bool is_initial) {

    std::vector<std::thread> state_threads;
    sem_t mutex;
    sem_init(&mutex, 0, 1);

    // Init state logic
    if (is_initial) {
        // create the init thread
        std::thread init_thread(initial_state_logic, std::ref(initial_pipes), std::ref(error_pipes), std::ref(ending_pipes), std::ref(transitions), automaton_name, state_name, current_final_states, &mutex);
        state_threads.push_back(std::move(init_thread));
    }

    // Intermediate state logic
    for (Edge &trans : transitions[automaton_name]) {
        if (trans.destination == state_name) {
            std::thread inter_thread(intermediate_state_logic, std::ref(transitions), std::ref(ending_pipes), std::ref(error_pipes), automaton_name, state_name, current_final_states, std::ref(trans), &mutex);
            state_threads.push_back(std::move(inter_thread));
        }
    }

    for (std::thread &t : state_threads) {
        t.join();
    }

}

void initial_state_logic(std::map<std::string, std::map<std::string, int[2]>> &initial_pipes, 
                         std::map<std::string, std::map<std::string, int[2]>> &error_pipes,
                         std::map<std::string, std::map<std::string, int[2]>> &ending_pipes,
                         std::map<std::string, std::vector<Edge>> &transitions,
                         std::string automaton_name,
                         std::string state_name,
                         std::vector<std::string> current_final_states,
                         sem_t *mutex) {

    char c;
    std::string buffer = "";
    while (read(initial_pipes[automaton_name][state_name][0], &c, 1) > 0) {
        // std::cout << "State " << state_name << " from automaton " << automaton_name << " got: " << c << std::endl;
        buffer += c;
    }
    if (c == '}') {
        propagate_message(buffer, error_pipes, ending_pipes, transitions, automaton_name, state_name, current_final_states, mutex);
        c = 0;
        buffer = "";
    }

}

void intermediate_state_logic(std::map<std::string, std::vector<Edge>> &transitions, 
                              std::map<std::string, std::map<std::string, int[2]>> &ending_pipes,
                              std::map<std::string, std::map<std::string, int[2]>> &error_pipes,
                              std::string automaton_name,
                              std::string state_name, 
                              std::vector<std::string> current_final_states, 
                              Edge &trans, 
                              sem_t *mutex) {

    char c;
    std::string buffer = "";
    while (read(trans.pip[0], &c, 1) > 0) {
        buffer += c;
    }
    if (c == '}') {
        propagate_message(buffer, error_pipes, ending_pipes, transitions, automaton_name, state_name, current_final_states, mutex);
        // flush the buffer
        c = 0;
        buffer = "";
    }
}


void get_user_input(std::string &input_message) {
    std::cout << "Input a character string to be parsed: ";
    std::getline(std::cin, input_message);
}


void close_sisctrl_intern_pipes(std::map<std::string, std::vector<Edge>> &transitions, YAML::Node config) {
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

void close_sisctrl_error_pipes(std::map<std::string, std::map<std::string, int[2]>> &error_pipes, YAML::Node config) {
    for (size_t i = 0; i < config.size(); i++) {
        std::string current_name = config[i]["automata"].as<std::string>();
        for (size_t j = 0; j < error_pipes[current_name].size(); j++) {
            std::string current_state = config[i]["delta"][j]["node"].as<std::string>();
            close(error_pipes[current_name][current_state][1]);
        }
    }
}

void close_sisctrl_ending_pipes(std::map<std::string, std::map<std::string, int[2]>> &ending_pipes, YAML::Node config) {
    for (size_t i = 0; i < config.size(); i++) {
        std::string current_name = config[i]["automata"].as<std::string>();
        for (size_t j = 0; j < ending_pipes[current_name].size(); j++) {
            std::string current_state = config[i]["delta"][j]["node"].as<std::string>();
            close(ending_pipes[current_name][current_state][1]);
        }
    }
}

void close_sisctrl_initial_pipes(std::map<std::string, std::map<std::string, int[2]>> &initial_pipes, YAML::Node config) {
    for (size_t i = 0; i < config.size(); i++) {
        std::string current_name = config[i]["automata"].as<std::string>();
        for (size_t j = 0; j < initial_pipes[current_name].size(); j++) {
            std::string current_state = config[i]["delta"][j]["node"].as<std::string>();
            close(initial_pipes[current_name][current_state][0]);
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
    if (fcntl(error_pipes[automaton_name][state_name][0], F_SETFL, O_NONBLOCK) < 0) {
        std::cerr << "ERROR!: pipe could not be set to non-blocking" << std::endl;
        exit(EXIT_FAILURE);
    }
    if (fcntl(error_pipes[automaton_name][state_name][1], F_SETFL, O_NONBLOCK) < 0) {
        std::cerr << "ERROR!: pipe could not be set to non-blocking" << std::endl;
        exit(EXIT_FAILURE);
    }
}

void create_ending_pipes(std::map<std::string, std::map<std::string, int[2]>> &ending_pipes, std::string automaton_name, std::string state_name) {
    if (pipe(ending_pipes[automaton_name][state_name]) == -1) {
        std::cerr << "ERROR!: pipe could not be created." << std::endl;
        exit(EXIT_FAILURE);
    }
    if (fcntl(ending_pipes[automaton_name][state_name][0], F_SETFL, O_NONBLOCK) < 0) {
        std::cerr << "ERROR!: pipe could not be set to non-blocking" << std::endl;
        exit(EXIT_FAILURE);
    }
    if (fcntl(ending_pipes[automaton_name][state_name][1], F_SETFL, O_NONBLOCK) < 0) {
        std::cerr << "ERROR!: pipe could not be set to non-blocking" << std::endl;
        exit(EXIT_FAILURE);
    }
}

void create_initial_pipes(std::map<std::string, std::map<std::string, int[2]>> &initial_pipes, std::string automaton_name, std::string state_name) {
    if (pipe(initial_pipes[automaton_name][state_name]) == -1) {
        std::cerr << "ERROR!: pipe could not be created." << std::endl;
        exit(EXIT_FAILURE);
    }
    if (fcntl(initial_pipes[automaton_name][state_name][0], F_SETFL, O_NONBLOCK) < 0) {
        std::cerr << "ERROR!: pipe could not be set to non-blocking" << std::endl;
        exit(EXIT_FAILURE);
    }
    if (fcntl(initial_pipes[automaton_name][state_name][1], F_SETFL, O_NONBLOCK) < 0) {
        std::cerr << "ERROR!: pipe could not be set to non-blocking" << std::endl;
        exit(EXIT_FAILURE);
    }
}


