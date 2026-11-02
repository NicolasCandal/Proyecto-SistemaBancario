#include "banking.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <type_traits>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace banking {
namespace {

using File = std::unique_ptr<std::FILE, decltype(&std::fclose)>;

std::runtime_error fileError(const std::string& action, const std::string& path) {
    return std::runtime_error(action + ": " + path);
}

std::string checkedText(const char (&value)[textCapacity]) {
    const auto end = std::find(value, value + textCapacity, '\0');
    if (end == value + textCapacity) {
        throw std::runtime_error("Invalid text field in a customer record.");
    }
    return std::string(value, end);
}

void validateRecord(const Customer& customer) {
    checkedText(customer.name);
    checkedText(customer.username);
    if (!std::isfinite(customer.balance)) {
        throw std::runtime_error("Invalid balance in a customer record.");
    }
}

void validateRecord(const Transaction& transaction) {
    validateRecord(transaction.customer);
    if (!std::isfinite(transaction.amount)) {
        throw std::runtime_error("Invalid amount in a transaction record.");
    }
}

bool fileExists(const std::string& path) {
    File file(std::fopen(path.c_str(), "rb"), &std::fclose);
    if (file) {
        return true;
    }
    if (errno != ENOENT) {
        throw fileError("Cannot open file", path);
    }
    return false;
}

void replaceFile(const std::string& source, const std::string& destination) {
#ifdef _WIN32
    if (!MoveFileExA(source.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING)) {
#else
    if (std::rename(source.c_str(), destination.c_str()) != 0) {
#endif
        throw fileError("Cannot replace file", destination);
    }
}

std::string dataPath(const std::string& directory, const char* name, const char* legacyName) {
    const std::string path = directory + "/" + name;
    const std::string legacyPath = directory + "/" + legacyName;
    const bool hasCurrent = fileExists(path);
    const bool hasLegacy = fileExists(legacyPath);
    if (hasCurrent && hasLegacy) {
        throw std::runtime_error("Both current and legacy data files exist: " + path
                                 + ". Keep only the dataset you intend to use.");
    }
    if (hasLegacy) {
        if (std::rename(legacyPath.c_str(), path.c_str()) != 0) {
            throw fileError("Cannot rename legacy data file", legacyPath);
        }
    }
    return path;
}

template <typename Record>
std::vector<Record> readRecords(const std::string& path) {
    static_assert(std::is_trivially_copyable<Record>::value, "Records must be binary-safe");
    File file(std::fopen(path.c_str(), "rb"), &std::fclose);
    if (!file) {
        if (errno == ENOENT) {
            return {};
        }
        throw fileError("Cannot open file for reading", path);
    }

    std::vector<Record> records;
    Record record{};
    while (true) {
        const auto bytes = std::fread(&record, 1, sizeof record, file.get());
        if (std::ferror(file.get())) {
            throw fileError("Cannot read file", path);
        }
        if (bytes == 0) {
            break;
        }
        if (bytes != sizeof record) {
            throw fileError("Incomplete binary record", path);
        }
        validateRecord(record);
        records.push_back(record);
    }
    return records;
}

template <typename Record>
void writeRecords(const std::string& path, const std::vector<Record>& records) {
    const std::string temporaryPath = path + ".tmp";
    bool openedTemporaryFile = false;
    try {
        File file(std::fopen(temporaryPath.c_str(), "wb"), &std::fclose);
        if (!file) {
            throw fileError("Cannot open temporary file for writing", temporaryPath);
        }
        openedTemporaryFile = true;
        for (const auto& record : records) {
            if (std::fwrite(&record, sizeof record, 1, file.get()) != 1) {
                throw fileError("Cannot write file", temporaryPath);
            }
        }
        if (std::fflush(file.get()) != 0) {
            throw fileError("Cannot flush file", temporaryPath);
        }
        if (std::fclose(file.release()) != 0) {
            throw fileError("Cannot close file", temporaryPath);
        }
        replaceFile(temporaryPath, path);
    } catch (...) {
        if (openedTemporaryFile) {
            std::remove(temporaryPath.c_str());
        }
        throw;
    }
}

void validateAmount(double amount) {
    if (!std::isfinite(amount) || amount <= 0) {
        throw std::invalid_argument("The amount must be a finite number greater than zero.");
    }
}

int daysInMonth(int year, int month) {
    const int lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leapYear = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    return lengths[month - 1] + (month == 2 && leapYear ? 1 : 0);
}

} // namespace

void Customer::deposit(double amount) {
    validateAmount(amount);
    if (!std::isfinite(balance) || !std::isfinite(balance + amount)) {
        throw std::invalid_argument("The resulting balance is outside the supported range.");
    }
    balance += amount;
}

void Customer::withdraw(double amount) {
    validateAmount(amount);
    if (!std::isfinite(balance) || amount > balance) {
        throw std::invalid_argument("Insufficient funds.");
    }
    balance -= amount;
}

bool Transaction::belongsTo(int nationalId) const {
    return customer.nationalId == nationalId;
}

bool Transaction::isWithin(int firstDate, int lastDate) const {
    return firstDate <= date && date <= lastDate;
}

const char* Transaction::typeName() const {
    return isDeposit ? "Deposit" : "Withdrawal";
}

bool isValidDate(int date) {
    const int year = date / 10000;
    const int month = date / 100 % 100;
    const int day = date % 100;
    return year >= 1900 && year <= 9999 && month >= 1 && month <= 12
        && day >= 1 && day <= daysInMonth(year, month);
}

int subtractDays(int date, int days) {
    if (!isValidDate(date) || days < 0) {
        throw std::invalid_argument("Enter a valid date in YYYYMMDD format (1900-9999).");
    }
    int year = date / 10000;
    int month = date / 100 % 100;
    int day = date % 100;
    while (days > 0) {
        if (days < day) {
            day -= days;
            break;
        }
        days -= day;
        if (--month == 0) {
            month = 12;
            --year;
        }
        if (year < 1) {
            throw std::invalid_argument("Date calculation is outside the supported range.");
        }
        day = daysInMonth(year, month);
    }
    return year * 10000 + month * 100 + day;
}

Bank::Bank(const std::string& directory)
    : customerPath_(dataPath(directory, "customers.dat", "clientes.dat")),
      transactionPath_(dataPath(directory, "transactions.dat", "transacciones.dat")) {}

void Bank::registerCustomer(const Customer& customer) {
    const auto name = checkedText(customer.name);
    const auto username = checkedText(customer.username);
    if (name.find_first_not_of(" \t\r\n") == std::string::npos
        || customer.nationalId <= 0 || customer.pin < 0) {
        throw std::invalid_argument("A name, positive national ID, and nonnegative numeric PIN are required.");
    }
    if (username.empty() || username.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != std::string::npos) {
        throw std::invalid_argument("The username must contain only letters, digits, or underscores.");
    }
    auto customers = readRecords<Customer>(customerPath_);
    for (const auto& existing : customers) {
        if (existing.nationalId == customer.nationalId) {
            throw std::invalid_argument("A customer with this national ID already exists.");
        }
        if (checkedText(existing.username) == username) {
            throw std::invalid_argument("This username is already taken.");
        }
    }
    Customer registered = customer;
    registered.balance = initialBalance;
    customers.push_back(registered);
    writeRecords(customerPath_, customers);
}

Customer Bank::authenticate(const std::string& username, int pin) const {
    for (const auto& customer : readRecords<Customer>(customerPath_)) {
        if (checkedText(customer.username) == username && customer.pin == pin) {
            return customer;
        }
    }
    throw std::invalid_argument("Incorrect username or PIN.");
}

Customer Bank::recordTransaction(int nationalId, double amount, int date, bool isDeposit) {
    if (!isValidDate(date)) {
        throw std::invalid_argument("Enter a valid date in YYYYMMDD format (1900-9999).");
    }
    auto customers = readRecords<Customer>(customerPath_);
    auto customer = std::find_if(customers.begin(), customers.end(), [nationalId](const Customer& item) {
        return item.nationalId == nationalId;
    });
    if (customer == customers.end()) {
        throw std::invalid_argument("Customer not found.");
    }

    Transaction transaction{};
    transaction.customer = *customer;
    transaction.amount = amount;
    transaction.date = date;
    transaction.isDeposit = isDeposit;
    if (isDeposit) {
        customer->deposit(amount);
    } else {
        customer->withdraw(amount);
    }

    auto transactions = readRecords<Transaction>(transactionPath_);
    int highestId = 0;
    for (const auto& existing : transactions) {
        highestId = std::max(highestId, existing.id);
    }
    if (highestId == std::numeric_limits<int>::max()) {
        throw std::runtime_error("No transaction IDs are available.");
    }
    transaction.id = highestId + 1;
    transactions.push_back(transaction);
    writeRecords(transactionPath_, transactions);
    try {
        writeRecords(customerPath_, customers);
    } catch (...) {
        transactions.pop_back();
        try {
            writeRecords(transactionPath_, transactions);
        } catch (...) {
            throw std::runtime_error("Balance update and transaction rollback failed. Check both data files before continuing.");
        }
        throw;
    }
    return *customer;
}

bool Bank::deleteTransaction(int id) {
    auto transactions = readRecords<Transaction>(transactionPath_);
    const auto remaining = std::remove_if(transactions.begin(), transactions.end(), [id](const Transaction& item) {
        return item.id == id;
    });
    if (remaining == transactions.end()) {
        return false;
    }
    transactions.erase(remaining, transactions.end());
    writeRecords(transactionPath_, transactions);
    return true;
}

std::vector<Transaction> Bank::customerTransactions(int nationalId) const {
    auto transactions = readRecords<Transaction>(transactionPath_);
    transactions.erase(std::remove_if(transactions.begin(), transactions.end(), [nationalId](const Transaction& item) {
        return !item.belongsTo(nationalId);
    }), transactions.end());
    std::stable_sort(transactions.begin(), transactions.end(), [](const Transaction& left, const Transaction& right) {
        return left.date < right.date;
    });
    return transactions;
}

TransactionSummary Bank::recentSummary(int nationalId, int referenceDate) const {
    const int firstDate = subtractDays(referenceDate, 30);
    TransactionSummary summary{};
    for (const auto& transaction : readRecords<Transaction>(transactionPath_)) {
        if (!transaction.belongsTo(nationalId) || !transaction.isWithin(firstDate, referenceDate)) {
            continue;
        }
        if (transaction.isDeposit) {
            ++summary.depositCount;
            summary.depositTotal += transaction.amount;
        } else {
            ++summary.withdrawalCount;
            summary.withdrawalTotal += transaction.amount;
        }
    }
    return summary;
}

bool Bank::largestTransaction(Transaction& result) const {
    const auto transactions = readRecords<Transaction>(transactionPath_);
    if (transactions.empty()) {
        return false;
    }
    result = *std::max_element(transactions.begin(), transactions.end(), [](const Transaction& left, const Transaction& right) {
        return left.amount < right.amount;
    });
    return true;
}

bool Bank::topDepositor(Depositor& result) const {
    const auto transactions = readRecords<Transaction>(transactionPath_);
    if (transactions.empty()) {
        return false;
    }
    const auto latest = std::max_element(transactions.begin(), transactions.end(), [](const Transaction& left, const Transaction& right) {
        return left.date < right.date;
    });
    const int firstDate = subtractDays(latest->date, 30);
    std::map<int, Depositor> totals;
    bool found = false;
    result = Depositor{};
    for (const auto& transaction : transactions) {
        if (!transaction.isDeposit || !transaction.isWithin(firstDate, latest->date)) {
            continue;
        }
        auto& depositor = totals[transaction.customer.nationalId];
        depositor.customer = transaction.customer;
        depositor.total += transaction.amount;
    }
    for (const auto& entry : totals) {
        if (!found || entry.second.total > result.total) {
            result = entry.second;
            found = true;
        }
    }
    return found;
}

} // namespace banking
