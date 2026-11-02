#include "banking.hpp"
#include "console.hpp"

#include <algorithm>
#include <iostream>

namespace {

void listTransactions(const banking::Bank& bank, int nationalId) {
    const auto transactions = bank.customerTransactions(nationalId);
    if (transactions.empty()) {
        std::cout << "No transactions found for this customer.\n";
        return;
    }
    constexpr std::size_t pageSize = 5;
    const auto pageCount = (transactions.size() + pageSize - 1) / pageSize;
    std::size_t page = 0;
    while (true) {
        std::cout << "\nTransactions for national ID " << nationalId
                  << " | Page " << page + 1 << " of " << pageCount << '\n';
        const auto end = std::min((page + 1) * pageSize, transactions.size());
        for (auto index = page * pageSize; index < end; ++index) {
            console::printTransaction(transactions[index]);
        }
        const auto command = console::readLine("N - Next page / P - Previous page / E - Exit: ");
        if (command == "E" || command == "e") {
            return;
        }
        if ((command == "N" || command == "n") && page + 1 < pageCount) {
            ++page;
        } else if ((command == "P" || command == "p") && page > 0) {
            --page;
        } else {
            std::cout << "That navigation option is not available.\n";
        }
    }
}

void printSummary(const banking::Bank& bank, int nationalId) {
    const int referenceDate = console::readDate("Today's date (YYYYMMDD): ");
    const auto summary = bank.recentSummary(nationalId, referenceDate);
    std::cout << "Transaction summary for national ID " << nationalId << '\n'
              << "Deposits: " << summary.depositCount << " | Total: $" << summary.depositTotal << '\n'
              << "Withdrawals: " << summary.withdrawalCount << " | Total: $" << summary.withdrawalTotal << '\n';
}

} // namespace

int main() {
    console::formatMoney();
    try {
        banking::Bank bank;
        std::cout << "------ Transaction analyst ------\n";
        while (true) {
            std::cout << "\n1. List a customer's transactions\n"
                      << "2. Summarize a customer's transactions over the last 30 days\n"
                      << "3. Show the largest transaction\n"
                      << "4. Show the customer with the highest total deposits over the last 30 days\n"
                      << "5. Exit\n";
            switch (console::readNumber<int>("Choose an option: ")) {
            case 1:
                listTransactions(bank, console::readNumber<int>("Customer national ID: "));
                break;
            case 2:
                printSummary(bank, console::readNumber<int>("Customer national ID: "));
                break;
            case 3: {
                banking::Transaction transaction{};
                if (bank.largestTransaction(transaction)) {
                    console::printTransaction(transaction);
                    std::cout << "Username: " << transaction.customer.username << '\n';
                } else {
                    std::cout << "No transactions found.\n";
                }
                break;
            }
            case 4: {
                banking::Depositor depositor{};
                if (bank.topDepositor(depositor)) {
                    std::cout << "Customer with the highest total deposits: " << depositor.customer.username
                              << "\nTotal deposits: $" << depositor.total << '\n';
                } else {
                    std::cout << "No deposits found in the reporting period.\n";
                }
                break;
            }
            case 5:
                std::cout << "Exiting.\n";
                return 0;
            default:
                std::cout << "Choose an option from 1 to 5.\n";
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
