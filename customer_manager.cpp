#include "banking.hpp"
#include "console.hpp"

#include <iostream>

int main() {
    console::formatMoney();
    try {
        banking::Bank bank;
        std::cout << "------ Customer registration ------\n";
        while (true) {
            banking::Customer customer{};
            console::readText("Full name: ", customer.name);
            customer.nationalId = console::readNumber<int>("National ID (digits only): ");
            console::readText("Username (letters, digits, or underscores): ", customer.username);
            customer.pin = console::readNumber<int>("Numeric PIN: ");
            try {
                bank.registerCustomer(customer);
                std::cout << "Customer registered. Initial balance: $" << banking::initialBalance << '\n';
                return 0;
            } catch (const std::invalid_argument& error) {
                std::cout << error.what() << " Please enter the customer details again.\n";
            }
        }
    } catch (const console::InputClosed&) {
        std::cout << "\nInput closed. Exiting.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
