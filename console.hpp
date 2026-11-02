#ifndef CONSOLE_HPP
#define CONSOLE_HPP

#include "banking.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>

namespace console {

class InputClosed : public std::runtime_error {
public:
    InputClosed() : std::runtime_error("Input closed.") {}
};

inline std::string readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string value;
    if (!std::getline(std::cin, value)) {
        throw InputClosed();
    }
    return value;
}

template <typename Value>
Value readNumber(const std::string& prompt) {
    while (true) {
        std::istringstream input(readLine(prompt));
        Value value{};
        if (input >> value) {
            input >> std::ws;
            if (input.eof()) {
                return value;
            }
        }
        std::cout << "Enter a valid number.\n";
    }
}

inline void readText(const std::string& prompt, char (&destination)[banking::textCapacity]) {
    while (true) {
        const auto value = readLine(prompt);
        if (!value.empty() && value.size() < banking::textCapacity) {
            std::fill(std::begin(destination), std::end(destination), '\0');
            std::copy(value.begin(), value.end(), destination);
            return;
        }
        std::cout << "Enter between 1 and 49 bytes of text.\n";
    }
}

inline int readDate(const std::string& prompt) {
    while (true) {
        const int date = readNumber<int>(prompt);
        if (banking::isValidDate(date)) {
            return date;
        }
        std::cout << "Enter a valid date in YYYYMMDD format (1900-9999).\n";
    }
}

inline void printTransaction(const banking::Transaction& transaction) {
    std::cout << "--------------------------\n"
              << "ID: " << transaction.id << '\n'
              << "Date: " << transaction.date << '\n'
              << "Type: " << transaction.typeName() << '\n'
              << "Amount: $" << transaction.amount << '\n'
              << "--------------------------\n";
}

inline void formatMoney() {
    std::cout << std::fixed << std::setprecision(2);
}

} // namespace console

#endif
