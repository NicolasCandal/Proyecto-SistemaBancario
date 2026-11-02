#include "banking.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace {

// Original field types and order, with English names for the compatibility fixture.
struct LegacyCustomer {
    char name[50];
    int nationalId;
    char username[50];
    int pin;
    double balance;
};

struct LegacyTransaction {
    double amount;
    int date;
    int id;
    LegacyCustomer customer;
    bool isDeposit;
};

static_assert(std::is_standard_layout<banking::Customer>::value, "Customer layout must be stable");
static_assert(std::is_standard_layout<banking::Transaction>::value, "Transaction layout must be stable");
static_assert(sizeof(LegacyCustomer) == sizeof(banking::Customer), "Customer record size changed");
static_assert(sizeof(LegacyTransaction) == sizeof(banking::Transaction), "Transaction record size changed");
static_assert(offsetof(LegacyCustomer, name) == offsetof(banking::Customer, name), "Name offset changed");
static_assert(offsetof(LegacyCustomer, nationalId) == offsetof(banking::Customer, nationalId), "ID offset changed");
static_assert(offsetof(LegacyCustomer, username) == offsetof(banking::Customer, username), "Username offset changed");
static_assert(offsetof(LegacyCustomer, pin) == offsetof(banking::Customer, pin), "PIN offset changed");
static_assert(offsetof(LegacyCustomer, balance) == offsetof(banking::Customer, balance), "Balance offset changed");
static_assert(offsetof(LegacyTransaction, amount) == offsetof(banking::Transaction, amount), "Amount offset changed");
static_assert(offsetof(LegacyTransaction, date) == offsetof(banking::Transaction, date), "Date offset changed");
static_assert(offsetof(LegacyTransaction, id) == offsetof(banking::Transaction, id), "Transaction ID offset changed");
static_assert(offsetof(LegacyTransaction, customer) == offsetof(banking::Transaction, customer), "Customer offset changed");
static_assert(offsetof(LegacyTransaction, isDeposit) == offsetof(banking::Transaction, isDeposit), "Type offset changed");

int checks = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
    ++checks;
}

template <typename Action>
void expectError(Action action, const std::string& message) {
    bool failed = false;
    try {
        action();
    } catch (const std::exception&) {
        failed = true;
    }
    check(failed, message);
}

banking::Customer makeCustomer(int id, const char* username) {
    banking::Customer customer{};
    std::strcpy(customer.name, "Test Customer");
    std::strcpy(customer.username, username);
    customer.nationalId = id;
    customer.pin = 1234;
    return customer;
}

void clearData(const std::string& directory) {
    for (const auto* name : {"customers.dat", "transactions.dat", "clientes.dat", "transacciones.dat"}) {
        std::remove((directory + "/" + name).c_str());
    }
}

template <typename Record>
void writeFixture(const std::string& path, const std::vector<Record>& records) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    for (const auto& record : records) {
        file.write(reinterpret_cast<const char*>(&record), sizeof record);
    }
    file.close();
    if (!file) {
        throw std::runtime_error("Cannot create test fixture: " + path);
    }
}

std::string readBytes(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

void testDates() {
    check(banking::isValidDate(20240229), "Leap day must be valid");
    check(banking::isValidDate(20000229), "Year 2000 must be a leap year");
    for (int invalid : {19000229, 20230229, 20241301, 20240431, 20240001, 20240100, -1, 18991231, 100000101}) {
        check(!banking::isValidDate(invalid), "Invalid calendar date accepted");
    }
    check(banking::subtractDays(20240330, 30) == 20240229, "Leap-year subtraction failed");
    check(banking::subtractDays(20230330, 30) == 20230228, "Non-leap-year subtraction failed");
    check(banking::subtractDays(20240115, 30) == 20231216, "Year boundary subtraction failed");
    check(banking::subtractDays(20240101, 0) == 20240101, "Zero-day subtraction failed");
    expectError([] { banking::subtractDays(20240230, 30); }, "Invalid reference date accepted");
    expectError([] { banking::subtractDays(20240101, -1); }, "Negative day count accepted");
}

void testRegistrationAndTransactions(const std::string& directory) {
    clearData(directory);
    banking::Bank bank(directory);
    banking::Transaction largest{};
    banking::Depositor top{};
    check(!bank.largestTransaction(largest), "Empty history has a largest transaction");
    check(!bank.topDepositor(top), "Empty history has a top depositor");
    check(bank.customerTransactions(1).empty(), "Empty history must return no transactions");
    check(bank.recentSummary(1, 20240330).depositCount == 0, "Empty summary must be zero");
    check(!bank.deleteTransaction(1), "Missing transaction must not be deleted");
    expectError([&] { bank.authenticate("missing", 1234); }, "Missing customer authenticated");

    auto first = makeCustomer(1, "alex");
    first.balance = 50;
    bank.registerCustomer(first);
    check(bank.authenticate("alex", 1234).balance == banking::initialBalance, "Initial balance changed");
    expectError([&] { bank.authenticate("alex", 9999); }, "Incorrect PIN authenticated");
    expectError([&] { bank.registerCustomer(makeCustomer(1, "other")); }, "Duplicate national ID accepted");
    expectError([&] { bank.registerCustomer(makeCustomer(2, "alex")); }, "Duplicate username accepted");
    expectError([&] { bank.registerCustomer(makeCustomer(2, "bad name")); }, "Invalid username accepted");
    expectError([&] { bank.registerCustomer(makeCustomer(0, "zero")); }, "Invalid national ID accepted");
    auto invalid = makeCustomer(2, "second");
    std::fill(std::begin(invalid.name), std::end(invalid.name), 'a');
    expectError([&] { bank.registerCustomer(invalid); }, "Unterminated name accepted");
    bank.registerCustomer(makeCustomer(2, "blair"));

    check(bank.recordTransaction(1, 100, 20240310, true).balance == 10100, "Deposit balance incorrect");
    check(bank.recordTransaction(1, 75, 20240311, false).balance == 10025, "Withdrawal balance incorrect");
    check(banking::Bank(directory).authenticate("alex", 1234).balance == 10025, "Balance was not persisted");
    const auto beforeCustomers = readBytes(directory + "/customers.dat");
    const auto beforeTransactions = readBytes(directory + "/transactions.dat");
    for (double amount : {-1.0, 0.0, std::numeric_limits<double>::infinity(),
                          std::numeric_limits<double>::quiet_NaN()}) {
        expectError([&] { bank.recordTransaction(1, amount, 20240312, true); }, "Invalid deposit accepted");
        expectError([&] { bank.recordTransaction(1, amount, 20240312, false); }, "Invalid withdrawal accepted");
    }
    expectError([&] { bank.recordTransaction(1, 20000, 20240312, false); }, "Overdraft accepted");
    expectError([&] { bank.recordTransaction(1, 10, 20240230, true); }, "Invalid transaction date accepted");
    expectError([&] { bank.recordTransaction(9, 10, 20240312, true); }, "Unknown customer accepted");
    check(readBytes(directory + "/customers.dat") == beforeCustomers, "Rejected operation changed balances");
    check(readBytes(directory + "/transactions.dat") == beforeTransactions, "Rejected operation changed history");

    // Deliberately insert dates out of order, including both boundaries and a future date.
    bank.recordTransaction(1, 30, 20240229, true);
    bank.recordTransaction(1, 900, 20240228, true);
    bank.recordTransaction(1, 70, 20240330, true);
    bank.recordTransaction(1, 5000, 20240331, true);
    bank.recordTransaction(2, 200, 20240315, true);
    const auto historyBeforeReports = readBytes(directory + "/transactions.dat");
    const auto summary = bank.recentSummary(1, 20240330);
    check(summary.depositCount == 3 && summary.depositTotal == 200, "Summary date boundaries incorrect");
    check(summary.withdrawalCount == 1 && summary.withdrawalTotal == 75, "Withdrawal summary incorrect");
    const auto transactions = bank.customerTransactions(1);
    check(transactions.size() == 6, "Customer transaction filter incorrect");
    check(std::is_sorted(transactions.begin(), transactions.end(), [](const banking::Transaction& a, const banking::Transaction& b) {
        return a.date < b.date;
    }), "Customer history is not chronological");
    check(transactions[2].customer.balance == 10000, "Pre-transaction customer snapshot changed");
    check(bank.largestTransaction(largest) && largest.amount == 5000, "Largest transaction incorrect");
    check(bank.topDepositor(top) && top.customer.nationalId == 1 && top.total == 5170,
          "Top depositor must use the latest transaction date and aggregate by customer");
    check(readBytes(directory + "/transactions.dat") == historyBeforeReports, "Reports modified history");

    // Deleting history intentionally does not reverse the balance.
    const auto balanceBeforeDelete = bank.authenticate("alex", 1234).balance;
    check(bank.deleteTransaction(3), "Existing transaction was not deleted");
    check(!bank.deleteTransaction(3), "Deleted transaction was found again");
    check(bank.authenticate("alex", 1234).balance == balanceBeforeDelete, "Deletion changed the balance");
    bank.recordTransaction(1, 1, 20240331, true);
    check(bank.customerTransactions(1).back().id == 8, "IDs must use the maximum existing ID, not file order");

    banking::Customer huge{};
    huge.balance = std::numeric_limits<double>::max();
    expectError([&] { huge.deposit(std::numeric_limits<double>::max()); }, "Balance overflow accepted");
}

void testReportingEdges(const std::string& directory) {
    clearData(directory);
    banking::Bank bank(directory);
    bank.registerCustomer(makeCustomer(1, "alex"));
    bank.registerCustomer(makeCustomer(2, "blair"));
    bank.recordTransaction(1, 10, 20240101, false);
    banking::Depositor top{};
    check(!bank.topDepositor(top), "Withdrawal-only history has a top depositor");
    bank.recordTransaction(1, 500, 20240101, true);
    bank.recordTransaction(2, 100, 20240330, true);
    bank.recordTransaction(1, 40, 20240310, true);
    bank.recordTransaction(1, 60, 20240311, true);
    check(bank.topDepositor(top) && top.customer.nationalId == 1 && top.total == 100,
          "Top depositor must exclude old deposits and break ties by national ID");
    const auto ordered = bank.customerTransactions(1);
    check(ordered[0].id == 1 && ordered[1].id == 2, "Equal-date transactions must preserve insertion order");
}

void testLegacyCompatibility(const std::string& directory) {
    clearData(directory);
    LegacyCustomer customer{};
    std::strcpy(customer.name, "Legacy Customer");
    std::strcpy(customer.username, "legacy");
    customer.nationalId = 9;
    customer.pin = 4321;
    customer.balance = 12345;
    LegacyTransaction transaction{};
    transaction.amount = 25;
    transaction.date = 20240101;
    transaction.id = 99;
    transaction.customer = customer;
    transaction.isDeposit = true;
    writeFixture(directory + "/clientes.dat", std::vector<LegacyCustomer>{customer});
    writeFixture(directory + "/transacciones.dat", std::vector<LegacyTransaction>{transaction});
    const auto originalCustomers = readBytes(directory + "/clientes.dat");
    const auto originalTransactions = readBytes(directory + "/transacciones.dat");
    banking::Bank bank(directory);
    check(bank.authenticate("legacy", 4321).balance == 12345, "Legacy customer cannot be read");
    check(bank.customerTransactions(9).front().id == 99, "Legacy transaction cannot be read");
    check(readBytes(directory + "/customers.dat") == originalCustomers, "Migration changed customer bytes");
    check(readBytes(directory + "/transactions.dat") == originalTransactions, "Migration changed transaction bytes");
    check(!std::ifstream(directory + "/clientes.dat"), "Legacy customer file was not renamed");
    check(!std::ifstream(directory + "/transacciones.dat"), "Legacy transaction file was not renamed");
    bank.recordTransaction(9, 5, 20240102, true);
    check(bank.customerTransactions(9).back().id == 100, "Legacy IDs were not preserved");
    writeFixture(directory + "/clientes.dat", std::vector<LegacyCustomer>{customer});
    expectError([&] { banking::Bank conflicting(directory); }, "Conflicting legacy files were silently ignored");
}

void testDamagedFiles(const std::string& directory) {
    clearData(directory);
    banking::Bank bank(directory);
    bank.registerCustomer(makeCustomer(1, "alex"));
    {
        std::ofstream file(directory + "/transactions.dat", std::ios::binary);
        file.put('x');
    }
    const auto customersBefore = readBytes(directory + "/customers.dat");
    expectError([&] { bank.recordTransaction(1, 1, 20240101, true); }, "Truncated history accepted");
    check(readBytes(directory + "/customers.dat") == customersBefore, "Damaged history caused a balance change");
    check(readBytes(directory + "/transactions.dat") == "x", "Damaged history was overwritten");

    auto invalid = makeCustomer(1, "alex");
    std::fill(std::begin(invalid.username), std::end(invalid.username), 'x');
    writeFixture(directory + "/customers.dat", std::vector<banking::Customer>{invalid});
    expectError([&] { bank.authenticate("alex", 1234); }, "Unterminated stored username accepted");
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: banking_tests <empty test directory>\n";
        return 1;
    }
    const std::string directory = argv[1];
    try {
        testDates();
        testRegistrationAndTransactions(directory);
        testReportingEdges(directory);
        testLegacyCompatibility(directory);
        testDamagedFiles(directory);
        clearData(directory);
        std::cout << checks << " banking checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failed: " << error.what() << '\n';
        return 1;
    }
}
