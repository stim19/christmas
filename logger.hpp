#pragma once
#include <iostream>
#include <string>

/* 
 * Simple global logging utility for console
 * Use Logger::info("message") for logging
 * Logging can be toggled globally 
 *              via Logger::enable
 * Example:
 *     Logger::enabled = true;
 *     Logger::info("Task success");
 */

class Logger {
    public:
        static bool enabled;                        // flag to turn logging on/off
        static void info(const std::string& msg) {
            if(enabled)
                std::cout << "[INFO]" << msg << std::endl;
        }
        static void warn(const std::string& msg) {
            if(enabled)
                std::cerr << "[WARN]" << msg << std::endl;
        }
        static void error(const std::string& msg) {
            if(enabled)
                std::cerr << "[ERROR]" << msg << std::endl;
        }
};

// initialize logger
inline bool Logger::enabled = true;
