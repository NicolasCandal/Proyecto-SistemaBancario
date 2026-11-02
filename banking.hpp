#ifndef BANKING_HPP
#define BANKING_HPP

#include <string>
#include <vector>

namespace banking {

constexpr double initialBalance = 10000.0;
constexpr unsigned int textCapacity = 50;

// Field order and types preserve the original raw binary record layout.
struct Customer {
    char name[textCapacity]{};
    int nationalId = 0;
    char username[textCapacity]{};
    int pin = 0;
    double balance = initialBalance;

    void deposit(double amount);
    void withdraw(double amount);
};

struct Transaction {
    double amount = 0;
    int date = 0; // YYYYMMDD
    int id = 0;
    Customer customer{}; // Snapshot taken before the transaction.
    bool isDeposit = false;

    bool belongsTo(int nationalId) const;
    bool isWithin(int firstDate, int lastDate) const;
    const char* typeName() const;
};

struct TransactionSummary {
    unsigned int depositCount = 0;
    unsigned int withdrawalCount = 0;
    double depositTotal = 0;
    double withdrawalTotal = 0;
};

struct Depositor {
    Customer customer{};
    double total = 0;
};

bool isValidDate(int date);
int subtractDays(int date, int days);

class Bank {
public:
    explicit Bank(const std::string& directory = ".");

    void registerCustomer(const Customer& customer);
    Customer authenticate(const std::string& username, int pin) const;
    Customer recordTransaction(int nationalId, double amount, int date, bool isDeposit);
    bool deleteTransaction(int id);

    std::vector<Transaction> customerTransactions(int nationalId) const;
    TransactionSummary recentSummary(int nationalId, int referenceDate) const;
    bool largestTransaction(Transaction& result) const;
    bool topDepositor(Depositor& result) const;

private:
    std::string customerPath_;
    std::string transactionPath_;
};

} // namespace banking

#endif
