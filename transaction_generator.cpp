#include "banking.hpp"
#include "console.hpp"

#include <iostream>

namespace {

void createTransactions(banking::Bank& bank) {
    banking::Customer customer{};
    while (true) {
        const auto username = console::readLine("Username (leave blank to return): ");
        if (username.empty()) {
            return;
        }
        const int pin = console::readNumber<int>("PIN: ");
        try {
            customer = bank.authenticate(username, pin);
            break;
        } catch (const std::invalid_argument& error) {
            std::cout << error.what() << '\n';
        }
    }
    std::cout << "Welcome, " << customer.name << ". Current balance: $" << customer.balance << '\n';
    while (true) {
        const int type = console::readNumber<int>("Enter 1 to deposit or 0 to withdraw: ");
        if (type != 0 && type != 1) {
            std::cout << "Choose 1 or 0.\n";
            continue;
        }
        const double amount = console::readNumber<double>("Amount: ");
        const int date = console::readDate("Transaction date (YYYYMMDD): ");
        try {
            customer = bank.recordTransaction(customer.nationalId, amount, date, type == 1);
            std::cout << "Transaction saved. New balance: $" << customer.balance << '\n';
        } catch (const std::invalid_argument& error) {
            std::cout << error.what() << '\n';
            continue;
        }
        if (console::readNumber<int>("Enter 1 for another transaction, or any other number to return: ") != 1) {
            return;
        }
    }
}

} // namespace

int main() {
    console::formatMoney();
    try {
        banking::Bank bank;
        std::cout << "------ Transaction generator ------\n";
        while (true) {
            std::cout << "\n1. Make a transaction\n2. Delete a transaction\n3. Exit\n";
            switch (console::readNumber<int>("Choose an option: ")) {
            case 1:
                createTransactions(bank);
                break;
            case 2: {
                const int id = console::readNumber<int>("Transaction ID to delete: ");
                std::cout << (bank.deleteTransaction(id)
                    ? "Transaction deleted. The account balance is unchanged.\n"
                    : "Transaction not found.\n");
                break;
            }
            case 3:
                std::cout << "Exiting.\n";
                return 0;
            default:
                std::cout << "Choose an option from 1 to 3.\n";
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
